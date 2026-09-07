/**
 * @file bits_test.cpp
 * @brief Compact BITS integration, recovery, flow-control, and terminal-state tests.
 */

#include "support/bits_fixture.hpp"

#include <algorithm>
#include <array>
#include <vector>

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


class CapturingReceiverPduSender final : public ReceiverPduSender {
public:
    MutableByteSpan prepare(uint16_t payload_size) noexcept override {
        prepared_size_ = 0U;
        if (payload_size > storage_.size()) {
            return MutableByteSpan{};
        }
        prepared_size_ = payload_size;
        return MutableByteSpan{storage_.data(), prepared_size_};
    }

    SendResult sendPrepared() noexcept override {
        sent_.assign(storage_.begin(), storage_.begin() + prepared_size_);
        ++calls;
        return result;
    }
    SendResult result{SendResult::kSent};
    uint32_t calls{0U};

    ByteSpan sent() const noexcept {
        return ByteSpan{sent_.data(), sent_.size()};
    }

private:
    std::array<uint8_t, 64U> storage_{};
    std::vector<uint8_t> sent_{};
    uint16_t prepared_size_{0U};
};

template <size_t PayloadSize>
std::array<uint8_t, kSegmentHeaderSize + PayloadSize> makeSegment(
    uint8_t session_id, uint16_t segment_index,
    const std::array<uint8_t, PayloadSize>& payload) {
    std::array<uint8_t, kSegmentHeaderSize + PayloadSize> message{};
    const MutableByteSpan storage{message.data(), message.size()};
    EXPECT_TRUE(encodeSegmentHeader(SegmentHeader{session_id, segment_index},
                                    storage));
    std::copy(payload.begin(), payload.end(),
              message.begin() + kSegmentHeaderSize);
    return message;
}

}  // namespace


TEST(BitsCodecTest, SetupUsesAlignedLittleEndianLayout) {
    const wirespaces::transport::bits::Setup original{0x12U, 0x34U, 0x5678U, 0x9ABCU, 0xDEF0U};
    std::array<uint8_t, kSetupSize> encoded{};
    ASSERT_TRUE(encodeSetup(original, MutableByteSpan{encoded.data(), encoded.size()}));

    const std::array<uint8_t, kSetupSize> expected{
        0x00U, 0x12U, 0x34U, 0x00U, 0x78U,
        0x56U, 0xBCU, 0x9AU, 0xF0U, 0xDEU};
    EXPECT_EQ(encoded, expected);

    wirespaces::transport::bits::Setup decoded{};
    ASSERT_TRUE(decodeSetup(ByteSpan{encoded.data(), encoded.size()}, decoded));
    EXPECT_EQ(decoded.session_id, original.session_id);
    EXPECT_EQ(decoded.initial_sequence_number, original.initial_sequence_number);
    EXPECT_EQ(decoded.final_segment_index, original.final_segment_index);
    EXPECT_EQ(decoded.segment_size, original.segment_size);
    EXPECT_EQ(decoded.final_segment_size, original.final_segment_size);

    encoded[3] = 1U;
    EXPECT_FALSE(decodeSetup(ByteSpan{encoded.data(), encoded.size()}, decoded));
    encoded[3] = 0U;
    encoded[0] = encodeControl(MessageType::kAck);
    EXPECT_FALSE(decodeSetup(ByteSpan{encoded.data(), encoded.size()}, decoded));
}

// Admission runs once after validation; temporary ACK congestion does not reset an accepted sink.
TEST(BitsReceiverEngineTest, AdmissionAndDuplicateSetupSurviveBackpressure) {
    ReceiverRecorder callbacks{};
    CapturingReceiverPduSender sender{};
    BitsReceiverEngine engine{{4U, 1U, 16U}, callbacks, sender};
    uint8_t setup[kSetupSize]{};
    ASSERT_TRUE(encodeSetup({9U, 0U, 0U, 4U, 4U}, MutableByteSpan{setup}));
    callbacks.admission = TransferAdmission::kBusy;
    EXPECT_EQ(engine.process(ByteSpan{setup}), ProcessResult::kProgress);
    EXPECT_EQ(engine.state(), TransferState::kIdle);
    callbacks.admission = TransferAdmission::kAccepted;
    sender.result = SendResult::kFull;
    EXPECT_EQ(engine.process(ByteSpan{setup}), ProcessResult::kBlocked);
    EXPECT_EQ(engine.state(), TransferState::kActive);
    EXPECT_EQ(callbacks.admission_count, 2U);
    EXPECT_EQ(callbacks.last_info.total_size, 4U);
    EXPECT_EQ(engine.process(ByteSpan{setup}), ProcessResult::kBlocked);
    EXPECT_EQ(callbacks.admission_count, 2U);
    sender.result = SendResult::kSent;
    EXPECT_EQ(engine.process(ByteSpan{setup}), ProcessResult::kProgress);
    const uint8_t malformed[]{0xFFU};
    EXPECT_EQ(engine.process(ByteSpan{malformed}), ProcessResult::kError);
    EXPECT_EQ(engine.state(), TransferState::kActive);
    EXPECT_EQ(callbacks.failure_count, 0U);
    ASSERT_TRUE(encodeSetup({10U, 0U, 0U, 4U, 4U}, MutableByteSpan{setup}));
    sender.result = SendResult::kRejected;
    engine.process(ByteSpan{setup});
    EXPECT_EQ(callbacks.admission_count, 2U); // busy active session is untouched
    EXPECT_EQ(engine.state(), TransferState::kActive); // failure to send Reject is unrelated
    sender.result = SendResult::kSent;
    EXPECT_EQ(engine.abort(), SendResult::kSent);
    EXPECT_EQ(callbacks.abortCount(), 1U);
    ASSERT_TRUE(encodeSetup({9U, 0U, 0U, 4U, 4U}, MutableByteSpan{setup}));
    engine.process(ByteSpan{setup});
    EXPECT_EQ(callbacks.admission_count, 3U); // ID reuse after termination is a new transfer
}

// Sink errors notify once; completed storage stays completed even if the final ACK is blocked.
TEST(BitsReceiverEngineTest, TerminalNotificationsDoNotDependOnAckAcceptance) {
    for (const bool accept : {false, true}) {
        ReceiverRecorder callbacks{};
        CapturingReceiverPduSender sender{};
        BitsReceiverEngine engine{{4U, 1U, 16U}, callbacks, sender};
        uint8_t setup[kSetupSize]{};
        ASSERT_TRUE(encodeSetup({9U, 0U, 0U, 4U, 4U}, MutableByteSpan{setup}));
        engine.process(ByteSpan{setup});
        uint8_t segment[kSegmentHeaderSize + 4U]{};
        ASSERT_TRUE(encodeSegmentHeader({9U, 0U}, MutableByteSpan{segment}));
        callbacks.accept_segments = accept;
        sender.result = SendResult::kFull;
        engine.process(ByteSpan{segment});
        engine.process(ByteSpan{segment});
        EXPECT_EQ(callbacks.failure_count, accept ? 0U : 1U);
        EXPECT_EQ(callbacks.completionCount(), accept ? 1U : 0U);
        EXPECT_EQ(engine.state(), accept ? TransferState::kCompleted : TransferState::kError);
        if (!accept) { EXPECT_EQ(callbacks.failure, FailureReason::kSinkRejected); }
    }
}

// Invalid geometry never reaches admission, while a permanent send failure terminates an accepted transfer.
TEST(BitsReceiverEngineTest, ValidationPrecedesAdmissionAndSendFailureIsReported) {
    ReceiverRecorder callbacks{};
    CapturingReceiverPduSender sender{};
    BitsReceiverEngine engine{{4U, 1U, 16U}, callbacks, sender};
    uint8_t setup[kSetupSize]{};
    ASSERT_TRUE(encodeSetup({9U, 0U, 0U, 8U, 4U}, MutableByteSpan{setup}));
    engine.process(ByteSpan{setup});
    EXPECT_EQ(callbacks.admission_count, 0U);
    ASSERT_TRUE(encodeSetup({9U, 0U, 0U, 4U, 4U}, MutableByteSpan{setup}));
    sender.result = SendResult::kNoRoute;
    EXPECT_EQ(engine.process(ByteSpan{setup}), ProcessResult::kError);
    EXPECT_EQ(callbacks.failure_count, 1U);
    EXPECT_EQ(callbacks.failure, FailureReason::kSendFailed);
    EXPECT_EQ(engine.state(), TransferState::kError);
}

// Backpressure is not a transmission or protocol retry; BITS retries admission on a later poll.
TEST_F(BitsConnectionFixture, FullLinksDoNotConsumeTransferRetries) {
    const uint8_t object[]{1U, 2U, 3U, 4U};
    ASSERT_EQ(transmitter_.startTransfer(ByteSpan{object}, 4U, 9U, 0U), StartResult::kStarted);
    transmitter_forwarder_.admission = LinkAdmission::kFull;
    for (unsigned i = 0; i < 10; ++i) {
        now_ms_ += 100U;
        EXPECT_EQ(transmitter_.process(now_ms_), ProcessResult::kBlocked);
        EXPECT_EQ(transmitter_.state(), TransferState::kStarting);
    }
    EXPECT_EQ(transmitter_forwarder_.attempts, 10U);
    EXPECT_EQ(transmitter_.sendDatagram(ByteSpan{object}), SendResult::kFull);
    transmitter_forwarder_.admission = LinkAdmission::kAccepted;
    ASSERT_EQ(transmitter_.process(++now_ms_), ProcessResult::kProgress);
    receiver_forwarder_.admission = LinkAdmission::kFull;
    EXPECT_EQ(receiver_.process(), ProcessResult::kBlocked);
    EXPECT_EQ(receiver_.state(), TransferState::kActive);
    receiver_forwarder_.admission = LinkAdmission::kAccepted;
    EXPECT_TRUE(pumpUntilTerminal());
    EXPECT_EQ(receiver_callbacks_.admission_count, 1U);
    EXPECT_EQ(transmitter_.state(), TransferState::kCompleted);
}

// The constrained engine exchanges user datagrams without endpoint, router, or queue objects.
TEST(BitsReceiverEngineTest, DeliversAndSendsUserDatagramsSynchronously) {
    ReceiverRecorder callbacks{};
    CapturingReceiverPduSender sender{};
    BitsReceiverEngine engine{ReceiverEngineConfig{8U, 1U}, callbacks, sender};

    const std::array<uint8_t, 3U> request{0x10U, 0x20U, 0x30U};
    std::array<uint8_t, kUserDatagramHeaderSize + request.size()> message{};
    ASSERT_TRUE(encodeUserDatagram(ByteSpan{request.data(), request.size()},
                                   MutableByteSpan{message.data(), message.size()}));
    EXPECT_EQ(engine.process(ByteSpan{message.data(), message.size()}),
              ProcessResult::kProgress);
    EXPECT_TRUE(std::equal(callbacks.datagram().begin(),
                           callbacks.datagram().end(), request.begin(),
                           request.end()));

    const std::array<uint8_t, 2U> response{0xA0U, 0xB0U};
    ASSERT_EQ(engine.sendDatagram(ByteSpan{response.data(), response.size()}),
              SendResult::kSent);
    ByteSpan decoded{};
    ASSERT_TRUE(decodeUserDatagram(sender.sent(), decoded));
    EXPECT_TRUE(std::equal(decoded.begin(), decoded.end(), response.begin(),
                           response.end()));
}

// A one-position grant accepts only the next segment and completes a partial final segment.
TEST(BitsReceiverEngineTest, OneWindowProfileTransfersInOrder) {
    ReceiverRecorder callbacks{};
    CapturingReceiverPduSender sender{};
    BitsReceiverEngine engine{ReceiverEngineConfig{4U, 1U}, callbacks, sender};

    std::array<uint8_t, kSetupSize> setup_message{};
    ASSERT_TRUE(encodeSetup(wirespaces::transport::bits::Setup{0x42U, 0x20U, 2U, 4U, 2U},
                            MutableByteSpan{setup_message.data(),
                                            setup_message.size()}));
    ASSERT_EQ(engine.process(ByteSpan{setup_message.data(), setup_message.size()}),
              ProcessResult::kProgress);
    Ack ack{};
    ASSERT_TRUE(decodeAck(sender.sent(), ack));
    EXPECT_EQ(ack.window_base, 0x1FU);
    EXPECT_EQ(ack.max_receive_sequence, 0x20U);

    const auto early_segment{
        makeSegment(0x42U, 1U, std::array<uint8_t, 4U>{4U, 5U, 6U, 7U})};
    EXPECT_EQ(engine.process(ByteSpan{early_segment.data(), early_segment.size()}),
              ProcessResult::kProgress);
    EXPECT_EQ(callbacks.segmentCount(), 0U);

    const auto segment_0{
        makeSegment(0x42U, 0U, std::array<uint8_t, 4U>{0U, 1U, 2U, 3U})};
    const auto segment_1{
        makeSegment(0x42U, 1U, std::array<uint8_t, 4U>{4U, 5U, 6U, 7U})};
    const auto segment_2{
        makeSegment(0x42U, 2U, std::array<uint8_t, 2U>{8U, 9U})};
    EXPECT_EQ(engine.process(ByteSpan{segment_0.data(), segment_0.size()}),
              ProcessResult::kProgress);
    EXPECT_EQ(engine.process(ByteSpan{segment_1.data(), segment_1.size()}),
              ProcessResult::kProgress);
    EXPECT_EQ(engine.process(ByteSpan{segment_2.data(), segment_2.size()}),
              ProcessResult::kProgress);

    EXPECT_EQ(engine.state(), TransferState::kCompleted);
    EXPECT_EQ(callbacks.segmentCount(), 3U);
    EXPECT_EQ(callbacks.completionCount(), 1U);
    ASSERT_EQ(callbacks.receivedSize(), 10U);
    for (size_t index{0U}; index < 10U; ++index) {
        EXPECT_EQ(callbacks.object()[index], static_cast<uint8_t>(index));
    }
}

// Resource limits are enforced by the engine and reported with a Compact BITS REJECT.
TEST(BitsReceiverEngineTest, RejectsUnsupportedSegmentSize) {
    ReceiverRecorder callbacks{};
    CapturingReceiverPduSender sender{};
    BitsReceiverEngine engine{ReceiverEngineConfig{4U, 1U}, callbacks, sender};

    std::array<uint8_t, kSetupSize> setup_message{};
    ASSERT_TRUE(encodeSetup(wirespaces::transport::bits::Setup{0x43U, 0x10U, 1U, 8U, 8U},
                            MutableByteSpan{setup_message.data(),
                                            setup_message.size()}));
    ASSERT_EQ(engine.process(ByteSpan{setup_message.data(), setup_message.size()}),
              ProcessResult::kProgress);

    Reject reject{};
    ASSERT_TRUE(decodeReject(sender.sent(), reject));
    EXPECT_EQ(reject.session_id, 0x43U);
    EXPECT_EQ(reject.reason, RejectReason::kUnsupportedSegmentSize);
    EXPECT_EQ(engine.state(), TransferState::kIdle);
}


TEST(BitsReceiverEngineTest, RejectsInvalidFinalSegmentGeometry) {
    ReceiverRecorder callbacks{};
    CapturingReceiverPduSender sender{};
    BitsReceiverEngine engine{ReceiverEngineConfig{8U, 1U}, callbacks, sender};

    std::array<uint8_t, kSetupSize> setup_message{};
    ASSERT_TRUE(encodeSetup(
        wirespaces::transport::bits::Setup{0x45U, 0x10U, 1U, 4U, 5U},
        MutableByteSpan{setup_message.data(), setup_message.size()}));
    ASSERT_EQ(engine.process(ByteSpan{setup_message.data(), setup_message.size()}),
              ProcessResult::kProgress);

    Reject reject{};
    ASSERT_TRUE(decodeReject(sender.sent(), reject));
    EXPECT_EQ(reject.session_id, 0x45U);
    EXPECT_EQ(reject.reason, RejectReason::kInvalidArgument);
    EXPECT_EQ(engine.state(), TransferState::kIdle);
}

TEST(BitsReceiverEngineTest, RejectsObjectLargerThanSinkCapacity) {
    ReceiverRecorder callbacks{};
    CapturingReceiverPduSender sender{};
    BitsReceiverEngine engine{ReceiverEngineConfig{8U, 1U, 12U}, callbacks,
                              sender};

    std::array<uint8_t, kSetupSize> setup_message{};
    ASSERT_TRUE(encodeSetup(
        wirespaces::transport::bits::Setup{0x44U, 0x10U, 3U, 4U, 1U},
        MutableByteSpan{setup_message.data(), setup_message.size()}));
    ASSERT_EQ(engine.process(ByteSpan{setup_message.data(), setup_message.size()}),
              ProcessResult::kProgress);

    Reject reject{};
    ASSERT_TRUE(decodeReject(sender.sent(), reject));
    EXPECT_EQ(reject.session_id, 0x44U);
    EXPECT_EQ(reject.reason, RejectReason::kObjectTooLarge);
    EXPECT_EQ(engine.state(), TransferState::kIdle);
}


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
                  0x77U, 0x10U, 1U, 8U, 8U}),
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
