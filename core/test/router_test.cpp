/**
 * @file router_test.cpp
 * @brief Router coverage: wire lookup, forward callback, invalid inputs.
 *
 * Equivalence classes:
 * - Wire number: registered, unregistered
 * - Forward callback: present, null
 * - Table / packet: null pointers
 */

#include <gtest/gtest.h>

#include "router.h"
#include "ws_constants.h"

#include "support/packet_builder.hpp"
#include "support/router_spy.hpp"

namespace wirespaces::test {
namespace {

using support::asPacketBuffer;
using support::PacketBuilder;
using support::RouteTableFixture;
using support::TestPacket;

class RouterTest : public RouteTableFixture {
protected:
    void SetUp() override {
        RouteTableFixture::SetUp();
        buildRouteTable(WS_WIRE_LOCAL_DOMAIN, WS_EGRESS_SET_NONE);
    }
};

// Verifies a registered wire invokes the forward callback with the route egress set.
TEST_F(RouterTest, ForwardsPacketOnRegisteredWire) {
    TestPacket packet = PacketBuilder{}.withWire(WS_WIRE_LOCAL_DOMAIN).withPayload("route").packet();

    EXPECT_EQ(forward(packet), DISPATCH_OK);
    EXPECT_TRUE(forward_spy_.called());
    EXPECT_EQ(forward_spy_.lastPacket(), asPacketBuffer(&packet));
    EXPECT_EQ(forward_spy_.lastEgressSet(), WS_EGRESS_SET_NONE);
}

// Verifies an unregistered wire number is rejected without forwarding.
TEST_F(RouterTest, RejectsUnregisteredWire) {
    TestPacket packet = PacketBuilder{}.withWire(support::kOtherWire).withPayload("route").packet();

    EXPECT_EQ(forward(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_FALSE(forward_spy_.called());
}

// Verifies a route table with a null forward callback rejects all packets.
TEST_F(RouterTest, RejectsWhenForwardCallbackMissing) {
    route_table_.forward = nullptr;

    TestPacket packet = PacketBuilder{}.withWire(WS_WIRE_LOCAL_DOMAIN).packet();
    EXPECT_EQ(forward(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_FALSE(forward_spy_.called());
}

// Verifies null route table and packet pointers are rejected.
TEST_F(RouterTest, RejectsNullTableOrPacket) {
    TestPacket packet = PacketBuilder{}.withWire(WS_WIRE_LOCAL_DOMAIN).packet();
    PacketBufferHeader* packet_buffer = asPacketBuffer(&packet);

    EXPECT_EQ(router_forward_packet(nullptr, packet_buffer), DISPATCH_NO_ENDPOINT);
    EXPECT_EQ(router_forward_packet(&route_table_, nullptr), DISPATCH_NO_ENDPOINT);
}

} // namespace
} // namespace wirespaces::test
