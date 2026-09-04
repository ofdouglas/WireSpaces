/**
 * @file endpoint_queue_test.cpp
 * @brief Queued EndpointReceiver copying, FIFO, capacity, and payload bounds.
 */

#include <cstring>

#include <gtest/gtest.h>

#include <wirespaces/runtime/core.hpp>

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::PacketBuilder;
using support::TestPacket;

constexpr std::size_t kQueuePayloadCapacity{8U};
constexpr std::size_t kQueueCapacity{2U};
using TestReceiverQueue =
    EndpointReceiverQueue<kQueuePayloadCapacity, kQueueCapacity>;
WS_PACKET_BUFFER_DEFINE(DequeuedPacket, kQueuePayloadCapacity);

// Accepted packets are copied and remain valid after the ingress packet changes.
TEST(EndpointReceiverQueueTest, CopiesAndDequeuesPacket) {
    TestReceiverQueue receiver{};
    TestPacket ingress{PacketBuilder{}.withPayload("first").packet()};
    const Header expected_header{ingress.header()};
    ASSERT_EQ(receiver.receive(ingress), ReceiveResult::kAccepted);
    ingress.payload()[0] = static_cast<uint8_t>('X');

    DequeuedPacket output{};
    ASSERT_TRUE(receiver.dequeue(output));
    EXPECT_EQ(output.header().wire, expected_header.wire);
    EXPECT_EQ(output.header().source, expected_header.source);
    EXPECT_EQ(output.size(), 5U);
    EXPECT_EQ(std::memcmp(output.payload().data(), "first", 5U), 0);
}

// Complete packets leave the receiver in first-in-first-out order.
TEST(EndpointReceiverQueueTest, PreservesPacketOrder) {
    TestReceiverQueue receiver{};
    TestPacket first{PacketBuilder{}.withPayload("one").packet()};
    TestPacket second{PacketBuilder{}.withPayload("two").packet()};
    ASSERT_EQ(receiver.receive(first), ReceiveResult::kAccepted);
    ASSERT_EQ(receiver.receive(second), ReceiveResult::kAccepted);

    DequeuedPacket output{};
    ASSERT_TRUE(receiver.dequeue(output));
    EXPECT_EQ(std::memcmp(output.payload().data(), "one", 3U), 0);
    ASSERT_TRUE(receiver.dequeue(output));
    EXPECT_EQ(std::memcmp(output.payload().data(), "two", 3U), 0);
}

// A full Endpoint queue reports bounded-resource pressure to the Dispatcher.
TEST(EndpointReceiverQueueTest, ReportsFull) {
    TestReceiverQueue receiver{};
    TestPacket packet{PacketBuilder{}.withPayload("item").packet()};
    ASSERT_EQ(receiver.receive(packet), ReceiveResult::kAccepted);
    ASSERT_EQ(receiver.receive(packet), ReceiveResult::kAccepted);
    EXPECT_EQ(receiver.receive(packet), ReceiveResult::kFull);
}

// Payloads larger than the Endpoint's declared slot size are rejected.
TEST(EndpointReceiverQueueTest, RejectsOversizedPayload) {
    TestReceiverQueue receiver{};
    TestPacket packet{PacketBuilder{}.withPayloadLength(kQueuePayloadCapacity + 1U).packet()};
    EXPECT_EQ(receiver.receive(packet), ReceiveResult::kRejected);
}

// Polling an empty Endpoint queue reports that no packet is ready.
TEST(EndpointReceiverQueueTest, EmptyDequeueLeavesOutputUnavailable) {
    TestReceiverQueue receiver{};
    DequeuedPacket output{};
    EXPECT_FALSE(receiver.dequeue(output));
}

}  // namespace
}  // namespace wirespaces::test
