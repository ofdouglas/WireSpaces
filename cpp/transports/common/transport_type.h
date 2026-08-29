/*
 * @file  transport_type.h
 * @brief WireSpaces Transport Type
 */

#pragma once

#include <cstdint>

enum class TransportType : std::uint8_t {
    Simple = 0,
    ReliableSegmented = 1,
    NumTransportTypes,
};
