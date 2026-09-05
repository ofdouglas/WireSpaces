/**
 * @file crc_test.cpp
 * @brief One-shot and incremental CRC calculation.
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <wirespaces/crc/crc_algorithm.h>

namespace wirespaces::test {
namespace {

using wirespaces::crc::algorithm::Crc16CcittFalse;
using wirespaces::crc::algorithm::SaeJ1850;
using wirespaces::crc::ComputeFlags;
using wirespaces::foundation::Span;

constexpr std::uint8_t kCheckInput[]{
    '1', '2', '3', '4', '5', '6', '7', '8', '9'};

// CRC-16/CCITT-FALSE matches its standard check vector.
TEST(CrcTest, Crc16CcittFalseMatchesCheckVector) {
    const Span<const std::uint8_t> check_span{kCheckInput, sizeof(kCheckInput)};
    EXPECT_EQ(Crc16CcittFalse::compute(check_span), 0x29B1U);
    EXPECT_EQ(Crc16CcittFalse::compute(kCheckInput), 0x29B1U);
}

// Incremental init/update/finalize matches one-shot computation.
TEST(CrcTest, Crc16CcittFalseIncrementalMatchesOneShot) {
    const Span<const std::uint8_t> full_input{kCheckInput, sizeof(kCheckInput)};
    const Span<const std::uint8_t> first_chunk{kCheckInput, 4U};
    const Span<const std::uint8_t> second_chunk{kCheckInput + 4U, 5U};

    const auto one_shot{Crc16CcittFalse::compute(full_input)};

    auto state{Crc16CcittFalse::init()};
    state = Crc16CcittFalse::update(first_chunk, state);
    state = Crc16CcittFalse::update(second_chunk, state);
    EXPECT_EQ(Crc16CcittFalse::finalize(state), one_shot);
}

// Flag-based incremental processing matches one-shot computation.
TEST(CrcTest, Crc16CcittFalseFlagIncrementalMatchesOneShot) {
    const Span<const std::uint8_t> full_input{kCheckInput, sizeof(kCheckInput)};
    const Span<const std::uint8_t> first_chunk{kCheckInput, 4U};
    const Span<const std::uint8_t> second_chunk{kCheckInput + 4U, 5U};

    const auto one_shot{Crc16CcittFalse::compute(full_input)};

    auto state{Crc16CcittFalse::compute(first_chunk, ComputeFlags{ComputeFlags::kInitialize})};
    state = Crc16CcittFalse::compute(second_chunk, ComputeFlags{}, state);
    EXPECT_EQ(
        Crc16CcittFalse::compute({}, ComputeFlags{ComputeFlags::kFinalize}, state),
        one_shot);
}

// finalize applies xorOut for algorithms where it is non-zero.
TEST(CrcTest, SaeJ1850FinalizeAppliesXorOut) {
    const Span<const std::uint8_t> empty{};
    EXPECT_EQ(SaeJ1850::finalize(SaeJ1850::init()), SaeJ1850::compute(empty));
}

}  // namespace
}  // namespace wirespaces::test
