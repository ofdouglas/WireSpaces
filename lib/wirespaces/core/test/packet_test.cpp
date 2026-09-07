/**
 * @file packet_test.cpp
 * @brief Packet initialization, bounded resizing, spans, and alignment.
 */

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include <wirespaces/runtime/core.hpp>

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::TestPacket;
WS_PACKET_BUFFER_DEFINE(SmallPacket, 8U);

// Base prefix slicing/assignment is inaccessible; owning packets still copy complete storage.
TEST(PacketTest, OnlyConcretePacketsSupportOrdinaryCopyAndMove) {
    static_assert(!std::is_copy_constructible_v<PacketBuffer>);
    static_assert(!std::is_move_constructible_v<PacketBuffer>);
    static_assert(!std::is_copy_assignable_v<PacketBuffer>);
    static_assert(!std::is_move_assignable_v<PacketBuffer>);
    static_assert(std::is_copy_constructible_v<SmallPacket> && std::is_copy_assignable_v<SmallPacket>);
    SmallPacket source{};
    ASSERT_TRUE(source.initialize(8U, ControlFields::bits()));
    source.payload()[7] = 0xAB;
    source.setIngressIndex(3U);
    SmallPacket copy{source};
    SmallPacket assigned{};
    assigned = copy;
    SmallPacket moved{std::move(assigned)};
    copy = std::move(moved);
    EXPECT_EQ(copy.payload()[7], 0xAB);
    EXPECT_EQ(copy.capacity(), 8U);
    EXPECT_EQ(copy.ingressIndex(), 3U);
}

// Complete initialization replaces stale addresses/controls and ingress, not payload storage.
TEST(PacketTest, InitializesCompleteConnectionAtSizeBoundaries) {
    constexpr ConnectionAddress connection{WireNumber{7U}, HostId{2U}, HostId{3U}, EndpointAddress{123U}};
    SmallPacket packet{};
    for (const uint16_t size : {0U, 8U}) {
        ASSERT_TRUE(packet.initialize(8U, ControlFields{QoS::kCritical, true, TransportType::kSimple}));
        std::memset(packet.payload().data(), 0xAA, 8U);
        packet.header().wire = WireNumber{99U};
        packet.header().source = HostId{98U};
        packet.header().destination = HostId{97U};
        packet.header().endpoint = EndpointAddress{96U};
        packet.setIngressIndex(8U);
        ASSERT_TRUE(packet.initialize(size, connection, ControlFields::bits(QoS::kHigh)));
        EXPECT_EQ(packet.size(), size);
        EXPECT_EQ(packet.capacity(), 8U);
        EXPECT_EQ(packet.ingressIndex(), 0U);
        EXPECT_EQ(packet.header().wire, connection.wire);
        EXPECT_EQ(packet.header().source, connection.local_host);
        EXPECT_EQ(packet.header().destination, connection.remote_host);
        EXPECT_EQ(packet.header().endpoint, connection.endpoint);
        EXPECT_EQ(packet.header().qos(), QoS::kHigh);
        EXPECT_EQ(packet.header().transportType(), TransportType::kBits);
        EXPECT_FALSE(packet.header().hasExtensions());
        ASSERT_TRUE(packet.resize(8U));
        for (const auto byte : packet.payload()) EXPECT_EQ(byte, 0xAAU);
    }
}

// Rejected complete initialization preserves header, active bytes, size, capacity and ingress.
TEST(PacketTest, FailedConnectionInitializationLeavesPacketUnchanged) {
    SmallPacket packet{};
    constexpr ConnectionAddress original{WireNumber{7U}, HostId{2U}, HostId{3U}, EndpointAddress{123U}};
    ASSERT_TRUE(packet.initialize(8U, original, ControlFields::simple()));
    std::memset(packet.payload().data(), 0xAA, 8U);
    packet.setIngressIndex(4U);
    SmallPacket before{};
    ASSERT_TRUE(before.copyFrom(packet));
    EXPECT_FALSE(packet.initialize(9U, ConnectionAddress{}, ControlFields::bits()));
    EXPECT_EQ(packet.size(), before.size());
    EXPECT_EQ(packet.capacity(), 8U);
    EXPECT_EQ(packet.ingressIndex(), 4U);
    EXPECT_EQ(std::memcmp(packet.headerAndPayload().data(), before.headerAndPayload().data(), packet.totalSize()), 0);
}

// Copying between different capacities preserves destination storage and only copies active bytes.
TEST(PacketTest, CopiesContentsWithoutCapacityOrUnusedBytes) {
    TestPacket source{};
    SmallPacket output{};
    ASSERT_TRUE(source.initialize(3U, ControlFields{QoS::kCritical, true, TransportType::kBits}));
    source.header().wire = WireNumber{7U};
    source.header().source = HostId{2U};
    source.header().destination = HostId{3U};
    source.header().endpoint = EndpointAddress{123U};
    source.setIngressIndex(8U);
    std::memcpy(source.payload().data(), "abc", 3U);
    ASSERT_TRUE(output.resize(8U));
    std::memset(output.payload().data(), 0xAA, 8U);
    ASSERT_TRUE(output.copyFrom(source));
    EXPECT_EQ(output.capacity(), 8U);
    EXPECT_EQ(output.size(), 3U);
    EXPECT_EQ(output.ingressIndex(), 8U);
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
    output.setIngressIndex(4U);
    std::memset(output.payload().data(), 0xAA, 8U);
    EXPECT_FALSE(output.copyFrom(source));
    EXPECT_EQ(output.capacity(), 8U);
    EXPECT_EQ(output.size(), 8U);
    EXPECT_EQ(output.header().wire, WireNumber{17U});
    EXPECT_EQ(output.ingressIndex(), 4U);
    for (const auto byte : output.payload()) {
        EXPECT_EQ(byte, 0xAAU);
    }
}

// Self-copy is harmless; header-only packets replace the active size without touching storage.
TEST(PacketTest, CopiesSelfAndEmptyPayload) {
    SmallPacket packet{};
    ASSERT_TRUE(packet.resize(8U));
    packet.setIngressIndex(3U);
    std::memset(packet.payload().data(), 0xAA, 8U);
    ASSERT_TRUE(packet.copyFrom(packet));
    EXPECT_EQ(packet.ingressIndex(), 3U);
    EXPECT_EQ(packet.size(), 8U);
    EXPECT_EQ(packet.payload()[7], 0xAAU);
    TestPacket empty{};
    empty.header().wire = WireNumber{9U};
    ASSERT_TRUE(packet.copyFrom(empty));
    EXPECT_EQ(packet.size(), 0U);
    EXPECT_EQ(packet.ingressIndex(), 0U);
    EXPECT_EQ(packet.header().wire, WireNumber{9U});
    EXPECT_EQ(packet.capacity(), 8U);
}

// Ingress is host-local: resize preserves it; successful initialization/replies clear it.
TEST(PacketTest, IngressMetadataLifecycleAndSerialization) {
    SmallPacket packet{};
    EXPECT_EQ(sizeof(PacketBuffer), 12U);
    EXPECT_EQ(packet.ingressIndex(), 0U);
    ASSERT_TRUE(packet.initialize(3U, support::defaultControlFields()));
    std::memcpy(packet.payload().data(), "abc", 3U);
    uint8_t canonical[sizeof(Header) + 3U]{};
    std::memcpy(canonical, packet.headerAndPayload().data(), sizeof(canonical));
    packet.setIngressIndex(8U);
    EXPECT_EQ(std::memcmp(canonical, packet.headerAndPayload().data(), sizeof(canonical)), 0);
    ASSERT_TRUE(packet.resize(2U));
    EXPECT_EQ(packet.ingressIndex(), 8U);
    EXPECT_FALSE(packet.initialize(9U, support::defaultControlFields()));
    EXPECT_EQ(packet.ingressIndex(), 8U);
    ASSERT_TRUE(packet.initialize(3U, support::defaultControlFields()));
    EXPECT_EQ(packet.ingressIndex(), 0U);
    packet.setIngressIndex(2U);
    const Header request{packet.header()};
    ASSERT_TRUE(packet.initializeResponseTo(request, 1U, support::defaultControlFields()));
    EXPECT_EQ(packet.ingressIndex(), 0U);
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
