/**
 * @file mailbox_test.cpp
 * @brief Snapshot receiver storage, overwrite, generation, and bounds.
 */

#include <cstring>

#include <gtest/gtest.h>

#include <wirespaces/runtime/core.hpp>

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::PacketBuilder;
using support::TestPacket;

// Reading an empty snapshot reports that no value is available.
TEST(EndpointSnapshotReceiverTest, StartsEmpty) {
    EndpointSnapshotReceiver receiver{};
    uint8_t output[kDefaultEndpointStorageCapacity]{};
    uint16_t length{0U};
    uint32_t generation{0U};
    EXPECT_FALSE(receiver.read(MutableByteSpan{output}, length, generation));
}

// Receive copies the active payload and publishes generation one.
TEST(EndpointSnapshotReceiverTest, StoresPacketPayload) {
    EndpointSnapshotReceiver receiver{};
    TestPacket packet{PacketBuilder{}.withPayload("hello").packet()};
    EXPECT_EQ(receiver.receive(packet), ReceiveResult::kAccepted);

    uint8_t output[kDefaultEndpointStorageCapacity]{};
    uint16_t length{0U};
    uint32_t generation{0U};
    ASSERT_TRUE(receiver.read(MutableByteSpan{output}, length, generation));
    EXPECT_EQ(length, 5U);
    EXPECT_EQ(generation, 1U);
    EXPECT_EQ(std::memcmp(output, "hello", 5U), 0);
}

// A newer receive replaces the snapshot and increments its generation.
TEST(EndpointSnapshotReceiverTest, OverwritesWithNewGeneration) {
    EndpointSnapshotReceiver receiver{};
    TestPacket first{PacketBuilder{}.withPayload("first").packet()};
    TestPacket second{PacketBuilder{}.withPayload("second").packet()};
    EXPECT_EQ(receiver.receive(first), ReceiveResult::kAccepted);
    EXPECT_EQ(receiver.receive(second), ReceiveResult::kAccepted);
    EXPECT_EQ(receiver.generation(), 2U);
}

// Reads reject output spans too small for the retained payload.
TEST(EndpointSnapshotReceiverTest, RejectsSmallReadBuffer) {
    EndpointSnapshotReceiver receiver{};
    TestPacket packet{PacketBuilder{}.withPayload("hello").packet()};
    ASSERT_EQ(receiver.receive(packet), ReceiveResult::kAccepted);
    uint8_t output[4]{};
    uint16_t length{0U};
    uint32_t generation{0U};
    EXPECT_FALSE(receiver.read(MutableByteSpan{output}, length, generation));
}

} // namespace
} // namespace wirespaces::test
