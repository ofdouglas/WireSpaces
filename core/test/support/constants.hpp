/**
 * @file constants.hpp
 * @brief Shared strongly typed constants for core tests.
 */

#pragma once

#include <runtime/core.hpp>

namespace wirespaces::test::support {

constexpr HostId kLocalHostId{0x01U};
constexpr HostId kRemoteHostId{0x02U};
constexpr WireNumber kLocalWire{kLocalWireValue};
constexpr WireNumber kOtherWire{0x05U};

constexpr uint16_t kSenderEndpointId{0x0001U};
constexpr uint16_t kReceiverEndpointId{0x0002U};
constexpr uint16_t kUnknownEndpointId{0x0099U};

constexpr EndpointAddress kSenderEndpoint{
    EndpointAddress::from(Namespace::kUser0, kSenderEndpointId)};
constexpr EndpointAddress kReceiverEndpoint{
    EndpointAddress::from(Namespace::kUser0, kReceiverEndpointId)};
constexpr EndpointAddress kUnknownEndpoint{
    EndpointAddress::from(Namespace::kUser0, kUnknownEndpointId)};

} // namespace wirespaces::test::support
