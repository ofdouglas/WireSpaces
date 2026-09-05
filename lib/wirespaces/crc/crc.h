/*
 * @file  crc.h
 * @brief Generic bitwise CRC calculation for embedded C++17.
 */

#pragma once

#include <wirespaces/foundation/span.h>

#include <cstdint>


namespace wirespaces::crc {

/**
 * @brief Controls CRC processing (incremental, all-at-once, etc).
 */
struct ComputeFlags {
    static constexpr uint8_t kInitialize = 1 << 0U;
    static constexpr uint8_t kFinalize = 1 << 1U;
    uint8_t value{0U};
};

constexpr ComputeFlags kDefaultComputeFlags{ComputeFlags::kInitialize | ComputeFlags::kFinalize};

} // namespace wirespaces::crc

namespace wirespaces::crc::details {

/**
 * @brief Calculate a non-reflected CRC over contiguous input bytes.
 *
 * @tparam CrcAlgorithm The CRC parameter specification.
 * @param[in] input Bytes protected by the CRC.
 * @param[in] initial CRC register when @p flags omits kInitialize.
 * @param[in] flags Compute flags.
 * @return Computed CRC value, or the intermediate register when @p flags omits kFinalize.
 *
 * @todo Handle reflect_in and reflect_out.
 */
template <typename CrcAlgorithm>
constexpr typename CrcAlgorithm::value_type crcBitwise(
    foundation::Span<const uint8_t> input, typename CrcAlgorithm::value_type initial, ComputeFlags flags) noexcept {
    static_assert(!(CrcAlgorithm::reflect_in || CrcAlgorithm::reflect_out),
                  "Reflection not implemented yet");

    using T = typename CrcAlgorithm::value_type;
    constexpr T kMsbBit = static_cast<T>(static_cast<T>(1U) << ((sizeof(T) * 8U) - 1U));
    constexpr size_t kDataShift{8U * (sizeof(T) - 1U)};

    T result = (flags.value & ComputeFlags::kInitialize) ? CrcAlgorithm::initial : initial;
    for (const uint8_t byte : input) {
        result ^= static_cast<T>(byte) << kDataShift;
        for (uint8_t bit_index{0U}; bit_index < 8U; ++bit_index) {
            result = ((result & kMsbBit) != 0U)
                         ? static_cast<T>(static_cast<T>(result << 1U) ^ CrcAlgorithm::polynomial)
                         : static_cast<T>(result << 1U);
        }
    }

    return (flags.value & ComputeFlags::kFinalize) ? static_cast<T>(result ^ CrcAlgorithm::xorOut) : result;
}

/** @brief Compile-time specification and implementation of one CRC algorithm. */
template <typename Derived, typename T, T kPoly, T kInit, T kXorOut, bool refIn, bool refOut>
struct SpecImpl {
    static_assert(static_cast<T>(-1) > static_cast<T>(0), "T must be an unsigned integer type");

    using value_type = T;
    static constexpr T polynomial{kPoly};
    static constexpr T initial{kInit};
    static constexpr T xorOut{kXorOut};
    static constexpr bool reflect_in{refIn};
    static constexpr bool reflect_out{refOut};

    // TODO: return a StringView instead?
    static constexpr const char* name() noexcept {
        return Derived::name();
    }

    /** @brief Compute this CRC over one contiguous byte span. */
    static constexpr value_type compute(
        foundation::Span<const uint8_t> input,
        ComputeFlags flags = kDefaultComputeFlags,
        value_type initial = Derived::initial) noexcept {
        return crcBitwise<Derived>(input, initial, flags);
    }

    /** @brief Return the initial CRC register before any input has been processed. */
    static constexpr value_type init() noexcept {
        return crcBitwise<Derived>({}, Derived::initial, ComputeFlags{ComputeFlags::kInitialize});
    }

    /** @brief Update the CRC register with more input bytes. */
    static constexpr value_type update(foundation::Span<const uint8_t> input, value_type state) noexcept {
        return crcBitwise<Derived>(input, state, ComputeFlags{});
    }

    /** @brief Apply xorOut to a CRC register after all input has been processed. */
    static constexpr value_type finalize(value_type state) noexcept {
        return crcBitwise<Derived>({}, state, ComputeFlags{ComputeFlags::kFinalize});
    }
};

}  // namespace wirespaces::crc::details
