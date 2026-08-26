/*
 * @file  crc_algorithm.h
 * @brief Defines the parameters of various CRC algorithms.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "crc/crc.h"
#include "util/span.h"

/*
 * @brief Definitions of specific CRC algorithms.
 *
 * @todo Add an implementation-selection feature (not necessarily in this file)
 * @todo Add more algorithms
 */
namespace crc::algorithm {

/******************************************************************************
 *  CRC-8 Algorithms
 ******************************************************************************/

struct SaeJ1850 : details::SpecImpl<SaeJ1850, uint8_t, 0x1D, UINT8_MAX, UINT8_MAX, false, false> {
    static constexpr const char* name() { return "SaeJ1850"; }
};

struct AutosarCrc8 : details::SpecImpl<AutosarCrc8, uint8_t, 0x2F, UINT8_MAX, UINT8_MAX, false, false> {
    static constexpr const char* name() { return "AutosarCrc8"; }
};


/******************************************************************************
 *  CRC-16 Algorithms
 ******************************************************************************/

struct Crc16CcittFalse : details::SpecImpl<Crc16CcittFalse, uint16_t, 0x1021, UINT16_MAX, 0U, false, false> {
    static constexpr const char* name() { return "Crc16CcittFalse"; }
};


/******************************************************************************
 *  CRC-32 Algorithms
 ******************************************************************************/

 struct Crc32Mpeg2 : details::SpecImpl<Crc32Mpeg2, uint32_t, 0x04C11DB7, UINT32_MAX, 0U, false, false> {
    static constexpr const char* name() { return "Crc32Mpeg2"; }
};

// TODO: Ethernet CRC-32 once reflection is implemented
// struct Crc32Ethernet : details::SpecImpl<Crc32Ethernet, uint32_t, 0x04C11DB7, UINT32_MAX, UINT32_MAX, true, true> {
//     static constexpr const char* name() { return "Crc32Ethernet"; }
// };

} // namespace crc::algorithm
