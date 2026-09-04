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

}  // namespace


using wirespaces::examples::arduino_uno::UartHdlcForwarder;
using wirespaces::examples::arduino_uno::UartHdlcReceiver;
using wirespaces::foundation::Span;

int main() {
    wirespaces::platform::avr::initialize(wiring_constants::kUartBaudRate);

    UartHdlcForwarder<kMaximumPayloadSize> uart_forwarder{};
    UartHdlcReceiver<UartReceivePacket> uart_receiver{};
    wirespaces::Router router{
        Span<const wirespaces::RouteTableEntry>{
            &wiring_constants::kUartRoute, 1U}, uart_forwarder};

    wirespaces::setLocalHostInfo(wiring_constants::kArduinoHostInfo);

    heartbeat::HeartbeatService<1000U> heartbeat_service{
        &router,
        wiring_constants::kTestWire,
        wiring_constants::kArduinoHost,
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
        {wiring_constants::kArduinoHost, wiring_constants::kPingEndpoint,
         &ping_service.receiver()},
        {wiring_constants::kArduinoHost, wiring_constants::kLedControlEndpoint,
         &led_control_service.receiver()},
    };
    const wirespaces::Dispatcher dispatcher{
        wirespaces::foundation::Span<const wirespaces::DispatchTableEntry>{dispatch_entries},
    };

    sei();
    for (;;) {
        uart_receiver.process(dispatcher);
        ping_service.run();
        led_control_service.run();
        heartbeat_service.run();
    }
}
