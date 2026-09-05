/**
 * @file packet_test.cpp
 * @brief Packet initialization, bounded resizing, spans, and alignment.
 */

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include <wirespaces/runtime/core.hpp>

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::TestPacket;
WS_PACKET_BUFFER_DEFINE(SmallPacket, 8U);

// Copying between different capacities preserves destination storage and only copies active bytes.
TEST(PacketTest, CopiesContentsWithoutCapacityOrUnusedBytes) {
    TestPacket source{};
    SmallPacket output{};
    ASSERT_TRUE(source.initialize(3U, ControlFields{QoS::kCritical, true, TransportType::kBits}));
    source.header().wire = WireNumber{7U};
    source.header().source = HostId{2U};
    source.header().destination = HostId{3U};
    source.header().endpoint = EndpointAddress{123U};
    std::memcpy(source.payload().data(), "abc", 3U);
    ASSERT_TRUE(output.resize(8U));
    std::memset(output.payload().data(), 0xAA, 8U);
    ASSERT_TRUE(output.copyFrom(source));
    EXPECT_EQ(output.capacity(), 8U);
    EXPECT_EQ(output.size(), 3U);
    EXPECT_EQ(std::memcmp(&output.header(), &source.header(), sizeof(Header)), 0);
    EXPECT_EQ(std::memcmp(output.payload().data(), "abc", 3U), 0);
    ASSERT_TRUE(output.resize(8U));
    EXPECT_EQ(output.payload()[3], 0xAAU);
    EXPECT_EQ(output.payload()[7], 0xAAU);
    ASSERT_TRUE(source.copyFrom(output));
    EXPECT_EQ(source.capacity(), kDefaultEndpointStorageCapacity);
    EXPECT_EQ(source.size(), 8U);
}

// An oversized source leaves the destination's size, header and payload untouched.
TEST(PacketTest, FailedCopyPreservesDestination) {
    TestPacket source{};
    SmallPacket output{};
    ASSERT_TRUE(source.resize(9U));
    ASSERT_TRUE(output.resize(8U));
    output.header().wire = WireNumber{17U};
    std::memset(output.payload().data(), 0xAA, 8U);
    EXPECT_FALSE(output.copyFrom(source));
    EXPECT_EQ(output.capacity(), 8U);
    EXPECT_EQ(output.size(), 8U);
    EXPECT_EQ(output.header().wire, WireNumber{17U});
    for (const auto byte : output.payload()) {
        EXPECT_EQ(byte, 0xAAU);
    }
}

// Self-copy is harmless; header-only packets replace the active size without touching storage.
TEST(PacketTest, CopiesSelfAndEmptyPayload) {
    SmallPacket packet{};
    ASSERT_TRUE(packet.resize(8U));
    std::memset(packet.payload().data(), 0xAA, 8U);
    ASSERT_TRUE(packet.copyFrom(packet));
    EXPECT_EQ(packet.size(), 8U);
    EXPECT_EQ(packet.payload()[7], 0xAAU);
    TestPacket empty{};
    empty.header().wire = WireNumber{9U};
    ASSERT_TRUE(packet.copyFrom(empty));
    EXPECT_EQ(packet.size(), 0U);
    EXPECT_EQ(packet.header().wire, WireNumber{9U});
    EXPECT_EQ(packet.capacity(), 8U);
}

// Initialization sets size and canonical control fields without changing capacity.
TEST(PacketTest, InitializesSizeAndControlFields) {
    TestPacket packet{};
    ASSERT_TRUE(packet.initialize(12U, ControlFields{QoS::kBackground, true, TransportType::kSimple}));
    EXPECT_EQ(packet.capacity(), kDefaultEndpointStorageCapacity);
    EXPECT_EQ(packet.size(), 12U);
    EXPECT_EQ(packet.header().qos(), QoS::kBackground);
    EXPECT_TRUE(packet.header().hasExtensions());
}

// Mutable and const payload access expose the same active bytes.
TEST(PacketTest, PayloadReturnsSpanOfActiveSize) {
    TestPacket packet{};
    ASSERT_TRUE(packet.resize(7U));
    std::memcpy(packet.payload().data(), "payload", 7U);

    const PacketBuffer& const_packet{packet};
    EXPECT_EQ(const_packet.payload().size(), 7U);
    EXPECT_EQ(std::memcmp(const_packet.payload().data(), "payload", 7U), 0);
}

// PacketBuffer and its trailing payload begin at four-byte boundaries.
TEST(PacketTest, PayloadIsFourByteAligned) {
    TestPacket packet{};
    ASSERT_TRUE(packet.resize(1U));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&packet) % 4U, 0U);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(packet.payload().data()) % 4U, 0U);
}

// Resize rejects capacity overflow and preserves the previous active size.
TEST(PacketTest, RejectsResizeBeyondCapacity) {
    TestPacket packet{};
    ASSERT_TRUE(packet.resize(5U));
    EXPECT_FALSE(packet.resize(kDefaultEndpointStorageCapacity + 1U));
    EXPECT_EQ(packet.size(), 5U);
}

} // namespace
} // namespace wirespaces::test
