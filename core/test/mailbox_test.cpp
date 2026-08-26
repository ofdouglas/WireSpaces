/**
 * @file mailbox_test.cpp
 * @brief Single-slot mailbox coverage: store, read, generation, capacity boundaries.
 *
 * Equivalence classes:
 * - Payload length: 0, 1, max capacity, over capacity
 * - Read buffer: exact fit, too small, empty mailbox
 * - Generation: increments on each successful store; overwrite replaces payload
 * - Null pointers: init, store, read, callback guards
 */

#include <cstring>

#include <gtest/gtest.h>

#include "mailbox.h"
#include "packet.h"
#include "ws_constants.h"

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::asPacketBuffer;
using support::PacketBuilder;
using support::TestPacket;

class MailboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        mailbox_init(&mailbox_);
    }

    TestPacket makePacketWithPayloadLength(uint16_t length) {
        return PacketBuilder{}.withPayloadLength(length).packet();
    }

    Mailbox mailbox_{};
};

// Verifies an empty mailbox rejects read attempts.
TEST_F(MailboxTest, RejectsReadWhenEmpty) {
    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;

    EXPECT_FALSE(mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
}

// Verifies a zero-length payload can be stored and read back.
TEST_F(MailboxTest, StoresAndReadsZeroLengthPayload) {
    const TestPacket packet = makePacketWithPayloadLength(0U);
    EXPECT_TRUE(mailbox_store_from_packet(&mailbox_, asPacketBuffer(
        const_cast<TestPacket*>(&packet))));

    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_TRUE(mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
    EXPECT_EQ(length, 0U);
    EXPECT_EQ(generation, 1U);
}

// Verifies a max-capacity payload is stored and read verbatim.
TEST_F(MailboxTest, StoresAndReadsMaxCapacityPayload) {
    const TestPacket packet = makePacketWithPayloadLength(WS_MAILBOX_DEFAULT_CAPACITY);
    EXPECT_TRUE(mailbox_store_from_packet(&mailbox_, asPacketBuffer(
        const_cast<TestPacket*>(&packet))));

    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_TRUE(mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
    EXPECT_EQ(length, WS_MAILBOX_DEFAULT_CAPACITY);
    EXPECT_EQ(generation, 1U);
    EXPECT_EQ(std::memcmp(buffer, packet.data, WS_MAILBOX_DEFAULT_CAPACITY), 0);
}

// Verifies payloads larger than mailbox capacity are rejected.
TEST_F(MailboxTest, RejectsPayloadLargerThanCapacity) {
    TestPacket packet = makePacketWithPayloadLength(WS_MAILBOX_DEFAULT_CAPACITY);
    asPacketBuffer(&packet)->length = WS_MAILBOX_DEFAULT_CAPACITY + 1U;

    EXPECT_FALSE(mailbox_store_from_packet(&mailbox_, asPacketBuffer(&packet)));
    EXPECT_FALSE(mailbox_.occupied);
}

// Verifies generation increments when the slot is overwritten.
TEST_F(MailboxTest, IncrementsGenerationOnOverwrite) {
    TestPacket first = PacketBuilder{}.withPayload("first").packet();
    TestPacket second = PacketBuilder{}.withPayload("second").packet();

    EXPECT_TRUE(mailbox_store_from_packet(&mailbox_, asPacketBuffer(&first)));
    EXPECT_TRUE(mailbox_store_from_packet(&mailbox_, asPacketBuffer(&second)));

    uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_TRUE(mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
    EXPECT_EQ(generation, 2U);
    EXPECT_EQ(length, 6U);
    EXPECT_STREQ(reinterpret_cast<char*>(buffer), "second");
}

// Verifies read fails when the output buffer is smaller than stored length.
TEST_F(MailboxTest, RejectsReadWhenOutputBufferTooSmall) {
    const TestPacket packet = PacketBuilder{}.withPayload("hello").packet();
    EXPECT_TRUE(mailbox_store_from_packet(&mailbox_, asPacketBuffer(
        const_cast<TestPacket*>(&packet))));

    uint8_t buffer[4]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_FALSE(mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation));
}

// Verifies receive callback stores through mailbox_store_from_packet.
TEST_F(MailboxTest, ReceiveCallbackStoresPacket) {
    const TestPacket packet = PacketBuilder{}.withPayload("callback").packet();
    mailbox_receive_callback(&mailbox_, asPacketBuffer(const_cast<TestPacket*>(&packet)));

    EXPECT_TRUE(mailbox_.occupied);
    EXPECT_EQ(mailbox_.generation, 1U);
}

// Verifies null pointer guards on mailbox APIs.
TEST_F(MailboxTest, HandlesNullPointers) {
    mailbox_init(nullptr);
    EXPECT_FALSE(mailbox_store_from_packet(nullptr, nullptr));
    EXPECT_FALSE(mailbox_read(nullptr, nullptr, 0U, nullptr, nullptr));

    uint8_t buffer[4]{};
    uint16_t length = 0U;
    uint32_t generation = 0U;
    EXPECT_FALSE(mailbox_read(&mailbox_, nullptr, sizeof(buffer), &length, &generation));
    EXPECT_FALSE(mailbox_read(&mailbox_, buffer, sizeof(buffer), nullptr, &generation));
    EXPECT_FALSE(mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, nullptr));

    mailbox_receive_callback(nullptr, nullptr);
    SUCCEED();
}

} // namespace
} // namespace wirespaces::test
