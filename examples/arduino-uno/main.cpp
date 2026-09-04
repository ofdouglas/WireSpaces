#include <avr/interrupt.h>
#include <platform/avr/init.h>
#include <platform/avr/uart0.h>
#include <platform/avr/stack_monitor.h>
#include <wirespaces/links/uart_hdlc/decoder.h>
#include <wirespaces/links/uart_hdlc/encoder.h>
#include <wirespaces/services/heartbeat/heartbeat.h>
#include <wirespaces/services/led_control/led_control.h>
#include <wirespaces/services/ping/ping.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <wirespaces/runtime/core.hpp>

namespace {

// TODO: these should be codegen eventually
constexpr wirespaces::WireNumber kHeartbeatWire{1U};
constexpr wirespaces::HostId kArduinoHost{1U};
constexpr std::uint8_t kUartEgress{1U};

constexpr std::size_t kMaximumPayloadSize{4U};
constexpr std::size_t kMaximumCanonicalSize{sizeof(wirespaces::Header) + kMaximumPayloadSize};
constexpr std::size_t kHdlcCrcSize{2U};
constexpr std::size_t kMaximumFrameCapacity{(kMaximumCanonicalSize + kHdlcCrcSize) * 2U + 2U};

// TODO: these should be codegen eventually
constexpr wirespaces::EndpointAddress kPingEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kCommon, WS_SERVICE_PING_ENDPOINT_ID)};

constexpr wirespaces::EndpointAddress kLedControlEndpoint{wirespaces::EndpointAddress::from(
    wirespaces::Namespace::kCommon, WS_SERVICE_LED_CONTROL_ENDPOINT_ID)};

const wirespaces::RouteTableEntry route_entries[]{
    {kHeartbeatWire, kUartEgress},
};
const wirespaces::HostInfo kHostInfo{kArduinoHost, 1U, {kHeartbeatWire}};


WS_PACKET_BUFFER_DEFINE(UartReceivePacket, kMaximumPayloadSize);

wirespaces::links::uart_hdlc::HdlcDecoder<kMaximumCanonicalSize> g_hdlc_decoder{};


/**
 * @brief Send a packet via the UART HDLC Link.
 */
class UartForwarder final : public wirespaces::PacketForwarder {
public:
    void forward(const wirespaces::PacketBuffer& packet,
                 wirespaces::EgressSet egress_set) noexcept override {
        (void)egress_set;
        if (packet.size() > kMaximumPayloadSize) {
            return;
        }

        std::uint8_t frame_storage[kMaximumFrameCapacity]{};
        const std::size_t frame_size{wirespaces::links::uart_hdlc::HdlcEncoder::encode(
            packet.headerAndPayload(),
            frame_storage)};
        if (frame_size == 0U) {
            return;
        }

        wirespaces::platform::avr::uart0WriteSpan(
            wirespaces::foundation::Span<const uint8_t>{frame_storage, frame_size});
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
    while (wirespaces::platform::avr::uart0ByteAvailable()) {
        const std::uint8_t byte{wirespaces::platform::avr::uart0ReadByte()};
        if (!g_hdlc_decoder.push(byte)) {
            continue;
        }

        const auto decoded_frame{g_hdlc_decoder.frame()};
        const std::size_t frame_size{decoded_frame.size()};

        if ((frame_size >= sizeof(wirespaces::Header)) && (frame_size <= kMaximumCanonicalSize)) {
            UartReceivePacket packet{};
            static_cast<void>(packet.resize(
                static_cast<uint16_t>(frame_size - sizeof(wirespaces::Header))));
            std::memcpy(&packet.header(), decoded_frame.data(), frame_size);
            static_cast<void>(dispatcher.dispatch(packet));
        }
        g_hdlc_decoder.consume();
    }
}

}  // namespace


int main() {
    wirespaces::platform::avr::initialize(115200UL);

    UartForwarder uart_forwarder{};
    wirespaces::Router router{
        wirespaces::foundation::Span<const wirespaces::RouteTableEntry>{route_entries},
        uart_forwarder,
    };

    wirespaces::setLocalHostInfo(kHostInfo);

    heartbeat::HeartbeatService<1000U> heartbeat_service{
        &router,
        kHeartbeatWire,
        kArduinoHost,
        wirespaces::HostId{wirespaces::kBroadcastHostValue},
    };
    ping::PingService ping_service{&router};
    led_control::LedControlService led_control_service{
        &router,
        wirespaces::platform::avr::setBuiltinLedBrightness,
        nullptr,
    };

    // TODO: this should be codegen eventually
    const wirespaces::DispatchTableEntry dispatch_entries[]{
        {kArduinoHost, kPingEndpoint, &ping_service.receiver()},
        {kArduinoHost, kLedControlEndpoint, &led_control_service.receiver()},
    };
    const wirespaces::Dispatcher dispatcher{
        wirespaces::foundation::Span<const wirespaces::DispatchTableEntry>{dispatch_entries},
    };

    sei();
    for (;;) {
        processUartInput(dispatcher);
        ping_service.run();
        led_control_service.run();
        heartbeat_service.run();
    }
}
