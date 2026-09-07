/** @file receiver_engine_narrow_test.cpp */

#include <wirespaces/transports/bits/receiver_engine.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace wirespaces::transport::bits::test {
namespace {

class NarrowCallbacks final : public ReceiverCallbacks {
public:
    TransferAdmission beginTransfer(const TransferInfo& info) noexcept override {
        return info.total_size <= object_.size() ? TransferAdmission::kAccepted : TransferAdmission::kTooLarge;
    }
    void onTransferFailed(FailureReason) noexcept override {}
    bool onSegment(uint32_t offset, ByteSpan payload) noexcept override {
        if (offset + payload.size() > object_.size()) {
            return false;
        }
        std::memcpy(object_.data() + offset, payload.data(), payload.size());
        ++segments_;
        return true;
    }

    void onDatagram(ByteSpan payload) noexcept override {
        datagram_size_ = static_cast<uint8_t>(payload.size());
        std::copy(payload.begin(), payload.end(), datagram_.begin());
    }

    void onTransferComplete() noexcept override { complete_ = true; }
    void onTransferAborted(AbortReason) noexcept override {}

    std::array<uint8_t, 16U> object_{};
    std::array<uint8_t, 8U> datagram_{};
    uint8_t datagram_size_{0U};
    uint8_t segments_{0U};
    bool complete_{false};
};

class NarrowSender final : public ReceiverPduSender {
public:
    MutableByteSpan prepare(uint16_t size) noexcept override {
        size_ = size <= storage_.size() ? size : 0U;
        return MutableByteSpan{storage_.data(), size_};
    }
    SendResult sendPrepared() noexcept override { return SendResult::kSent; }

    ByteSpan sent() const noexcept { return ByteSpan{storage_.data(), size_}; }

private:
    std::array<uint8_t, 16U> storage_{};
    uint16_t size_{0U};
};

template <size_t Size>
std::array<uint8_t, kSegmentHeaderSize + Size> segment(
    uint16_t index, const std::array<uint8_t, Size>& payload) {
    std::array<uint8_t, kSegmentHeaderSize + Size> message{};
    EXPECT_TRUE(encodeSegmentHeader(
        SegmentHeader{0x51U, index},
        MutableByteSpan{message.data(), message.size()}));
    std::copy(payload.begin(), payload.end(),
              message.begin() + kSegmentHeaderSize);
    return message;
}

}  // namespace

static_assert(WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1,
              "test must compile the constrained window implementation");

TEST(BitsNarrowReceiverEngineTest, TransfersAndExchangesDatagrams) {
    NarrowCallbacks callbacks{};
    NarrowSender sender{};
    BitsReceiverEngine engine{ReceiverEngineConfig{4U, 1U, 16U}, callbacks,
                              sender};

    const std::array<uint8_t, 2U> request_payload{0xAAU, 0x55U};
    std::array<uint8_t, kUserDatagramHeaderSize + request_payload.size()> request{};
    ASSERT_TRUE(encodeUserDatagram(
        ByteSpan{request_payload.data(), request_payload.size()},
        MutableByteSpan{request.data(), request.size()}));
    ASSERT_EQ(engine.process(ByteSpan{request.data(), request.size()}),
              ProcessResult::kProgress);
    EXPECT_EQ(callbacks.datagram_size_, 2U);
    EXPECT_EQ(callbacks.datagram_[0], 0xAAU);
    const std::array<uint8_t, 2U> response{0x11U, 0x22U};
    ASSERT_EQ(engine.sendDatagram(ByteSpan{response.data(), response.size()}),
              SendResult::kSent);

    std::array<uint8_t, kSetupSize> setup{};
    ASSERT_TRUE(encodeSetup(
        wirespaces::transport::bits::Setup{0x51U, 0x20U, 2U, 4U, 2U},
        MutableByteSpan{setup.data(), setup.size()}));
    ASSERT_EQ(engine.process(ByteSpan{setup.data(), setup.size()}),
              ProcessResult::kProgress);
    const std::array<uint8_t, kAckSize> initial_ack{
        encodeControl(MessageType::kAck), 0x51U, 0x00U,
        0x00U, 0x20U, 0x1FU};
    ASSERT_EQ(sender.sent().size(), initial_ack.size());
    EXPECT_TRUE(std::equal(sender.sent().begin(), sender.sent().end(),
                           initial_ack.begin(), initial_ack.end()));

    const auto early{segment(1U, std::array<uint8_t, 4U>{4U, 5U, 6U, 7U})};
    EXPECT_EQ(engine.process(ByteSpan{early.data(), early.size()}),
              ProcessResult::kProgress);
    EXPECT_EQ(callbacks.segments_, 0U);

    const auto first{segment(0U, std::array<uint8_t, 4U>{0U, 1U, 2U, 3U})};
    const auto second{segment(1U, std::array<uint8_t, 4U>{4U, 5U, 6U, 7U})};
    const auto final{segment(2U, std::array<uint8_t, 2U>{8U, 9U})};
    EXPECT_EQ(engine.process(ByteSpan{first.data(), first.size()}),
              ProcessResult::kProgress);
    EXPECT_EQ(engine.process(ByteSpan{second.data(), second.size()}),
              ProcessResult::kProgress);
    EXPECT_EQ(engine.process(ByteSpan{final.data(), final.size()}),
              ProcessResult::kProgress);
    const std::array<uint8_t, kAckSize> completed_ack{
        encodeControl(MessageType::kAck), 0x51U, 0x00U,
        0x00U, 0x22U, 0x22U};
    ASSERT_EQ(sender.sent().size(), completed_ack.size());
    EXPECT_TRUE(std::equal(sender.sent().begin(), sender.sent().end(),
                           completed_ack.begin(), completed_ack.end()));

    EXPECT_TRUE(callbacks.complete_);
    EXPECT_EQ(callbacks.segments_, 3U);
    for (uint8_t index{0U}; index < 10U; ++index) {
        EXPECT_EQ(callbacks.object_[index], index);
    }
}

}  // namespace wirespaces::transport::bits::test
