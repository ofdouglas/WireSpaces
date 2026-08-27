#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include <cstddef>
#include <cstdint>

#include "WireSpaces/cpp/core/wirespaces_core.hpp"
#include "WireSpaces/cpp/hal/clock.h"
#include "WireSpaces/cpp/services/heartbeat.h"
#include "hdlc_encoder.hpp"

namespace {

constexpr std::uint32_t kBaudRate{115200UL};
constexpr std::uint16_t kBaudDivider{
    static_cast<std::uint16_t>((F_CPU / (8UL * kBaudRate)) - 1UL)};
constexpr std::uint8_t kHeartbeatWire{1U};
constexpr std::uint8_t kArduinoParticipant{1U};
constexpr std::uint8_t kUartEgress{1U};
constexpr std::size_t kHeartbeatCanonicalSize{
    sizeof(wirespaces::Header) + sizeof(heartbeat::HeartbeatMessage)};
constexpr std::size_t kHeartbeatFrameCapacity{kHeartbeatCanonicalSize * 2U + 2U};

volatile std::uint32_t g_milliseconds{0U};

void uartInit() {
    UBRR0H = static_cast<std::uint8_t>(kBaudDivider >> 8U);
    UBRR0L = static_cast<std::uint8_t>(kBaudDivider);
    UCSR0A = _BV(U2X0);
    UCSR0B = _BV(TXEN0);
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
        (packet->size > sizeof(heartbeat::HeartbeatMessage))) {
        return;
    }

    const std::size_t canonical_size{sizeof(packet->header) + packet->size};
    const auto* canonical_bytes{
        reinterpret_cast<const std::uint8_t*>(&packet->header)};

    std::uint8_t frame_storage[kHeartbeatFrameCapacity]{};
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

} // namespace

ISR(TIMER0_COMPA_vect) {
    ++g_milliseconds;
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
    uartInit();
    clockInit();

    wirespaces::RouteTableEntry route_entries[]{
        {kHeartbeatWire, kUartEgress},
    };
    wirespaces::RouteTable route_table{
        route_entries,
        1U,
        forwardToUart,
        nullptr,
    };
    heartbeat::HeartbeatService<1000U> heartbeat_service{
        &route_table,
        kHeartbeatWire,
        kArduinoParticipant,
        WS_HOST_BROADCAST,
    };

    sei();
    for (;;) {
        heartbeat_service.run();
    }
}
