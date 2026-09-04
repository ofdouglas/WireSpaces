/**
 * @file hello_world_test.cpp
 * @brief Sender to Router to Dispatcher to snapshot integration coverage.
 */

#include <gtest/gtest.h>

#include "stub_hello_receiver.hpp"
#include "stub_hello_sender.hpp"
#include "support/constants.hpp"
#include "support/host_fixture.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::kLocalHostId;
using support::kLocalWire;
using support::kRemoteHostId;
using support::kUnknownEndpoint;
using support::PacketBuilder;
using support::TestPacket;

class HelloWorldTest : public support::DefaultHostFixture {
protected:
    HelloReceiver receiver_{};
    DispatchTableEntry dispatch_entry_{kHelloReceiverEndpoint, &receiver_};
    Dispatcher dispatcher_{foundation::Span<const DispatchTableEntry>{&dispatch_entry_, 1U}};
    LocalDomainForwarder local_forwarder_{dispatcher_};
    RouteTableEntry route_entry_{kLocalWire, kNoEgress};
    Router router_{foundation::Span<const RouteTableEntry>{&route_entry_, 1U}, local_forwarder_};
    HelloSender sender_{router_, kHelloReceiverEndpoint, kLocalHostId};
};

// The complete local-domain path stores the sent payload in the receiver snapshot.
TEST_F(HelloWorldTest, DeliversHelloToReceiverSnapshot) {
    sender_.sendHello();
    EXPECT_TRUE(receiver_.hasMessage());
    EXPECT_EQ(receiver_.generation(), 1U);
    EXPECT_EQ(receiver_.text(), "hello");
}

// A second send replaces the snapshot and advances its generation.
TEST_F(HelloWorldTest, OverwritesSnapshotOnSecondSend) {
    sender_.sendHello();
    sender_.sendMessage("world");
    EXPECT_EQ(receiver_.generation(), 2U);
    EXPECT_EQ(receiver_.text(), "world");
}

// Direct dispatch reports an unknown endpoint without modifying the receiver.
TEST_F(HelloWorldTest, RejectsUnknownEndpoint) {
    TestPacket packet{PacketBuilder{}.withEndpoint(kUnknownEndpoint).packet()};
    EXPECT_EQ(dispatcher_.dispatch(packet), DispatchResult::kNoEndpoint);
    EXPECT_FALSE(receiver_.hasMessage());
}

// Direct dispatch rejects a packet addressed to a different host.
TEST_F(HelloWorldTest, RejectsWrongDestinationHost) {
    TestPacket packet{PacketBuilder{}
                          .withDestination(kRemoteHostId)
                          .withEndpoint(kHelloReceiverEndpoint)
                          .packet()};
    EXPECT_EQ(dispatcher_.dispatch(packet), DispatchResult::kNoEndpoint);
}

} // namespace
} // namespace wirespaces::test
