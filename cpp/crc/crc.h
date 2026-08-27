/*
 * @file  crc.h
 * @brief Implements the CRC calculation algorithm.
*/

#pragma once

#include <cstdint>
#include <type_traits>
#include <limits>

#include "util/span.h"
#include "util/static_string.h"

// TODO: should this be in ::details?
namespace crc::details {

// CRC algorithm name string
// TODO: detect truncation / name overflow
static constexpr size_t kMaxNameLength = 16U;
using NameString = util::StaticString<kMaxNameLength>;

/**
 * @brief Calculate the CRC of a given input data using the bitwise software algorithm.
 *
 * @tparam    CrcAlgorithm Class type defining a specific CRC algorithm
 * @param[in] input        The input data to calculate the CRC of.
 * @return    The CRC of the input data.
 *
 * @todo Handle reflect_in and reflect_out
 * @todo Support incremental processing (update, ... finalize)
 */
 template <typename CrcAlgorithm>
 typename CrcAlgorithm::value_type crcBitwise(util::Span<const uint8_t> input) {
    static_assert(!(CrcAlgorithm::reflect_in || CrcAlgorithm::reflect_out), "Reflection not implemented yet");

    using T = typename CrcAlgorithm::value_type;
    constexpr T kMsbBit = static_cast<T>((std::numeric_limits<T>::max() >> 1U) + 1U);
    // MSB-first byte-at-a-time: each new byte enters the top 8 bits of the W-bit register.
    constexpr size_t kDataShift = 8U * (sizeof(T) - 1U);

    T result = CrcAlgorithm::initial;
    for (auto x : input) {
        result ^= static_cast<T>(x) << kDataShift;
        for (int i = 0; i < 8; i++) {
            if (result & kMsbBit) {
                result = (result << 1U) ^ CrcAlgorithm::polynomial;
            } else {
                result <<= 1U;
            }
        }
    }

    return result ^ CrcAlgorithm::xorOut;
}

/**
 * @brief  Specification for a CRC algorithm.
 *
 * @tparam Derived The derived class type (a specific CRC algorithm)
 * @tparam T       The CRC integer type (e.g. uint16_t for CRC-16)
 * @tparam kPoly   The polynomial of the CRC algorithm
 * @tparam kInit   The initial value of the CRC algorithm
 * @tparam kXorOut The XOR output value of the CRC algorithm
 * @tparam refIn   Whether to reflect the input data
 * @tparam refOut  Whether to reflect the output data
 */
template <typename Derived, typename T, T kPoly, T kInit, T kXorOut, bool refIn, bool refOut>
struct SpecImpl {
    static_assert(std::is_integral_v<T>, "T must be an integral type");
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned type");

    using value_type = T;
    static constexpr T polynomial     = kPoly;
    static constexpr T initial        = kInit;
    static constexpr T xorOut         = kXorOut;
    static constexpr bool reflect_in  = refIn;
    static constexpr bool reflect_out = refOut;

    // TODO: return a StringView instead?
    static constexpr const char* name() { return Derived::name(); }

    // Implementation defined via link-seam injection?
    static constexpr value_type compute(util::Span<const uint8_t> input) {
        return details::crcBitwise<Derived>(input);
    }

    // TODO: support incremental processing (update, ... finalize)
};

} // namespace crc::details
