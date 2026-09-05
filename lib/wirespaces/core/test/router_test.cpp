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

// All eight ingress positions suppress their bit, including sparse/high-bit routes.
TEST_F(RouterTest, ExcludesSelectedIngressAndRejectsUnselectedIngress) {
    TestPacket packet{PacketBuilder{}.packet()};
    for (uint8_t index{1U}; index <= 8U; ++index) {
        route_entry_.egress_set = 0xFFU;
        packet.setIngressIndex(index);
        EXPECT_EQ(forward(packet), RouteResult::kForwarded);
        const EgressSet expected{static_cast<EgressSet>(0xFFU & ~(1U << (index - 1U)))};
        EXPECT_EQ(forward_spy_.lastEgressSet(), expected);
        route_entry_.egress_set = expected;
        forward_spy_.reset();
        EXPECT_EQ(forward(packet), RouteResult::kInvalidIngress);
        EXPECT_FALSE(forward_spy_.called());
    }
}

// Invalid metadata cannot cause an out-of-range shift or reach the Link callback.
TEST_F(RouterTest, RejectsInvalidIngressIndices) {
    TestPacket packet{PacketBuilder{}.packet()};
    for (const uint8_t index : {9U, 32U, 255U}) {
        packet.setIngressIndex(index);
        EXPECT_EQ(forward(packet), RouteResult::kInvalidIngress);
        EXPECT_FALSE(forward_spy_.called());
    }
}

// Incoming packets stop at a leaf, but local-origin compatibility still offers a zero mask.
TEST_F(RouterTest, StopsReceivedTrafficAtLeaf) {
    route_entry_.egress_set = 0x80U;
    TestPacket packet{PacketBuilder{}.packet()};
    packet.setIngressIndex(8U);
    EXPECT_EQ(forward(packet), RouteResult::kNoEgress);
    EXPECT_FALSE(forward_spy_.called());
}

// Ingress validity and membership gate both unicast and broadcast delivery; transit still forwards.
TEST_F(RouterTest, ReceiveValidatesBeforeForwardingAndLocalDelivery) {
    EndpointReceiverQueue<8U, 1U> receiver{};
    const DispatchTableEntry entry{support::kReceiverEndpoint, &receiver};
    const Dispatcher dispatcher{foundation::Span<const DispatchTableEntry>{&entry, 1U}};
    TestPacket packet{PacketBuilder{}.withEndpoint(support::kReceiverEndpoint).withPayload("test").packet()};
    TestPacket output{};
    route_entry_.egress_set = 0x81U;
    for (const uint8_t invalid : {0U, 2U, 9U, 255U}) {
        const auto result{router_.receive(packet, invalid, dispatcher)};
        EXPECT_EQ(result.routing, RouteResult::kInvalidIngress);
        EXPECT_EQ(result.delivery, DispatchResult::kNoEndpoint);
        EXPECT_FALSE(forward_spy_.called());
        EXPECT_FALSE(receiver.dequeue(output));
    }
    auto result{router_.receive(packet, 8U, dispatcher)};
    EXPECT_EQ(result.routing, RouteResult::kForwarded);
    EXPECT_EQ(forward_spy_.lastEgressSet(), 1U);
    EXPECT_EQ(result.delivery, DispatchResult::kAccepted);
    ASSERT_TRUE(receiver.dequeue(output));
    EXPECT_EQ(output.ingressIndex(), 8U);

    support::configureHostWithoutLocalWire();
    for (const HostId destination : {support::kLocalHostId, HostId{0xFFU}}) {
        packet.header().destination = destination;
        result = router_.receive(packet, 1U, dispatcher);
        EXPECT_EQ(result.routing, RouteResult::kForwarded);
        EXPECT_EQ(forward_spy_.lastEgressSet(), 0x80U);
        EXPECT_EQ(result.delivery, DispatchResult::kNoEndpoint);
        EXPECT_FALSE(receiver.dequeue(output));
    }
    support::configureDefaultHost();
    route_entry_.egress_set = 1U;
    forward_spy_.reset();
    result = router_.receive(packet, 1U, dispatcher);
    EXPECT_EQ(result.routing, RouteResult::kNoEgress);
    EXPECT_EQ(result.delivery, DispatchResult::kAccepted);
    EXPECT_FALSE(forward_spy_.called());
    ASSERT_TRUE(receiver.dequeue(output));
    packet.header().wire = kOtherWire;
    result = router_.receive(packet, 1U, dispatcher);
    EXPECT_EQ(result.routing, RouteResult::kNoRoute);
    EXPECT_EQ(result.delivery, DispatchResult::kNoEndpoint);
    EXPECT_FALSE(receiver.dequeue(output));
}

} // namespace
} // namespace wirespaces::test
