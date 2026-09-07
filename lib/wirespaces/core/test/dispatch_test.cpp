/**
 * @file dispatch_test.cpp
 * @brief Strong-address dispatch, broadcast fanout, and receiver outcomes.
 */

#include <gtest/gtest.h>

#include "support/dispatch_recorder.hpp"

namespace wirespaces::test {
namespace {

using support::DispatchTableFixture;
using support::kLocalHostId;
using support::kLocalWire;
using support::kReceiverEndpoint;
using support::kRemoteHostId;
using support::kUnknownEndpoint;
using support::PacketBuilder;
using support::TestPacket;

class DispatchTest : public DispatchTableFixture {
protected:
    void SetUp() override {
        DispatchTableFixture::SetUp();
        registerEndpoint(kReceiverEndpoint, recorder_);
    }
};

// Directed packets are offered to the matching host and EndpointReceiver.
TEST_F(DispatchTest, DeliversToRegisteredEndpoint) {
    TestPacket packet{PacketBuilder{}.withPayload("hello").withEndpoint(kReceiverEndpoint).packet()};
    EXPECT_EQ(dispatch(packet), DispatchResult::kAccepted);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
    EXPECT_EQ(recorder_.lastPacket(), &packet);
}

// A missing endpoint is reported without invoking a receiver.
TEST_F(DispatchTest, RejectsUnknownEndpoint) {
    TestPacket packet{PacketBuilder{}.withEndpoint(kUnknownEndpoint).packet()};
    EXPECT_EQ(dispatch(packet), DispatchResult::kNoEndpoint);
    EXPECT_EQ(recorder_.invocationCount(), 0U);
}

// A matching endpoint on another host is not a local directed destination.
TEST_F(DispatchTest, RejectsUnicastToRemoteHost) {
    TestPacket packet{PacketBuilder{}
                          .withDestination(kRemoteHostId)
                          .withEndpoint(kReceiverEndpoint)
                          .packet()};
    EXPECT_EQ(dispatch(packet), DispatchResult::kNoEndpoint);
}

// Broadcast on a joined wire fans out to every binding for the endpoint.
TEST_F(DispatchTest, FansOutBroadcastOnMemberWire) {
    support::DispatchRecorder second{};
    registerEndpoint(kReceiverEndpoint, second);
    TestPacket packet{PacketBuilder{}
                          .withDestination(HostId{kBroadcastHostValue})
                          .withWire(kLocalWire)
                          .withEndpoint(kReceiverEndpoint)
                          .packet()};
    EXPECT_EQ(dispatch(packet), DispatchResult::kAccepted);
    EXPECT_EQ(recorder_.invocationCount(), 1U);
    EXPECT_EQ(second.invocationCount(), 1U);
}

// Broadcast on a wire the local domain has not joined is rejected before fanout.
TEST_F(DispatchTest, RejectsBroadcastOnNonMemberWire) {
    support::configureHostWithoutLocalWire();
    TestPacket packet{PacketBuilder{}
                          .withDestination(HostId{kBroadcastHostValue})
                          .withWire(kLocalWire)
                          .withEndpoint(kReceiverEndpoint)
                          .packet()};
    EXPECT_EQ(dispatch(packet), DispatchResult::kNoEndpoint);
}

// Full and rejected receiver results propagate through the dispatcher.
TEST_F(DispatchTest, PropagatesReceiverResult) {
    TestPacket packet{PacketBuilder{}.withEndpoint(kReceiverEndpoint).packet()};
    recorder_.setResult(ReceiveResult::kFull);
    EXPECT_EQ(dispatch(packet), DispatchResult::kFull);
    recorder_.setResult(ReceiveResult::kRejected);
    EXPECT_EQ(dispatch(packet), DispatchResult::kRejected);
}

// Unsupported controls cannot reach even a generic receiver via local dispatch.
TEST_F(DispatchTest, RejectsUnsupportedControlBeforeReceiver) {
    TestPacket packet{PacketBuilder{}.withEndpoint(kReceiverEndpoint).packet()};
    for (const HostId destination : {kLocalHostId, HostId{kBroadcastHostValue}}) {
        packet.header().destination = destination;
        for (const uint8_t control : {2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 16U, 32U, 48U, 255U}) {
            SCOPED_TRACE(static_cast<unsigned>(control));
            packet.header().control = control;
            EXPECT_EQ(dispatch(packet), DispatchResult::kRejected);
        }
    }
    EXPECT_EQ(recorder_.invocationCount(), 0U);
    packet.header().destination = kLocalHostId;
    for (const uint8_t control : {0U, 1U, 64U, 65U, 128U, 129U, 192U, 193U}) {
        packet.header().control = control;
        EXPECT_EQ(dispatch(packet), DispatchResult::kAccepted);
    }
    EXPECT_EQ(recorder_.invocationCount(), 8U);
}

} // namespace
} // namespace wirespaces::test
