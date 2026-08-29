/*
 * @file  crc.h
 * @brief Generic bitwise CRC calculation for embedded C++17.
 */

#pragma once

#include <cstdint>

#include <foundation/span.h>

namespace wirespaces::crc::details {

/**
 * @brief Calculate a non-reflected CRC over contiguous input bytes.
 *
 * @tparam CrcAlgorithm CRC parameter specification.
 * @param[in] input Bytes protected by the CRC.
 * @return Computed CRC value.
 *
 * @todo Handle reflect_in and reflect_out.
 * @todo Support incremental processing (update, ... finalize).
 */
template <typename CrcAlgorithm>
constexpr typename CrcAlgorithm::value_type crcBitwise(
    foundation::Span<const uint8_t> input) noexcept {
    static_assert(
        !(CrcAlgorithm::reflect_in || CrcAlgorithm::reflect_out),
        "Reflection not implemented yet");

    using T = typename CrcAlgorithm::value_type;
    constexpr T kMsbBit = static_cast<T>(
        static_cast<T>(1U) << ((sizeof(T) * 8U) - 1U));
    constexpr size_t kDataShift{8U * (sizeof(T) - 1U)};

    T result{CrcAlgorithm::initial};
    for (const uint8_t byte : input) {
        result ^= static_cast<T>(byte) << kDataShift;
        for (uint8_t bit_index{0U}; bit_index < 8U; ++bit_index) {
            result = ((result & kMsbBit) != 0U)
                         ? static_cast<T>(
                               static_cast<T>(result << 1U) ^
                               CrcAlgorithm::polynomial)
                         : static_cast<T>(result << 1U);
        }
    }

    return static_cast<T>(result ^ CrcAlgorithm::xorOut);
}

/** @brief Compile-time specification and implementation of one CRC algorithm. */
template <
    typename Derived,
    typename T,
    T kPoly,
    T kInit,
    T kXorOut,
    bool refIn,
    bool refOut>
struct SpecImpl {
    static_assert(
        static_cast<T>(-1) > static_cast<T>(0),
        "T must be an unsigned integer type");

    using value_type = T;
    static constexpr T polynomial{kPoly};
    static constexpr T initial{kInit};
    static constexpr T xorOut{kXorOut};
    static constexpr bool reflect_in{refIn};
    static constexpr bool reflect_out{refOut};

    // TODO: return a StringView instead?
    static constexpr const char* name() noexcept { return Derived::name(); }

    /** @brief Compute this CRC over one contiguous byte span. */
    static constexpr value_type compute(
        foundation::Span<const uint8_t> input) noexcept {
        return crcBitwise<Derived>(input);
    }

    // TODO: support incremental processing (update, ... finalize).
};

} // namespace wirespaces::crc::details
