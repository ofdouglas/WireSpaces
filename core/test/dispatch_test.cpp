/**
 * @file dispatch_test.cpp
 * @brief Endpoint dispatch coverage: addressing, table lookup, callback invocation.
 */

#include <gtest/gtest.h>

#include <runtime/core.hpp>

#include "support/constants.hpp"
#include "support/dispatch_recorder.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test {
namespace {

using support::asPacketBuffer;
using support::DispatchTableFixture;
using support::kReceiverEndpoint;
using support::kRemoteHostId;
using support::kSenderEndpoint;
using support::kUnknownEndpoint;
using support::PacketBuilder;
using support::TestPacket;
using wirespaces::EndpointReceiver;
using wirespaces::kDispatchNoEndpoint;
using wirespaces::kDispatchOk;
using wirespaces::kNamespaceUser0;
using wirespaces::PacketBuffer;

class DispatchTest : public DispatchTableFixture {
protected:
    void SetUp() override {
        DispatchTableFixture::SetUp();
        registerEndpoint(kReceiverEndpoint, recorder_.handle());
    }
};

TEST_F(DispatchTest, DeliversToRegisteredEndpoint) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withEndpoint(kNamespaceUser0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), kDispatchOk);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
    EXPECT_EQ(recorder_.lastPacket(), asPacketBuffer(&packet));
}

TEST_F(DispatchTest, RejectsUnknownEndpoint) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withEndpoint(kNamespaceUser0, kUnknownEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), kDispatchNoEndpoint);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

TEST_F(DispatchTest, RejectsUnicastToRemoteHost) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withDstHost(kRemoteHostId)
                            .withEndpoint(kNamespaceUser0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), kDispatchNoEndpoint);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

TEST_F(DispatchTest, AcceptsBroadcastOnMemberWire) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withDstHost(WS_HOST_BROADCAST)
                            .withWire(WS_WIRE_LOCAL_DOMAIN)
                            .withEndpoint(kNamespaceUser0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), kDispatchOk);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
}

TEST_F(DispatchTest, RejectsBroadcastOnNonMemberWire) {
    support::configureHostWithoutLocalWire();

    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withDstHost(WS_HOST_BROADCAST)
                            .withWire(WS_WIRE_LOCAL_DOMAIN)
                            .withEndpoint(kNamespaceUser0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), kDispatchNoEndpoint);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

TEST_F(DispatchTest, RejectsEndpointWithNullCallback) {
    registerEndpoint(kSenderEndpoint, EndpointReceiver{nullptr, nullptr});

    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withEndpoint(kNamespaceUser0, kSenderEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), kDispatchNoEndpoint);
}

TEST_F(DispatchTest, RejectsNullTableOrPacket) {
    TestPacket packet = PacketBuilder{}.withPayload("hello").packet();
    const PacketBuffer* packet_buffer = asPacketBuffer(&packet);

    EXPECT_EQ(ws_dispatch_packet(nullptr, packet_buffer), kDispatchNoEndpoint);
    EXPECT_EQ(ws_dispatch_packet(&dispatch_table_, nullptr), kDispatchNoEndpoint);
}

} // namespace
} // namespace wirespaces::test
