/**
 * @file wiring_constants.h
 * @brief Shared WireSpaces identities and UART routing for Arduino UNO applications.
 */

#pragma once

#include <wirespaces/runtime/core.hpp>
#include <wirespaces/services/led_control/led_control.h>
#include <wirespaces/services/ping/ping.h>

#include <cstdint>

#include "demo_wiring.h"

namespace wiring_constants {

using demo_wiring::kTestWire;
using demo_wiring::kArduinoHost;
using demo_wiring::kPcHost;
using demo_wiring::kUartBaudRate;
using demo_wiring::kArduinoHostInfo;

constexpr wirespaces::PublicationConfig kDiagnosticsPublication{
    kTestWire, wirespaces::HostId{wirespaces::kBroadcastHostValue}};

// Endpoint assignments remain application-owned; deployment wiring is generated above.
constexpr wirespaces::EndpointAddress kPingEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kCommon, WS_SERVICE_PING_ENDPOINT_ID)};

constexpr wirespaces::EndpointAddress kLedControlEndpoint{wirespaces::EndpointAddress::from(
    wirespaces::Namespace::kCommon, WS_SERVICE_LED_CONTROL_ENDPOINT_ID)};

constexpr wirespaces::EndpointAddress kBitsUploadEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kUser0, 1U)};
constexpr wirespaces::EndpointAddress kBitsEchoEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kUser0, 2U)};

// Application-owned connections; topology codegen does not select service peers/endpoints.
constexpr wirespaces::ConnectionAddress kUploadConnection{
    kTestWire, kArduinoHost, kPcHost, kBitsUploadEndpoint};
constexpr wirespaces::ConnectionAddress kEchoConnection{
    kTestWire, kArduinoHost, kPcHost, kBitsEchoEndpoint};

}  // namespace wiring_constants
