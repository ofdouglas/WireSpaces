/**
 * @file local_domain_test.cpp
 * @brief Local-domain glue: router → dispatch integration and ingress validation.
 */

#include <gtest/gtest.h>

#include "core/wirespaces_core.hpp"

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
using wirespaces::kDispatchNoEndpoint;
using wirespaces::kDispatchOk;
using wirespaces::kNamespaceUser0;
using wirespaces::PacketBuffer;

class LocalDomainTest : public LocalDomainFixture {};

TEST_F(LocalDomainTest, ForwardsLocalWirePacketToDispatch) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("domain")
                            .withEndpoint(kNamespaceUser0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(forwardDomain(packet), kDispatchOk);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
}

TEST_F(LocalDomainTest, ForwardImplDispatchesRegardlessOfEgressSet) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("egress")
                            .withEndpoint(kNamespaceUser0, kReceiverEndpoint)
                            .packet();

    ws_local_domain_forward_impl(&forward_context_, asPacketBuffer(&packet), 0xFFU);

    EXPECT_EQ(recorder_.invocationCount(), 1U);
}

TEST_F(LocalDomainTest, RejectsUnknownWireAtRouter) {
    TestPacket packet = PacketBuilder{}
                            .withWire(support::kOtherWire)
                            .withPayload("nowhere")
                            .withEndpoint(kNamespaceUser0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(forwardDomain(packet), kDispatchNoEndpoint);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

TEST_F(LocalDomainTest, RejectsNullRouteTableOrPacket) {
    TestPacket packet = PacketBuilder{}.withPayload("null").packet();
    const PacketBuffer* packet_buffer = asPacketBuffer(&packet);

    EXPECT_EQ(ws_local_domain_forward(nullptr, packet_buffer), kDispatchNoEndpoint);
    EXPECT_EQ(ws_local_domain_forward(&route_table_, nullptr), kDispatchNoEndpoint);
}

} // namespace
} // namespace wirespaces::test
