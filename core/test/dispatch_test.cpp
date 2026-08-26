/**
 * @file dispatch_test.cpp
 * @brief Endpoint dispatch coverage: addressing, table lookup, callback invocation.
 *
 * Equivalence classes:
 * - Destination host: local unicast, remote unicast, broadcast on member wire, broadcast off wire
 * - Endpoint: registered, unknown, registered with null callback
 * - Table / packet: null pointers, empty table
 */

#include <gtest/gtest.h>

#include "dispatch.h"
#include "ws_constants.h"

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

class DispatchTest : public DispatchTableFixture {
protected:
    void SetUp() override {
        DispatchTableFixture::SetUp();
        registerEndpoint(kReceiverEndpoint, recorder_.handle());
    }
};

// Verifies dispatch delivers to a registered endpoint and invokes its callback.
TEST_F(DispatchTest, DeliversToRegisteredEndpoint) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), DISPATCH_OK);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
    EXPECT_EQ(recorder_.lastPacket(), asPacketBuffer(&packet));
}

// Verifies unknown endpoint ids are rejected without invoking callbacks.
TEST_F(DispatchTest, RejectsUnknownEndpoint) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withEndpoint(WS_NAMESPACE_USER0, kUnknownEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

// Verifies unicast to a non-local host is rejected.
TEST_F(DispatchTest, RejectsUnicastToRemoteHost) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withDstHost(kRemoteHostId)
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

// Verifies broadcast to a wire the host belongs to is accepted.
TEST_F(DispatchTest, AcceptsBroadcastOnMemberWire) {
    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withDstHost(WS_HOST_BROADCAST)
                            .withWire(WS_WIRE_LOCAL_DOMAIN)
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), DISPATCH_OK);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
}

// Verifies broadcast on a wire the host does not belong to is rejected.
TEST_F(DispatchTest, RejectsBroadcastOnNonMemberWire) {
    support::configureHostWithoutLocalWire();

    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withDstHost(WS_HOST_BROADCAST)
                            .withWire(WS_WIRE_LOCAL_DOMAIN)
                            .withEndpoint(WS_NAMESPACE_USER0, kReceiverEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), DISPATCH_NO_ENDPOINT);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

// Verifies a matching endpoint with a null receive callback is treated as no delivery.
TEST_F(DispatchTest, RejectsEndpointWithNullCallback) {
    registerEndpoint(kSenderEndpoint, EndpointReceiverHandle{nullptr, nullptr});

    TestPacket packet = PacketBuilder{}
                            .withPayload("hello")
                            .withEndpoint(WS_NAMESPACE_USER0, kSenderEndpoint)
                            .packet();

    EXPECT_EQ(dispatch(packet), DISPATCH_NO_ENDPOINT);
}

// Verifies null table and packet pointers are rejected.
TEST_F(DispatchTest, RejectsNullTableOrPacket) {
    TestPacket packet = PacketBuilder{}.withPayload("hello").packet();
  PacketBufferHeader* packet_buffer = asPacketBuffer(&packet);

    EXPECT_EQ(dispatch_packet(nullptr, packet_buffer), DISPATCH_NO_ENDPOINT);
    EXPECT_EQ(dispatch_packet(&dispatch_table_, nullptr), DISPATCH_NO_ENDPOINT);
}

} // namespace
} // namespace wirespaces::test
