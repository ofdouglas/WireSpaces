/**
 * @file local_domain_test.cpp
 * @brief Local-domain glue: router → dispatch integration and ingress validation.
 */

#include <gtest/gtest.h>

#include "local_domain.h"
#include "ws_constants.h"

#include "support/constants.hpp"
#include "support/local_domain_fixture.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::asPacketBuffer;
using support::kReceiverEndpoint;
using support::LocalDomainFixture;
using support::PacketBuilder;
using support::TestPacket;

class LocalDomainTest : public LocalDomainFixture {};

// Verifies local_domain_forward routes a local-wire packet into dispatch.
TEST_F(LocalDomainTest, ForwardsLocalWirePacketToDispatch) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("domain")
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(forwardDomain(packet), DISPATCH_OK);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
}

// Verifies local_domain_forward_impl ignores egress and still dispatches locally.
TEST_F(LocalDomainTest, ForwardImplDispatchesRegardlessOfEgressSet) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("egress")
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    local_domain_forward_impl(&forward_context_, asPacketBuffer(&packet), 0xFFU);

    EXPECT_EQ(recorder_.invocationCount(), 1U);
}

// Verifies unknown wire numbers are rejected at the router boundary.
TEST_F(LocalDomainTest, RejectsUnknownWireAtRouter) {
    TestPacket packet = PacketBuilder{}
                            .withWire(support::kOtherWire)
                            .withPayload("nowhere")
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(forwardDomain(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

// Verifies null route table or packet is rejected by local_domain_forward.
TEST_F(LocalDomainTest, RejectsNullRouteTableOrPacket) {
    TestPacket packet = PacketBuilder{}.withPayload("null").packet();
    PacketBufferHeader* packet_buffer = asPacketBuffer(&packet);

    EXPECT_EQ(local_domain_forward(nullptr, packet_buffer), DISPATCH_NO_ENDPOINT);
    EXPECT_EQ(local_domain_forward(&route_table_, nullptr), DISPATCH_NO_ENDPOINT);
}

} // namespace
} // namespace wirespaces::test
