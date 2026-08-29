/**
 * @file router_test.cpp
 * @brief Route lookup and PacketForwarder invocation coverage.
 */

#include <gtest/gtest.h>

#include "support/packet_builder.hpp"
#include "support/router_spy.hpp"

namespace wirespaces::test {
namespace {

using support::kLocalWire;
using support::kOtherWire;
using support::PacketBuilder;
using support::RouteTableFixture;
using support::TestPacket;

class RouterTest : public RouteTableFixture {};

// A packet on a configured wire is offered to the PacketForwarder.
TEST_F(RouterTest, ForwardsPacketOnRegisteredWire) {
    TestPacket packet{PacketBuilder{}.withWire(kLocalWire).withPayload("route").packet()};
    EXPECT_EQ(forward(packet), RouteResult::kForwarded);
    EXPECT_TRUE(forward_spy_.called());
    EXPECT_EQ(forward_spy_.lastPacket(), &packet);
    EXPECT_EQ(forward_spy_.lastEgressSet(), kNoEgress);
}

// A packet on an unconfigured wire returns without calling the forwarder.
TEST_F(RouterTest, RejectsUnregisteredWire) {
    TestPacket packet{PacketBuilder{}.withWire(kOtherWire).packet()};
    EXPECT_EQ(forward(packet), RouteResult::kNoRoute);
    EXPECT_FALSE(forward_spy_.called());
}

} // namespace
} // namespace wirespaces::test
