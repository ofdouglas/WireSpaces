/**
 * @file router_test.cpp
 * @brief Router coverage: wire lookup, forward callback, invalid inputs.
 */

#include <gtest/gtest.h>

#include "core/wirespaces_core.hpp"

#include "support/packet_builder.hpp"
#include "support/router_spy.hpp"

namespace wirespaces::test {
namespace {

using support::asPacketBuffer;
using support::PacketBuilder;
using support::RouteTableFixture;
using support::TestPacket;
using wirespaces::kDispatchNoEndpoint;
using wirespaces::kDispatchOk;
using wirespaces::PacketBuffer;

class RouterTest : public RouteTableFixture {
protected:
    void SetUp() override {
        RouteTableFixture::SetUp();
        buildRouteTable(WS_WIRE_LOCAL_DOMAIN, WS_EGRESS_SET_NONE);
    }
};

TEST_F(RouterTest, ForwardsPacketOnRegisteredWire) {
    TestPacket packet = PacketBuilder{}.withWire(WS_WIRE_LOCAL_DOMAIN).withPayload("route").packet();

    EXPECT_EQ(forward(packet), kDispatchOk);
    EXPECT_TRUE(forward_spy_.called());
    EXPECT_EQ(forward_spy_.lastPacket(), asPacketBuffer(&packet));
    EXPECT_EQ(forward_spy_.lastEgressSet(), WS_EGRESS_SET_NONE);
}

TEST_F(RouterTest, RejectsUnregisteredWire) {
    TestPacket packet = PacketBuilder{}.withWire(support::kOtherWire).withPayload("route").packet();

    EXPECT_EQ(forward(packet), kDispatchNoEndpoint);
    EXPECT_FALSE(forward_spy_.called());
}

TEST_F(RouterTest, RejectsWhenForwardCallbackMissing) {
    route_table_.forward = nullptr;

    TestPacket packet = PacketBuilder{}.withWire(WS_WIRE_LOCAL_DOMAIN).packet();
    EXPECT_EQ(forward(packet), kDispatchNoEndpoint);
    EXPECT_FALSE(forward_spy_.called());
}

TEST_F(RouterTest, RejectsNullTableOrPacket) {
    TestPacket packet = PacketBuilder{}.withWire(WS_WIRE_LOCAL_DOMAIN).packet();
    const PacketBuffer* packet_buffer = asPacketBuffer(&packet);

    EXPECT_EQ(ws_router_forward_packet(nullptr, packet_buffer), kDispatchNoEndpoint);
    EXPECT_EQ(ws_router_forward_packet(&route_table_, nullptr), kDispatchNoEndpoint);
}

} // namespace
} // namespace wirespaces::test
