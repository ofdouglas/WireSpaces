/**
 * @file packet_test.cpp
 * @brief Packet helper coverage: init, payload pointer, endpoint delegate.
 */

#include <gtest/gtest.h>

#include "core/wirespaces_core.hpp"

#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::asPacketBuffer;
using support::PacketBuilder;
using support::TestPacket;
using wirespaces::ControlFields;
using wirespaces::kNamespaceCommon;
using wirespaces::kNamespaceUser0;
using wirespaces::kQoSBackground;
using wirespaces::kQoSNormal;
using wirespaces::kTransportSimple;

TEST(PacketInitTest, InitializesSizeAndControlFields) {
    TestPacket packet{};
    const ControlFields fields{kQoSBackground, true, kTransportSimple};
    ws_packet_init(asPacketBuffer(&packet), WS_MAILBOX_DEFAULT_CAPACITY, 12U, fields);

    EXPECT_EQ(asPacketBuffer(&packet)->capacity, WS_MAILBOX_DEFAULT_CAPACITY);
    EXPECT_EQ(asPacketBuffer(&packet)->size, 12U);
    EXPECT_EQ(ws_header_get_qos(&packet.header), kQoSBackground);
    EXPECT_TRUE(ws_header_get_has_extensions(&packet.header));
    EXPECT_EQ(ws_header_get_transport_type(&packet.header), kTransportSimple);
}

TEST(PacketPayloadTest, PayloadBytesFollowHeader) {
    TestPacket packet = PacketBuilder{}.withPayload("payload").packet();
    const uint8_t* payload = ws_packet_payload_bytes(asPacketBuffer(&packet));

    EXPECT_NE(payload, nullptr);
    EXPECT_EQ(payload, packet.data);
    EXPECT_STREQ(reinterpret_cast<const char*>(payload), "payload");
}

TEST(PacketEndpointTest, SetEndpointDelegatesToHeader) {
    TestPacket packet{};
    ws_packet_set_endpoint(&packet.header, kNamespaceCommon, 0x0042U);

    EXPECT_EQ(ws_header_get_namespace(&packet.header), kNamespaceCommon);
    EXPECT_EQ(ws_header_get_endpoint_id(&packet.header), 0x0042U);
}

TEST(PacketNullSafetyTest, HandlesNullPointers) {
    ws_packet_init(
        nullptr,
        WS_MAILBOX_DEFAULT_CAPACITY,
        0U,
        ControlFields{kQoSNormal, false, kTransportSimple});
    EXPECT_EQ(ws_packet_payload_bytes(nullptr), nullptr);
    EXPECT_EQ(ws_packet_payload_mut(nullptr), nullptr);
    ws_packet_set_endpoint(nullptr, kNamespaceUser0, 0x0001U);
    SUCCEED();
}

} // namespace
} // namespace wirespaces::test
