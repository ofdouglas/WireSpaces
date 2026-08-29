#include <avr/interrupt.h>
#include <platform/avr/builtin_led.h>
#include <platform/avr/millisecond_clock.h>
#include <platform/avr/stack_monitor.h>
#include <platform/avr/uart0.h>
#include <wirespaces/links/uart_hdlc/decoder.h>
#include <wirespaces/links/uart_hdlc/encoder.h>
#include <wirespaces/services/heartbeat/heartbeat.h>
#include <wirespaces/services/led_control/led_control.h>
#include <wirespaces/services/ping/ping.h>
#include <wirespaces/services/stack_report/stack_report.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <wirespaces/runtime/core.hpp>

namespace {

constexpr std::uint32_t kBaudRate{115200UL};
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

wirespaces::links::uart_hdlc::HdlcDecoder<kMaximumCanonicalSize> g_hdlc_decoder{};

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
            wirespaces::platform::avr::uart0WriteByte(frame_storage[index]);
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
    while (wirespaces::platform::avr::uart0ByteAvailable()) {
        const std::uint8_t byte{wirespaces::platform::avr::uart0ReadByte()};
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

int main() {
    wirespaces::platform::avr::StackMonitor stack_monitor{};
    stack_monitor.initialize();

    wirespaces::platform::avr::uart0Init(kBaudRate);
    wirespaces::platform::avr::millisecondClockInit();
    wirespaces::platform::avr::builtinLedInit();

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
        wirespaces::platform::avr::setBuiltinLedBrightness,
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
