/**
 * @file bits_test.cpp
 * @brief Compact BITS happy-path integration tests.
 *
 * Modules covered: BITS Router/Dispatcher integration, USER_DATAGRAM sideband delivery,
 * SETUP, depth-one segment flow control, cumulative ACKs, and final partial segments.
 * Equivalence classes: datagrams in both connection directions and a multi-segment object.
 */

#include "support/bits_fixture.hpp"

#include <algorithm>
#include <array>

namespace wirespaces::transport::bits::test {

// USER_DATAGRAM payloads traverse the shared non-segment ingress path in both directions.
TEST_F(BitsConnectionFixture, DeliversUserDatagramsInBothDirections) {
    const uint8_t request_bytes[]{0x10U, 0x20U, 0x30U};
    const uint8_t response_bytes[]{0xA0U, 0xB0U};

    ASSERT_EQ(transmitter_.sendDatagram(ByteSpan{request_bytes}), SendResult::kSent);
    EXPECT_EQ(receiver_.process(), ProcessResult::kProgress);
    EXPECT_TRUE(std::equal(receiver_callbacks_.datagram().begin(),
                           receiver_callbacks_.datagram().end(), std::begin(request_bytes),
                           std::end(request_bytes)));

    ASSERT_EQ(receiver_.sendDatagram(ByteSpan{response_bytes}), SendResult::kSent);
    EXPECT_EQ(transmitter_.process(), ProcessResult::kProgress);
    EXPECT_TRUE(std::equal(transmitter_callbacks_.datagram().begin(),
                           transmitter_callbacks_.datagram().end(), std::begin(response_bytes),
                           std::end(response_bytes)));
}

// A RAM object crosses SETUP and three depth-one SEGMENT/ACK exchanges without data loss.
TEST_F(BitsConnectionFixture, TransfersMultiSegmentObject) {
    std::array<uint8_t, 35U> source{};
    for (size_t index{0U}; index < source.size(); ++index) {
        source[index] = static_cast<uint8_t>(index * 3U + 1U);
    }

    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 12U, 0x5AU, 0xFEU),
              StartResult::kStarted);

    for (uint8_t iteration{0U};
         iteration < 16U && transmitter_.state() != TransferState::kCompleted; ++iteration) {
        ASSERT_NE(transmitter_.process(), ProcessResult::kError);
        ASSERT_NE(receiver_.process(), ProcessResult::kError);
    }

    EXPECT_EQ(transmitter_.state(), TransferState::kCompleted);
    EXPECT_EQ(receiver_.state(), TransferState::kCompleted);
    EXPECT_EQ(receiver_callbacks_.segmentCount(), 3U);
    EXPECT_EQ(receiver_callbacks_.completionCount(), 1U);
    EXPECT_EQ(transmitter_callbacks_.completionCount(), 1U);
    ASSERT_EQ(receiver_callbacks_.receivedSize(), source.size());
    EXPECT_TRUE(std::equal(source.begin(), source.end(), receiver_callbacks_.object().begin()));
}

}  // namespace wirespaces::transport::bits::test
