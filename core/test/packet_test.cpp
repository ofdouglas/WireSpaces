/**
 * @file packet_test.cpp
 * @brief Packet initialization, bounded resizing, spans, and alignment.
 */

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include <runtime/core.hpp>

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::TestPacket;

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
