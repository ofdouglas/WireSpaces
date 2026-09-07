/**
 * @file local_domain_test.cpp
 * @brief Router to local Dispatcher integration coverage.
 */

#include <gtest/gtest.h>

#include "support/local_domain_fixture.hpp"

namespace wirespaces::test {
namespace {

using support::kOtherWire;
using support::kReceiverEndpoint;
using support::LocalDomainFixture;
using support::PacketBuilder;
using support::TestPacket;

// Explicit context routing/delivery uses its own identity even with an unrelated legacy global.
TEST(DomainContextTest, BindsIdentityAndIngressWithoutGlobalRegistration) {
    const RouteTableEntry routes[]{{WireNumber{7U}, 1U}};
    support::DispatchRecorder receiver{};
    const DispatchTableEntry entries[]{{kReceiverEndpoint, &receiver}};
    Dispatcher dispatcher{foundation::Span<const DispatchTableEntry>{entries}};
    // The selected ingress is the only egress, so the forwarder must not be called.
    LocalDispatchForwarder unused_forwarder{dispatcher};
    DomainContext domain{HostInfo{HostId{12U}, 1U, {WireNumber{7U}}},
                         foundation::Span<const RouteTableEntry>{routes}, unused_forwarder};
    const HostInfo previous{localHostInfo()};
    setLocalHostInfo(HostInfo{HostId{99U}, 0U, {}});
    TestPacket packet{};
    const auto connection{domain.connection({WireNumber{7U}, HostId{12U}}, kReceiverEndpoint)};
    EXPECT_EQ(connection.local_host, HostId{12U});
    EXPECT_TRUE(packet.initialize(0U, connection, ControlFields::simple()));
    EXPECT_EQ(domain.receive(packet, 1U, dispatcher).delivery, DispatchResult::kAccepted);
    packet.header().destination = HostId{99U};
    EXPECT_EQ(domain.receive(packet, 1U, dispatcher).delivery, DispatchResult::kNoEndpoint);
    packet.header().destination = HostId{kBroadcastHostValue};
    EXPECT_EQ(domain.receive(packet, 1U, dispatcher).delivery, DispatchResult::kAccepted);
    EXPECT_EQ(domain.receive(packet, 2U, dispatcher).routing, RouteResult::kInvalidIngress);
    packet.header().wire = WireNumber{8U};
    EXPECT_EQ(domain.receive(packet, 1U, dispatcher).routing, RouteResult::kNoRoute);
    EXPECT_EQ(receiver.invocationCount(), 2U);
    EXPECT_EQ(localHostInfo().id, HostId{99U});
    setLocalHostInfo(previous);
}

// Local-domain forwarding routes and dispatches a matching packet.
TEST_F(LocalDomainFixture, RoutesIntoDispatcher) {
    TestPacket packet{PacketBuilder{}.withEndpoint(kReceiverEndpoint).packet()};
    EXPECT_EQ(forwardDomain(packet), RouteResult::kAccepted);
    EXPECT_EQ(local_forwarder_.lastResult(), DispatchResult::kAccepted);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
}

// A zero-egress local-domain route still reports actual queue admission, not vacuous success.
TEST_F(LocalDomainFixture, ReportsLocalQueueFullAndRejection) {
    EndpointReceiverQueue<8U, 1U> queue{};
    dispatch_entry_.receiver = &queue;
    TestPacket packet{PacketBuilder{}.withEndpoint(kReceiverEndpoint).packet()};
    EXPECT_EQ(forwardDomain(packet), RouteResult::kAccepted);
    EXPECT_EQ(forwardDomain(packet), RouteResult::kFull);
    packet.header().destination = HostId{99U};
    EXPECT_EQ(forwardDomain(packet), RouteResult::kRejected);
}

// A missing route never reaches local dispatch.
TEST_F(LocalDomainFixture, RejectsUnregisteredWireBeforeDispatch) {
    TestPacket packet{PacketBuilder{}
                          .withWire(kOtherWire)
                          .withEndpoint(kReceiverEndpoint)
                          .packet()};
    EXPECT_EQ(forwardDomain(packet), RouteResult::kNoRoute);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

} // namespace
} // namespace wirespaces::test
