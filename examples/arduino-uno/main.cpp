#include <avr/interrupt.h>
#include <avr/io.h>
#include <hal/clock.h>
#include <links/uart_hdlc/decoder.h>
#include <links/uart_hdlc/encoder.h>
#include <platform/avr/stack_monitor.h>
#include <services/heartbeat/heartbeat.h>
#include <services/led_control/led_control.h>
#include <services/ping/ping.h>
#include <services/stack_report/stack_report.h>
#include <util/atomic.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <runtime/core.hpp>

namespace {

constexpr std::uint32_t kBaudRate{115200UL};
constexpr std::uint16_t kBaudDivider{static_cast<std::uint16_t>((F_CPU / (8UL * kBaudRate)) - 1UL)};
constexpr wirespaces::WireNumber kHeartbeatWire{1U};
constexpr wirespaces::HostId kArduinoHost{1U};
constexpr std::uint8_t kUartEgress{1U};
constexpr std::uint16_t kStackScanIterationPeriod{4096U};
constexpr std::size_t kMaximumPayloadSize{4U};
constexpr std::size_t kMaximumCanonicalSize{sizeof(wirespaces::Header) + kMaximumPayloadSize};
constexpr std::size_t kHdlcCrcSize{2U};
constexpr std::size_t kMaximumFrameCapacity{(kMaximumCanonicalSize + kHdlcCrcSize) * 2U + 2U};
constexpr wirespaces::EndpointAddress kPingEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kCommon, WS_SERVICE_PING_ENDPOINT_ID)};
constexpr wirespaces::EndpointAddress kLedControlEndpoint{wirespaces::EndpointAddress::from(
    wirespaces::Namespace::kCommon, WS_SERVICE_LED_CONTROL_ENDPOINT_ID)};

WS_PACKET_BUFFER_DEFINE(UartReceivePacket, kMaximumPayloadSize);

volatile std::uint32_t g_milliseconds{0U};
wirespaces::links::uart_hdlc::HdlcDecoder<kMaximumCanonicalSize> g_hdlc_decoder{};

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
class UartForwarder final : public wirespaces::PacketForwarder {
public:
    void forward(const wirespaces::PacketBuffer& packet,
                 wirespaces::EgressSet egress_set) noexcept override {
        (void)egress_set;
        if (packet.size() > kMaximumPayloadSize) {
            return;
        }

        const std::size_t canonical_size{sizeof(packet.header()) + packet.size()};
        const auto* canonical_bytes{reinterpret_cast<const std::uint8_t*>(&packet.header())};

        std::uint8_t frame_storage[kMaximumFrameCapacity]{};
        const std::size_t frame_size{wirespaces::links::uart_hdlc::HdlcEncoder::encode(
            wirespaces::foundation::Span<const std::uint8_t>{canonical_bytes, canonical_size},
            frame_storage)};
        if (frame_size == 0U) {
            return;
        }

        for (std::size_t index{0U}; index < frame_size; ++index) {
            uartWriteByte(frame_storage[index]);
        }
    }
};

/**
 * @brief Dispatch all complete canonical PDUs currently available from UART.
 *
 * Frames with invalid CRCs and invalid or oversized payloads are discarded.
 * Reception is polled so the example does not allocate an interrupt-side
 * packet queue.
 */
void processUartInput(const wirespaces::Dispatcher& dispatcher) {
    while ((UCSR0A & _BV(RXC0)) != 0U) {
        const std::uint8_t byte{UDR0};
        if (!g_hdlc_decoder.push(byte)) {
            continue;
        }

        const auto decoded_frame{g_hdlc_decoder.frame()};
        const std::size_t frame_size{decoded_frame.size()};
        if ((frame_size >= sizeof(wirespaces::Header)) && (frame_size <= kMaximumCanonicalSize)) {
            UartReceivePacket packet{};
            const auto payload_size{
                static_cast<std::uint16_t>(frame_size - sizeof(wirespaces::Header))};
            static_cast<void>(packet.resize(payload_size));
            std::memcpy(&packet.header(), decoded_frame.data(), sizeof(packet.header()));
            std::memcpy(packet.payload().data(), decoded_frame.data() + sizeof(packet.header()),
                        packet.size());
            static_cast<void>(dispatcher.dispatch(packet));
        }
        g_hdlc_decoder.consume();
    }
}

}  // namespace

ISR(TIMER0_COMPA_vect) {
    ++g_milliseconds;
}

ISR(TIMER1_COMPA_vect) {
    PORTB &= static_cast<std::uint8_t>(~_BV(PORTB5));
}

ISR(TIMER1_OVF_vect) {
    PORTB |= _BV(PORTB5);
}

wirespaces::hal::MillisecondClock::TimePoint wirespaces::hal::MillisecondClock::now() noexcept {
    std::uint32_t result{0U};
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        result = g_milliseconds;
    }
    return result;
}

int main() {
    wirespaces::platform::avr::StackMonitor stack_monitor{};
    stack_monitor.initialize();

    uartInit();
    clockInit();
    ledPwmInit();

    const wirespaces::RouteTableEntry route_entries[]{
        {kHeartbeatWire, kUartEgress},
    };
    UartForwarder uart_forwarder{};
    wirespaces::Router router{
        wirespaces::foundation::Span<const wirespaces::RouteTableEntry>{route_entries},
        uart_forwarder,
    };
    const wirespaces::HostInfo host_info{kArduinoHost, 1U, {kHeartbeatWire}};
    wirespaces::setLocalHostInfo(host_info);

    heartbeat::HeartbeatService<1000U> heartbeat_service{
        &router,
        kHeartbeatWire,
        kArduinoHost,
        wirespaces::HostId{wirespaces::kBroadcastHostValue},
    };
    stack_report::StackReportService<1000U> stack_report_service{
        &router,
        kHeartbeatWire,
        kArduinoHost,
        wirespaces::HostId{wirespaces::kBroadcastHostValue},
    };
    ping::PingService ping_service{&router};
    led_control::LedControlService led_control_service{
        &router,
        setBuiltInLedBrightness,
        nullptr,
    };
    const wirespaces::DispatchTableEntry dispatch_entries[]{
        {kArduinoHost, kPingEndpoint, &ping_service},
        {kArduinoHost, kLedControlEndpoint, &led_control_service},
    };
    const wirespaces::Dispatcher dispatcher{
        wirespaces::foundation::Span<const wirespaces::DispatchTableEntry>{dispatch_entries},
    };

    sei();
    std::uint16_t loop_iteration{0U};
    for (;;) {
        processUartInput(dispatcher);
        heartbeat_service.run();
        stack_report_service.run(stack_monitor.peakUsedBytes(), stack_monitor.capacityBytes());

        loop_iteration = static_cast<std::uint16_t>(loop_iteration + 1U);
        if ((loop_iteration % kStackScanIterationPeriod) == 0U) {
            stack_monitor.scan();
        }
    }
}
