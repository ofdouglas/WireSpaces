#include <avr/interrupt.h>
#include <platform/avr/init.h>
#include <wirespaces/services/heartbeat/heartbeat.h>
#include <wirespaces/services/led_control/led_control.h>
#include <wirespaces/services/ping/ping.h>

#include <cstddef>
#include <wirespaces/runtime/core.hpp>

#include "uart_hdlc_link.h"
#include "wiring_constants.h"

namespace {

constexpr std::size_t kMaximumPayloadSize{4U};

WS_PACKET_BUFFER_DEFINE(UartReceivePacket, kMaximumPayloadSize);

using wirespaces::examples::arduino_uno::UartHdlcForwarder;
using wirespaces::examples::arduino_uno::UartHdlcReceiver;
using wirespaces::foundation::Span;

/**
 * @brief Own and poll the complete Arduino UNO demonstration application.
 *
 * Member declaration order records the lifetime dependencies between the Link,
 * Router, Services, endpoint bindings, and Dispatcher.
 */
class DemoApplication final {
public:
    DemoApplication() noexcept = default;
    DemoApplication(const DemoApplication&) = delete;
    DemoApplication(DemoApplication&&) = delete;
    DemoApplication& operator=(const DemoApplication&) = delete;
    DemoApplication& operator=(DemoApplication&&) = delete;

    /** @brief Initialize target hardware and process-wide host identity. */
    void initialize() noexcept;

    /** @brief Poll ingress and run each Service once. */
    void runOnce() noexcept;

private:
    // Links
    UartHdlcForwarder<kMaximumPayloadSize> uart_forwarder_{};
    UartHdlcReceiver<UartReceivePacket> uart_receiver_{};

    // Routes
    demo_wiring::Forwarder egress_forwarder_{uart_forwarder_};
    wirespaces::Router router_{demo_wiring::routes(), egress_forwarder_};

    // Services
    heartbeat::HeartbeatService<1000U> heartbeat_service_{
        &router_, wiring_constants::kTestWire, wiring_constants::kArduinoHost,
        wirespaces::HostId{wirespaces::kBroadcastHostValue}};

    ping::PingService ping_service_{&router_};
    
    led_control::LedControlService led_control_service_{
        &router_, wirespaces::platform::avr::setBuiltinLedBrightness, nullptr};

    // Dispatch Table
    // TODO: this should be codegen eventually
    wirespaces::DispatchTableEntry dispatch_entries_[2U]{
        {wiring_constants::kPingEndpoint, &ping_service_.receiver()},
        {wiring_constants::kLedControlEndpoint, &led_control_service_.receiver()}};
    wirespaces::Dispatcher dispatcher_{Span<const wirespaces::DispatchTableEntry>{dispatch_entries_}};
};

// --- DemoApplication implementations ---

void DemoApplication::initialize() noexcept {
    wirespaces::platform::avr::initialize(wiring_constants::kUartBaudRate);
    wirespaces::setLocalHostInfo(wiring_constants::kArduinoHostInfo);
}

void DemoApplication::runOnce() noexcept {
    uart_receiver_.process(router_, dispatcher_, demo_wiring::kUartIngressIndex);
    ping_service_.run();
    led_control_service_.run();
    heartbeat_service_.run();
}

}  // namespace

int main() {
    static DemoApplication application{};
    application.initialize();
    sei();
    for (;;) {
        application.runOnce();
    }
}
