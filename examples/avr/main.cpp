#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "WireSpaces/cpp/core/wirespaces_core.hpp"
#include "WireSpaces/cpp/hal/clock.h"
#include "WireSpaces/cpp/services/heartbeat.h"
#include "WireSpaces/cpp/services/led_control.h"
#include "WireSpaces/cpp/services/ping.h"
#include "WireSpaces/cpp/services/stack_report.h"
#include "hdlc_decoder.hpp"
#include "hdlc_encoder.hpp"
#include "stack_monitor.hpp"

namespace {

constexpr std::uint32_t kBaudRate{115200UL};
constexpr std::uint16_t kBaudDivider{
    static_cast<std::uint16_t>((F_CPU / (8UL * kBaudRate)) - 1UL)};
constexpr std::uint8_t kHeartbeatWire{1U};
constexpr std::uint8_t kArduinoParticipant{1U};
constexpr std::uint8_t kUartEgress{1U};
constexpr std::uint16_t kStackScanIterationPeriod{4096U};
constexpr std::size_t kMaximumPayloadSize{4U};
constexpr std::size_t kMaximumCanonicalSize{
    sizeof(wirespaces::Header) + kMaximumPayloadSize};
constexpr std::size_t kMaximumFrameCapacity{kMaximumCanonicalSize * 2U + 2U};
constexpr std::uint16_t kPingCanonicalEndpoint{
    static_cast<std::uint16_t>(0xC000U | WS_SERVICE_PING_ENDPOINT_ID)};
constexpr std::uint16_t kLedControlCanonicalEndpoint{
    static_cast<std::uint16_t>(0xC000U | WS_SERVICE_LED_CONTROL_ENDPOINT_ID)};

WS_PACKET_DEFINE(UartReceivePacket, kMaximumPayloadSize);

volatile std::uint32_t g_milliseconds{0U};
wirespaces::avr_example::HdlcDecoder<kMaximumCanonicalSize> g_hdlc_decoder{};

void uartInit() {
    UBRR0H = static_cast<std::uint8_t>(kBaudDivider >> 8U);
    UBRR0L = static_cast<std::uint8_t>(kBaudDivider);
    UCSR0A = _BV(U2X0);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

void uartWriteByte(std::uint8_t byte) {
    while ((UCSR0A & _BV(UDRE0)) == 0U) {
    }

    UDR0 = byte;
}

void clockInit() {
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01) | _BV(CS00);
    OCR0A = 249U;
    TIMSK0 = _BV(OCIE0A);
}

void ledPwmInit() {
    DDRB |= _BV(DDB5);
    PORTB &= static_cast<std::uint8_t>(~_BV(PORTB5));
    TCCR1A = _BV(WGM10);
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
    OCR1A = 0U;
    TIMSK1 = 0U;
}

/**
 * @brief Apply 8-bit ratiometric brightness to the UNO LED on D13/PB5.
 *
 * Zero and 255 are driven statically to guarantee exact 0% and 100%
 * endpoints. PB5 is not a hardware PWM output, so Timer 1 compare and
 * overflow interrupts generate approximately 977 Hz PWM in software.
 */
void setBuiltInLedBrightness(void* context, std::uint8_t brightness) {
    (void)context;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TIMSK1 = 0U;
        if (brightness == 0U) {
            PORTB &= static_cast<std::uint8_t>(~_BV(PORTB5));
        } else if (brightness == 0xFFU) {
            PORTB |= _BV(PORTB5);
        } else {
            OCR1A = brightness;
            TCNT1 = 0U;
            PORTB |= _BV(PORTB5);
            TIFR1 = _BV(OCF1A) | _BV(TOV1);
            TIMSK1 = _BV(OCIE1A) | _BV(TOIE1);
        }
    }
}

/**
 * @brief Project a canonical WireSpaces PDU onto the UART HDLC Link.
 *
 * The local packet capacity fields are not transmitted. This first-stage Link
 * sends the six-byte canonical header followed by the active payload.
 */
void forwardToUart(
    void* forwarder_context,
    const ws_packet_buffer_t* packet,
    ws_egress_set_t egress_set) {
    (void)forwarder_context;
    (void)egress_set;

    if ((packet == nullptr) ||
        (packet->size > kMaximumPayloadSize)) {
        return;
    }

    const std::size_t canonical_size{sizeof(packet->header) + packet->size};
    const auto* canonical_bytes{
        reinterpret_cast<const std::uint8_t*>(&packet->header)};

    std::uint8_t frame_storage[kMaximumFrameCapacity]{};
    const std::size_t frame_size{
        wirespaces::avr_example::HdlcEncoder::encode(
            canonical_bytes,
            canonical_size,
            frame_storage,
            sizeof(frame_storage))};
    if (frame_size == 0U) {
        return;
    }

    for (std::size_t index{0U}; index < frame_size; ++index) {
        uartWriteByte(frame_storage[index]);
    }
}

/**
 * @brief Dispatch all complete canonical PDUs currently available from UART.
 *
 * Invalid and oversized frames are discarded. Reception is polled so the
 * example does not allocate an interrupt-side packet queue.
 */
void processUartInput(const wirespaces::DispatchTable& dispatch_table) {
    while ((UCSR0A & _BV(RXC0)) != 0U) {
        const std::uint8_t byte{UDR0};
        if (!g_hdlc_decoder.push(byte)) {
            continue;
        }

        const std::size_t frame_size{g_hdlc_decoder.frameSize()};
        if ((frame_size >= sizeof(wirespaces::Header)) &&
            (frame_size <= kMaximumCanonicalSize)) {
            UartReceivePacket packet{};
            packet.capacity = kMaximumPayloadSize;
            packet.size = static_cast<std::uint16_t>(
                frame_size - sizeof(wirespaces::Header));
            std::memcpy(
                &packet.header,
                g_hdlc_decoder.frameData(),
                sizeof(packet.header));
            std::memcpy(
                packet.data,
                g_hdlc_decoder.frameData() + sizeof(packet.header),
                packet.size);
            (void)ws_dispatch_packet(
                &dispatch_table,
                reinterpret_cast<const wirespaces::PacketBuffer*>(&packet));
        }
        g_hdlc_decoder.consume();
    }
}

} // namespace

ISR(TIMER0_COMPA_vect) {
    ++g_milliseconds;
}

ISR(TIMER1_COMPA_vect) {
    PORTB &= static_cast<std::uint8_t>(~_BV(PORTB5));
}

ISR(TIMER1_OVF_vect) {
    PORTB |= _BV(PORTB5);
}

wirespaces::hal::MillisecondClock::TimePoint
wirespaces::hal::MillisecondClock::now() noexcept {
    std::uint32_t result{0U};
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        result = g_milliseconds;
    }
    return result;
}

int main() {
    wirespaces::avr_example::StackMonitor stack_monitor{};
    stack_monitor.initialize();

    uartInit();
    clockInit();
    ledPwmInit();

    wirespaces::RouteTableEntry route_entries[]{
        {kHeartbeatWire, kUartEgress},
    };
    wirespaces::RouteTable route_table{
        route_entries,
        1U,
        forwardToUart,
        nullptr,
    };
    const wirespaces::HostInfo host_info{
        kArduinoParticipant,
        1U,
        {kHeartbeatWire},
    };
    ws_host_set_info(&host_info);

    heartbeat::HeartbeatService<1000U> heartbeat_service{
        &route_table,
        kHeartbeatWire,
        kArduinoParticipant,
        WS_HOST_BROADCAST,
    };
    stack_report::StackReportService<1000U> stack_report_service{
        &route_table,
        kHeartbeatWire,
        kArduinoParticipant,
        WS_HOST_BROADCAST,
    };
    ping::PingService ping_service{&route_table};
    led_control::LedControlService led_control_service{
        &route_table,
        setBuiltInLedBrightness,
        nullptr,
    };
    wirespaces::DispatchTableEntry dispatch_entries[]{
        {
            kPingCanonicalEndpoint,
            ping_service.receiverHandle(),
        },
        {
            kLedControlCanonicalEndpoint,
            led_control_service.receiverHandle(),
        },
    };
    const wirespaces::DispatchTable dispatch_table{
        dispatch_entries,
        2U,
    };

    sei();
    std::uint16_t loop_iteration{0U};
    for (;;) {
        processUartInput(dispatch_table);
        heartbeat_service.run();
        stack_report_service.run(
            stack_monitor.peakUsedBytes(),
            stack_monitor.capacityBytes());

        loop_iteration = static_cast<std::uint16_t>(loop_iteration + 1U);
        if ((loop_iteration % kStackScanIterationPeriod) == 0U) {
            stack_monitor.scan();
        }
    }
}
