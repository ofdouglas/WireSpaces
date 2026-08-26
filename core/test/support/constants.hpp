/**
 * @file constants.hpp
 * @brief Shared literals for core unit and integration tests.
 */

#pragma once

#include <cstdint>

namespace wirespaces::test::support {

constexpr uint8_t kLocalHostId = 0x01U;
constexpr uint8_t kRemoteHostId = 0x02U;
constexpr uint8_t kOtherWire = 0x05U;

constexpr uint16_t kSenderEndpoint = 0x0001U;
constexpr uint16_t kReceiverEndpoint = 0x0002U;
constexpr uint16_t kUnknownEndpoint = 0x0099U;

constexpr uint16_t kMinEndpointId = 0x0000U;
constexpr uint16_t kMaxEndpointId = 0x3FFFU;

} // namespace wirespaces::test::support
