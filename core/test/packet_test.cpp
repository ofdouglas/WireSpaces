/**
 * @file packet_test.cpp
 * @brief Packet helper coverage: init, payload pointer, endpoint delegate.
 */

#include <gtest/gtest.h>

#include "packet.h"

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::TestPacket;
using support::PacketBuilder;
using support::asPacketBuffer;

// Verifies packet_init stores length and applies control fields to the header.
TEST(PacketInitTest, InitializesLengthAndControlFields) {
    TestPacket packet{};
    const ControlFields fields{QOS_BACKGROUND, true, TRANSPORT_SIMPLE};
    packet_init(asPacketBuffer(&packet), 12U, fields);

    EXPECT_EQ(asPacketBuffer(&packet)->length, 12U);
    EXPECT_EQ(header_get_qos(&packet.header), QOS_BACKGROUND);
    EXPECT_TRUE(header_get_has_extensions(&packet.header));
    EXPECT_EQ(header_get_transport_type(&packet.header), TRANSPORT_SIMPLE);
}

// Verifies packet_payload_bytes points immediately after the packet header struct.
TEST(PacketPayloadTest, PayloadBytesFollowHeader) {
    TestPacket packet = PacketBuilder{}.withPayload("payload").packet();
    uint8_t* payload = packet_payload_bytes(asPacketBuffer(&packet));

    EXPECT_NE(payload, nullptr);
    EXPECT_EQ(payload, packet.data);
    EXPECT_STREQ(reinterpret_cast<char*>(payload), "payload");
}

// Verifies packet_set_endpoint delegates to header_set_endpoint.
TEST(PacketEndpointTest, SetEndpointDelegatesToHeader) {
    TestPacket packet{};
    packet_set_endpoint(&packet.header, WS_NAMESPACE_COMMON, 0x0042U);

    EXPECT_EQ(header_get_namespace(&packet.header), WS_NAMESPACE_COMMON);
    EXPECT_EQ(header_get_endpoint_id(&packet.header), 0x0042U);
}

// Verifies null packet helpers return safe results without crashing.
TEST(PacketNullSafetyTest, HandlesNullPointers) {
    packet_init(nullptr, 0U, ControlFields{QOS_NORMAL, false, TRANSPORT_SIMPLE});
    EXPECT_EQ(packet_payload_bytes(nullptr), nullptr);
    packet_set_endpoint(nullptr, WS_NAMESPACE_USER0, 0x0001U);
    SUCCEED();
}

} // namespace
} // namespace wirespaces::test
