/**
 * @file mailbox_test.cpp
 * @brief Single-slot mailbox coverage: store, read, generation, capacity boundaries.
 */

#include <cstring>

#include <gtest/gtest.h>

#include <runtime/core.hpp>

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::asPacketBuffer;
using support::PacketBuilder;
using support::TestPacket;
using wirespaces::Mailbox;

class MailboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        ws_mailbox_init(&mailbox_);
    }

    TestPacket makePacketWithPayloadLength(uint16_t length) {
        return PacketBuilder{}.withPayloadLength(length).packet();
    }

    Mailbox mailbox_{};
};

TEST_F(MailboxTest, RejectsReadWhenEmpty) {
    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;

    EXPECT_FALSE(ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
}

TEST_F(MailboxTest, StoresAndReadsZeroLengthPayload) {
    const TestPacket packet = makePacketWithPayloadLength(0U);
    EXPECT_TRUE(ws_mailbox_store_from_packet(&mailbox_, asPacketBuffer(&packet)));

    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_TRUE(ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
    EXPECT_EQ(length, 0U);
    EXPECT_EQ(generation, 1U);
}

TEST_F(MailboxTest, StoresAndReadsMaxCapacityPayload) {
    const TestPacket packet = makePacketWithPayloadLength(WS_MAILBOX_DEFAULT_CAPACITY);
    EXPECT_TRUE(ws_mailbox_store_from_packet(&mailbox_, asPacketBuffer(&packet)));

    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_TRUE(ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
    EXPECT_EQ(length, WS_MAILBOX_DEFAULT_CAPACITY);
    EXPECT_EQ(generation, 1U);
    EXPECT_EQ(std::memcmp(buffer, packet.data, WS_MAILBOX_DEFAULT_CAPACITY), 0);
}

TEST_F(MailboxTest, RejectsPayloadLargerThanCapacity) {
    TestPacket packet = makePacketWithPayloadLength(WS_MAILBOX_DEFAULT_CAPACITY);
    asPacketBuffer(&packet)->size = WS_MAILBOX_DEFAULT_CAPACITY + 1U;

    EXPECT_FALSE(ws_mailbox_store_from_packet(&mailbox_, asPacketBuffer(&packet)));
    EXPECT_FALSE(mailbox_.occupied);
}

TEST_F(MailboxTest, RejectsPayloadSizeGreaterThanPacketCapacity) {
    TestPacket packet = makePacketWithPayloadLength(8U);
    asPacketBuffer(&packet)->size = 16U;
    asPacketBuffer(&packet)->capacity = 8U;

    EXPECT_FALSE(ws_mailbox_store_from_packet(&mailbox_, asPacketBuffer(&packet)));
    EXPECT_FALSE(mailbox_.occupied);
}

TEST_F(MailboxTest, IncrementsGenerationOnOverwrite) {
    TestPacket first = PacketBuilder{}.withPayload("first").packet();
    TestPacket second = PacketBuilder{}.withPayload("second").packet();

    EXPECT_TRUE(ws_mailbox_store_from_packet(&mailbox_, asPacketBuffer(&first)));
    EXPECT_TRUE(ws_mailbox_store_from_packet(&mailbox_, asPacketBuffer(&second)));

    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_TRUE(ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
    EXPECT_EQ(generation, 2U);
    EXPECT_EQ(length, 6U);
    EXPECT_STREQ(reinterpret_cast<char*>(buffer), "second");
}

TEST_F(MailboxTest, RejectsReadWhenOutputBufferTooSmall) {
    const TestPacket packet = PacketBuilder{}.withPayload("hello").packet();
    EXPECT_TRUE(ws_mailbox_store_from_packet(&mailbox_, asPacketBuffer(&packet)));

    uint8_t buffer[4]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_FALSE(ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
}

TEST_F(MailboxTest, ReceiveCallbackStoresPacket) {
    const TestPacket packet = PacketBuilder{}.withPayload("callback").packet();
    ws_mailbox_receive_callback(&mailbox_, asPacketBuffer(&packet));

    EXPECT_TRUE(mailbox_.occupied);
    EXPECT_EQ(mailbox_.generation, 1U);
}

TEST_F(MailboxTest, HandlesNullPointers) {
    ws_mailbox_init(nullptr);
    EXPECT_FALSE(ws_mailbox_store_from_packet(nullptr, nullptr));
    EXPECT_FALSE(ws_mailbox_read(nullptr, nullptr, 0U, nullptr, nullptr));

    uint8_t buffer[4]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_FALSE(ws_mailbox_read(&mailbox_, nullptr, sizeof(buffer), &length, &generation));
    EXPECT_FALSE(ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), nullptr, &generation));
    EXPECT_FALSE(ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, nullptr));

    ws_mailbox_receive_callback(nullptr, nullptr);
    SUCCEED();
}

} // namespace
} // namespace wirespaces::test
