/**
 * @file hello_world_test.cpp
 * @brief GoogleTest coverage for local-domain hello-world messaging.
 */

#include "stub_hello_receiver.hpp"
#include "stub_hello_sender.hpp"

#include "dispatch.h"
#include "host.h"
#include "local_domain.h"
#include "ws_constants.h"

#include <gtest/gtest.h>

namespace wirespaces::test {
namespace {

class HelloWorldTest : public ::testing::Test {
protected:
    void SetUp() override {
        HostInfo host_info{};
        host_info.host_id = 0x01U;
        host_info.num_wires = 1U;
        host_info.wires[0] = WS_WIRE_LOCAL_DOMAIN;
        host_set_info(&host_info);

        dispatch_entries_[0] = DispatchTableEntry{
            kHelloSenderEndpoint,
            EndpointReceiverHandle{nullptr, nullptr},
        };
        dispatch_entries_[1] = DispatchTableEntry{
            kHelloReceiverEndpoint,
            receiver_.receiverHandle(),
        };
    }

    HelloReceiver receiver_{};
    DispatchTableEntry dispatch_entries_[2]{};
    DispatchTable dispatch_table_{dispatch_entries_, 2U};
    RouteTableEntry route_entries_[1]{WS_WIRE_LOCAL_DOMAIN, WS_EGRESS_SET_NONE};
    LocalDomainForwardContext forward_context_{&dispatch_table_};
    RouteTable route_table_{route_entries_, 1U, local_domain_forward_impl, &forward_context_};
    HelloSender sender_{&route_table_, kHelloReceiverEndpoint, 0x01U};
};

TEST_F(HelloWorldTest, DeliversHelloToReceiverMailbox) {
    sender_.sendHello();

    EXPECT_TRUE(receiver_.hasMessage());
    EXPECT_EQ(receiver_.generation(), 1U);
    EXPECT_EQ(receiver_.text(), "hello");
}

TEST_F(HelloWorldTest, OverwritesMailboxOnSecondSend) {
    sender_.sendHello();
    sender_.sendMessage("world");

    EXPECT_TRUE(receiver_.hasMessage());
    EXPECT_EQ(receiver_.generation(), 2U);
    EXPECT_EQ(receiver_.text(), "world");
}

TEST_F(HelloWorldTest, RejectsUnknownEndpoint) {
    HelloPacket packet{};
    const ControlFields control_fields{QOS_NORMAL, false, TRANSPORT_SIMPLE};
    auto* packet_header = reinterpret_cast<PacketBufferHeader*>(&packet);
    packet_init(packet_header, 5U, control_fields);
    packet.header.wire_number = WS_WIRE_LOCAL_DOMAIN;
    packet.header.src_host = 0x01U;
    packet.header.dst_host = 0x01U;
    packet_set_endpoint(&packet.header, WS_NAMESPACE_USER0, 0x0099U);

    const DispatchResult result = dispatch_packet(&dispatch_table_, packet_header);
    EXPECT_EQ(result, DISPATCH_NO_ENDPOINT);
    EXPECT_FALSE(receiver_.hasMessage());
}

TEST_F(HelloWorldTest, RejectsWrongDestinationHost) {
    HelloPacket packet{};
    const ControlFields control_fields{QOS_NORMAL, false, TRANSPORT_SIMPLE};
    auto* packet_header = reinterpret_cast<PacketBufferHeader*>(&packet);
    packet_init(packet_header, 5U, control_fields);
    packet.header.wire_number = WS_WIRE_LOCAL_DOMAIN;
    packet.header.src_host = 0x01U;
    packet.header.dst_host = 0x02U;
    packet_set_endpoint(&packet.header, WS_NAMESPACE_USER0, kHelloReceiverEndpoint);

    const DispatchResult result = dispatch_packet(&dispatch_table_, packet_header);
    EXPECT_EQ(result, DISPATCH_NO_ENDPOINT);
    EXPECT_FALSE(receiver_.hasMessage());
}

} // namespace
} // namespace wirespaces::test
