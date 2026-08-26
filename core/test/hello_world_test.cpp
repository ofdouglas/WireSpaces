/**
 * @file hello_world_test.cpp
 * @brief Integration coverage: sender stub → router → dispatch → mailbox on one host.
 */

#include "stub_hello_receiver.hpp"
#include "stub_hello_sender.hpp"

#include "dispatch.h"
#include "host.h"
#include "local_domain.h"
#include "ws_constants.h"

#include "support/constants.hpp"
#include "support/packet_builder.hpp"
#include "support/host_fixture.hpp"

#include <gtest/gtest.h>

namespace wirespaces::test {
namespace {

using support::kLocalHostId;
using support::kReceiverEndpoint;
using support::kRemoteHostId;
using support::kUnknownEndpoint;
using support::TestPacket;
using support::PacketBuilder;
using support::asPacketBuffer;

class HelloWorldTest : public support::DefaultHostFixture {
protected:
    void SetUp() override {
        support::DefaultHostFixture::SetUp();

        dispatch_entries_[0] = DispatchTableEntry{
            kHelloSenderEndpoint,
            EndpointReceiverHandle{nullptr, nullptr},
        };
        dispatch_entries_[1] = DispatchTableEntry{
            kHelloReceiverEndpoint,
            receiver_.receiverHandle(),
        };

        dispatch_table_ = DispatchTable{dispatch_entries_, 2U};

        forward_context_ = LocalDomainForwardContext{&dispatch_table_};
        route_entries_[0] = RouteTableEntry{WS_WIRE_LOCAL_DOMAIN, WS_EGRESS_SET_NONE};
        route_table_ = RouteTable{
            route_entries_,
            1U,
            local_domain_forward_impl,
            &forward_context_,
        };
        sender_ = HelloSender{&route_table_, kHelloReceiverEndpoint, kLocalHostId};
    }

    DispatchResult dispatchBuiltPacket(TestPacket& packet) {
        return dispatch_packet(&dispatch_table_, asPacketBuffer(&packet));
    }

    HelloReceiver receiver_{};
    DispatchTableEntry dispatch_entries_[2]{};
    DispatchTable dispatch_table_{dispatch_entries_, 2U};
    RouteTableEntry route_entries_[1]{};
    LocalDomainForwardContext forward_context_{};
    RouteTable route_table_{route_entries_, 0U, nullptr, nullptr};
    HelloSender sender_{nullptr, 0U, 0U};
};

// Verifies end-to-end delivery of one hello message into the receiver mailbox.
TEST_F(HelloWorldTest, DeliversHelloToReceiverMailbox) {
    sender_.sendHello();

    EXPECT_TRUE(receiver_.hasMessage());
    EXPECT_EQ(receiver_.generation(), 1U);
    EXPECT_EQ(receiver_.text(), "hello");
}

// Verifies the single-slot mailbox retains only the latest message.
TEST_F(HelloWorldTest, OverwritesMailboxOnSecondSend) {
    sender_.sendHello();
    sender_.sendMessage("world");

    EXPECT_TRUE(receiver_.hasMessage());
    EXPECT_EQ(receiver_.generation(), 2U);
    EXPECT_EQ(receiver_.text(), "world");
}

// Verifies direct dispatch rejects an endpoint that is not in the table.
TEST_F(HelloWorldTest, RejectsUnknownEndpoint) {
    TestPacket packet = PacketBuilder{}.withPayload("hello").withEndpoint(WS_NAMESPACE_USER0, kUnknownEndpoint).packet();

    EXPECT_EQ(dispatchBuiltPacket(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_FALSE(receiver_.hasMessage());
}

// Verifies direct dispatch rejects a unicast addressed to another host.
TEST_F(HelloWorldTest, RejectsWrongDestinationHost) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withDstHost(kRemoteHostId)
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatchBuiltPacket(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_FALSE(receiver_.hasMessage());
}

} // namespace
} // namespace wirespaces::test
