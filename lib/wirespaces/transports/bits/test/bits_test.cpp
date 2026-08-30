/**
 * @file bits_test.cpp
 * @brief Compact BITS integration, recovery, flow-control, and terminal-state tests.
 */

#include "support/bits_fixture.hpp"

#include <algorithm>
#include <array>

namespace wirespaces::transport::bits::test {
namespace {

template <size_t Size>
std::array<uint8_t, Size> makeObject() {
    std::array<uint8_t, Size> object{};
    for (size_t index{0U}; index < object.size(); ++index) {
        object[index] = static_cast<uint8_t>(index * 3U + 1U);
    }
    return object;
}

}  // namespace

// USER_DATAGRAM remains connection-scoped and independent of segmented-transfer state.
TEST_F(BitsConnectionFixture, DeliversUserDatagramsInBothDirections) {
    const uint8_t request_bytes[]{0x10U, 0x20U, 0x30U};
    const uint8_t response_bytes[]{0xA0U, 0xB0U};

    ASSERT_EQ(transmitter_.sendDatagram(ByteSpan{request_bytes}), SendResult::kSent);
    EXPECT_EQ(receiver_.process(), ProcessResult::kProgress);
    EXPECT_TRUE(std::equal(receiver_callbacks_.datagram().begin(),
                           receiver_callbacks_.datagram().end(), std::begin(request_bytes),
                           std::end(request_bytes)));

    ASSERT_EQ(receiver_.sendDatagram(ByteSpan{response_bytes}), SendResult::kSent);
    EXPECT_EQ(transmitter_.process(now_ms_), ProcessResult::kProgress);
    EXPECT_TRUE(std::equal(transmitter_callbacks_.datagram().begin(),
                           transmitter_callbacks_.datagram().end(), std::begin(response_bytes),
                           std::end(response_bytes)));
}

// A partial final segment and sequence-number wrap both work across the four-position window.
TEST_F(BitsConnectionFixture, TransfersMultiSegmentObject) {
    const auto source{makeObject<35U>()};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 12U, 0x5AU,
                                         0xFEU),
              StartResult::kStarted);

    ASSERT_TRUE(pumpUntilTerminal());
    EXPECT_EQ(transmitter_.state(), TransferState::kCompleted);
    EXPECT_EQ(receiver_.state(), TransferState::kCompleted);
    EXPECT_EQ(receiver_callbacks_.segmentCount(), 3U);
    EXPECT_EQ(receiver_callbacks_.completionCount(), 1U);
    EXPECT_EQ(transmitter_callbacks_.completionCount(), 1U);
    ASSERT_EQ(receiver_callbacks_.receivedSize(), source.size());
    EXPECT_TRUE(std::equal(source.begin(), source.end(), receiver_callbacks_.object().begin()));
}

// SETUP is regenerated from stable state when the first transmission disappears.
TEST_F(BitsConnectionFixture, RetransmitsLostSetup) {
    const auto source{makeObject<18U>()};
    transmitter_forwarder_.dropNext(MessageType::kSetup);
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x21U,
                                         0x40U),
              StartResult::kStarted);

    ASSERT_TRUE(pumpUntilTerminal());
    EXPECT_EQ(transmitter_forwarder_.messageCount(MessageType::kSetup), 2U);
    EXPECT_EQ(transmitter_.state(), TransferState::kCompleted);
    EXPECT_TRUE(std::equal(source.begin(), source.end(), receiver_callbacks_.object().begin()));
}

// Selective ACK state preserves later segments while the missing first segment is retried.
TEST_F(BitsConnectionFixture, RetransmitsLostSegmentWithoutRedeliveringOthers) {
    const auto source{makeObject<47U>()};
    transmitter_forwarder_.dropNext(MessageType::kSegment);
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x22U,
                                         0x80U),
              StartResult::kStarted);

    ASSERT_TRUE(pumpUntilTerminal());
    EXPECT_EQ(transmitter_.state(), TransferState::kCompleted);
    EXPECT_EQ(receiver_callbacks_.segmentCount(), 6U);
    EXPECT_GT(transmitter_forwarder_.messageCount(MessageType::kSegment), 6U);
    EXPECT_TRUE(std::equal(source.begin(), source.end(), receiver_callbacks_.object().begin()));
}

// A duplicate of the final segment elicits a fresh cumulative ACK but not a second sink callback.
TEST_F(BitsConnectionFixture, RecoversWhenFinalAckIsLost) {
    const auto source{makeObject<8U>()};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x23U,
                                         0x10U),
              StartResult::kStarted);

    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);
    receiver_forwarder_.dropNext(MessageType::kAck);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    ASSERT_TRUE(pumpUntilTerminal());
    EXPECT_EQ(transmitter_.state(), TransferState::kCompleted);
    EXPECT_EQ(receiver_callbacks_.segmentCount(), 1U);
    EXPECT_EQ(receiver_callbacks_.completionCount(), 1U);
    EXPECT_EQ(transmitter_forwarder_.messageCount(MessageType::kSegment), 2U);
}

// A zero-width ACK blocks data; the active persist timer emits PROBE and recovers current grant.
TEST_F(BitsConnectionFixture, ProbesAndRecoversAClosedWindow) {
    const auto source{makeObject<20U>()};
    receiver_forwarder_.dropNext(MessageType::kAck);
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x24U,
                                         0x30U),
              StartResult::kStarted);

    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    const uint8_t empty_base{0x2FU};
    ASSERT_EQ(injectAck(Ack{0x24U, 0U, empty_base, empty_base}),
              DispatchResult::kAccepted);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);

    now_ms_ += timing_.probe_timeout_ms;
    ASSERT_EQ(transmitter_.process(now_ms_), ProcessResult::kProgress);
    EXPECT_EQ(transmitter_forwarder_.messageCount(MessageType::kProbe), 1U);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    ASSERT_TRUE(pumpUntilTerminal());
    EXPECT_EQ(transmitter_.state(), TransferState::kCompleted);
    EXPECT_TRUE(std::equal(source.begin(), source.end(), receiver_callbacks_.object().begin()));
}

// Four ingress slots permit later segments to be accepted and selectively ACKed before a hole.
TEST_F(BitsConnectionFixture, AcceptsAndRecoversOutOfOrderSegments) {
    const auto source{makeObject<32U>()};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x25U,
                                         0xFEU),
              StartResult::kStarted);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    transmitter_forwarder_.holdNextSegment();
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);
    ASSERT_TRUE(transmitter_forwarder_.releaseHeld());
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    ASSERT_GE(receiver_callbacks_.offsets().size(), 2U);
    EXPECT_EQ(receiver_callbacks_.offsets()[0], 8U);
    EXPECT_EQ(receiver_callbacks_.offsets()[1], 0U);

    ASSERT_TRUE(pumpUntilTerminal());
    EXPECT_EQ(receiver_callbacks_.segmentCount(), 4U);
    EXPECT_TRUE(std::equal(source.begin(), source.end(), receiver_callbacks_.object().begin()));
}

// A different valid SETUP cannot displace the active session and receives a session-specific REJECT.
TEST_F(BitsConnectionFixture, RejectsASecondSetupWhileBusy) {
    const auto source{makeObject<24U>()};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x26U,
                                         0x50U),
              StartResult::kStarted);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);
    ASSERT_EQ(injectSetup(wirespaces::transport::bits::Setup{
                  0x77U, 0x10U, 8U, 16U}),
              DispatchResult::kAccepted);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    Reject reject{};
    ASSERT_TRUE(decodeReject(receiver_forwarder_.lastPacket().payload(), reject));
    EXPECT_EQ(reject.session_id, 0x77U);
    EXPECT_EQ(reject.reason, RejectReason::kBusy);
    EXPECT_EQ(receiver_.state(), TransferState::kActive);
}

// Matching REJECT is terminal at the transmitter and reports the receiver's reason.
TEST_F(BitsConnectionFixture, ReportsMatchingSetupRejection) {
    const auto source{makeObject<16U>()};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x27U,
                                         0x60U),
              StartResult::kStarted);
    ASSERT_EQ(injectReject(Reject{0x27U, RejectReason::kUnsupportedSegmentSize}),
              DispatchResult::kAccepted);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);

    EXPECT_EQ(transmitter_.state(), TransferState::kRejected);
    EXPECT_EQ(transmitter_callbacks_.rejectionCount(), 1U);
    EXPECT_EQ(transmitter_callbacks_.rejectionReason(),
              RejectReason::kUnsupportedSegmentSize);
}

// Transmitter-initiated ABORT terminates both matching sessions and is reported once per side.
TEST_F(BitsConnectionFixture, TransmitterAbortStopsBothEndpoints) {
    const auto source{makeObject<24U>()};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x28U,
                                         0x70U),
              StartResult::kStarted);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    ASSERT_EQ(transmitter_.abort(), SendResult::kSent);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);
    EXPECT_EQ(transmitter_.state(), TransferState::kAborted);
    EXPECT_EQ(receiver_.state(), TransferState::kAborted);
    EXPECT_EQ(transmitter_callbacks_.abortCount(), 1U);
    EXPECT_EQ(receiver_callbacks_.abortCount(), 1U);
}

// Receiver-initiated ABORT uses the same session-specific terminal transition in reverse.
TEST_F(BitsConnectionFixture, ReceiverAbortStopsBothEndpoints) {
    const auto source{makeObject<24U>()};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x29U,
                                         0x90U),
              StartResult::kStarted);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);

    ASSERT_EQ(receiver_.abort(), SendResult::kSent);
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    EXPECT_EQ(transmitter_.state(), TransferState::kAborted);
    EXPECT_EQ(receiver_.state(), TransferState::kAborted);
    EXPECT_EQ(transmitter_callbacks_.abortCount(), 1U);
    EXPECT_EQ(receiver_callbacks_.abortCount(), 1U);
}

// Exhausting SETUP retries emits ABORT and stops both endpoints under configured policy.
TEST_F(BitsConnectionFixture, RetryExhaustionAbortsTheTransfer) {
    const auto source{makeObject<16U>()};
    receiver_forwarder_.dropNext(MessageType::kAck, UINT8_MAX);
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{source.data(), source.size()}, 8U, 0x2AU,
                                         0xA0U),
              StartResult::kStarted);

    ProcessResult result{ProcessResult::kIdle};
    for (uint8_t iteration{0U}; iteration < 64U && result != ProcessResult::kError;
         ++iteration) {
        ++now_ms_;
        result = transmitter_.process(now_ms_);
        if (result == ProcessResult::kError) {
            break;
        }
        ASSERT_NE(receiver_.process(), ProcessResult::kError);
    }
    ASSERT_EQ(result, ProcessResult::kError);
    ASSERT_EQ(receiver_.process(), ProcessResult::kProgress);

    EXPECT_EQ(transmitter_.state(), TransferState::kAborted);
    EXPECT_EQ(receiver_.state(), TransferState::kAborted);
    EXPECT_EQ(transmitter_callbacks_.abortCount(), 1U);
    EXPECT_EQ(receiver_callbacks_.abortCount(), 1U);
    EXPECT_EQ(transmitter_forwarder_.messageCount(MessageType::kSetup), 4U);
    EXPECT_EQ(transmitter_forwarder_.messageCount(MessageType::kAbort), 1U);
}

}  // namespace wirespaces::transport::bits::test
