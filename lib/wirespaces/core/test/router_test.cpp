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

// Three-link folds cover sticky partial acceptance and failure precedence in every order.
TEST(ForwardingTest, AggregatesThreeLinksWithoutLosingPartialAcceptance) {
    using R = RouteResult;
    for (unsigned a = 0; a < 4; ++a) {
        for (unsigned b = 0; b < 4; ++b) {
            for (unsigned c = 0; c < 4; ++c) {
                auto result{R::kNoEgress};
                unsigned accepted{0U};
                unsigned worst{0U};
                for (unsigned outcome : {a, b, c}) {
                    result = combineAdmission(result, static_cast<LinkAdmission>(outcome));
                    accepted += outcome == 0U;
                    if (outcome > worst) worst = outcome;
                }
                const R failures[]{R::kAccepted, R::kFull, R::kTooLarge, R::kRejected};
                const R expected{accepted == 3U ? R::kAccepted :
                    accepted != 0U ? R::kPartial : failures[worst]};
                EXPECT_EQ(result, expected);
            }
        }
    }
}

/** Test fan-out with controlled admission, without retaining caller-owned bytes. */
class AdmissionForwarder final : public PacketForwarder {
public:
    RouteResult forward(const PacketBuffer&, InterfaceSet selected) noexcept override {
        ++calls;
        seen = selected;
        return result;
    }
    RouteResult result{RouteResult::kAccepted};
    InterfaceSet seen{0U};
    unsigned calls{0U};
};

// Coarse admission propagates unchanged, with one fan-out attempt and no Router retries.
TEST(RouterAdmissionTest, PropagatesCoarseResultsWithoutRetries) {
    static_assert(sizeof(RouteResult) == 1U);
    const RouteTableEntry routes[]{{kLocalWire, 7U}};
    AdmissionForwarder egress{};
    Router router{foundation::Span<const RouteTableEntry>{routes}, egress};
    TestPacket packet{PacketBuilder{}.withWire(kLocalWire).packet()};
    for (auto outcome : {RouteResult::kAccepted, RouteResult::kPartial, RouteResult::kFull,
                         RouteResult::kTooLarge, RouteResult::kRejected}) {
        egress.result = outcome;
        egress.calls = 0U;
        EXPECT_EQ(router.forward(packet), outcome);
        EXPECT_EQ(egress.calls, 1U);
        EXPECT_EQ(egress.seen, 7U);
    }
}

// Outbound failures never suppress otherwise-valid local delivery.
TEST(RouterAdmissionTest, DeliversDespiteOutboundFailure) {
    const RouteTableEntry routes[]{{kLocalWire, 7U}};
    AdmissionForwarder egress{};
    Router router{foundation::Span<const RouteTableEntry>{routes}, egress};
    TestPacket packet{PacketBuilder{}.withWire(kLocalWire).packet()};
    EndpointReceiverQueue<8U, 1U> queue{};
    const DispatchTableEntry endpoints[]{{packet.header().endpoint, &queue}};
    Dispatcher dispatcher{foundation::Span<const DispatchTableEntry>{endpoints}};
    const HostInfo host{packet.header().destination, 1U, {kLocalWire}};
    for (auto outcome : {RouteResult::kPartial, RouteResult::kFull,
                         RouteResult::kTooLarge, RouteResult::kRejected}) {
        egress.result = outcome;
        const auto incoming = router.receive(packet, 1U, dispatcher, host);
        EXPECT_EQ(incoming.routing, outcome);
        EXPECT_EQ(incoming.delivery, DispatchResult::kAccepted);
        EXPECT_EQ(egress.seen, 6U);
        TestPacket output{};
        ASSERT_TRUE(queue.dequeue(output));
    }
}

// A packet on a configured wire is offered to the PacketForwarder.
TEST_F(RouterTest, ForwardsPacketOnRegisteredWire) {
    TestPacket packet{PacketBuilder{}.withWire(kLocalWire).withPayload("route").packet()};
    EXPECT_EQ(forward(packet), RouteResult::kAccepted);
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
        route_entry_.wire_interfaces = 0xFFU;
        packet.setIngressIndex(index);
        EXPECT_EQ(forward(packet), RouteResult::kAccepted);
        const InterfaceSet expected{static_cast<InterfaceSet>(0xFFU & ~(1U << (index - 1U)))};
        EXPECT_EQ(forward_spy_.lastEgressSet(), expected);
        route_entry_.wire_interfaces = expected;
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
    route_entry_.wire_interfaces = 0x80U;
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
    route_entry_.wire_interfaces = 0x81U;
    for (const uint8_t invalid : {0U, 2U, 9U, 255U}) {
        const auto result{router_.receive(packet, invalid, dispatcher)};
        EXPECT_EQ(result.routing, RouteResult::kInvalidIngress);
        EXPECT_EQ(result.delivery, DispatchResult::kNoEndpoint);
        EXPECT_FALSE(forward_spy_.called());
        EXPECT_FALSE(receiver.dequeue(output));
    }
    auto result{router_.receive(packet, 8U, dispatcher)};
    EXPECT_EQ(result.routing, RouteResult::kAccepted);
    EXPECT_EQ(forward_spy_.lastEgressSet(), 1U);
    EXPECT_EQ(result.delivery, DispatchResult::kAccepted);
    ASSERT_TRUE(receiver.dequeue(output));
    EXPECT_EQ(output.ingressIndex(), 8U);

    support::configureHostWithoutLocalWire();
    for (const HostId destination : {support::kLocalHostId, HostId{0xFFU}}) {
        packet.header().destination = destination;
        result = router_.receive(packet, 1U, dispatcher);
        EXPECT_EQ(result.routing, RouteResult::kAccepted);
        EXPECT_EQ(forward_spy_.lastEgressSet(), 0x80U);
        EXPECT_EQ(result.delivery, DispatchResult::kNoEndpoint);
        EXPECT_FALSE(receiver.dequeue(output));
    }
    support::configureDefaultHost();
    route_entry_.wire_interfaces = 1U;
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
