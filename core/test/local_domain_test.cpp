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

// Local-domain forwarding routes and dispatches a matching packet.
TEST_F(LocalDomainFixture, RoutesIntoDispatcher) {
    TestPacket packet{PacketBuilder{}.withEndpoint(kReceiverEndpoint).packet()};
    EXPECT_EQ(forwardDomain(packet), RouteResult::kForwarded);
    EXPECT_EQ(local_forwarder_.lastResult(), DispatchResult::kAccepted);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
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
