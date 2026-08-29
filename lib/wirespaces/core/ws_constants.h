/**
 * @file ws_constants.h
 * @brief WireSpaces compile-time constants and layout attributes.
 */

#pragma once

#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
#define WS_PACKED __attribute__((packed))
#else
#define WS_PACKED
#endif

namespace wirespaces {

constexpr uint8_t kLocalWireValue{0x00U};
constexpr uint8_t kBroadcastHostValue{0xFFU};
constexpr uint16_t kDefaultEndpointStorageCapacity{64U};

}  // namespace wirespaces
