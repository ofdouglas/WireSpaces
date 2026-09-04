/**
 * @file wiring_constants.h
 * @brief Shared WireSpaces identities and UART routing for Arduino UNO applications.
 */

#pragma once

#include <wirespaces/runtime/core.hpp>
#include <wirespaces/services/led_control/led_control.h>
#include <wirespaces/services/ping/ping.h>

#include <cstdint>

namespace wiring_constants {

constexpr wirespaces::WireNumber kTestWire{1U};
constexpr wirespaces::HostId kArduinoHost{1U};
constexpr wirespaces::HostId kPcHost{2U};
constexpr std::uint32_t kUartBaudRate{115200UL};
constexpr wirespaces::EgressSet kUartEgress{1U};

constexpr wirespaces::RouteTableEntry kUartRoute{kTestWire, kUartEgress};
constexpr wirespaces::HostInfo kArduinoHostInfo{kArduinoHost, 1U, {kTestWire}};

constexpr wirespaces::EndpointAddress kPingEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kCommon, WS_SERVICE_PING_ENDPOINT_ID)};

constexpr wirespaces::EndpointAddress kLedControlEndpoint{wirespaces::EndpointAddress::from(
    wirespaces::Namespace::kCommon, WS_SERVICE_LED_CONTROL_ENDPOINT_ID)};

constexpr wirespaces::EndpointAddress kBitsUploadEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kUser0, 1U)};
constexpr wirespaces::EndpointAddress kBitsEchoEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kUser0, 2U)};

}  // namespace wiring_constants
