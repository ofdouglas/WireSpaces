/**
 * @file bits_fixture.hpp
 * @brief In-memory routed BITS connection and deterministic fault injection for host tests.
 */

#pragma once

#include <wirespaces/core/host.h>
#include <wirespaces/transports/bits/bits.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace wirespaces::transport::bits::test {

constexpr WireNumber kTestWire{7U};
constexpr HostId kTransmitterHost{1U};
constexpr HostId kReceiverHost{2U};
constexpr EndpointAddress kTestEndpoint{EndpointAddress::from(Namespace::kCommon, 42U)};
constexpr EgressSet kTestEgress{1U};
constexpr uint16_t kTestPacketCapacity{64U};

WS_PACKET_BUFFER_DEFINE(TestPacket, kTestPacketCapacity);

/** @brief Select the simulated host receiving the next dispatched packet. */
inline void selectLocalHost(HostId host) noexcept {
    setLocalHostInfo(HostInfo{host, 1U, {kTestWire}});
}

/** @brief Forwards, drops, or holds packets synchronously before a peer Dispatcher. */
class DispatchForwarder final : public PacketForwarder {
public:
    void setTarget(Dispatcher& target, HostId target_host) noexcept {
        target_ = &target;
        target_host_ = target_host;
    }

    void dropNext(MessageType type, uint8_t count = 1U) noexcept {
        drop_type_ = type;
        drop_remaining_ = count;
    }

    void holdNextSegment() noexcept { hold_next_segment_ = true; }

    [[nodiscard]] bool releaseHeld() noexcept {
        if (!held_ || target_ == nullptr) {
            return false;
        }
        selectLocalHost(target_host_);
        last_result_ = target_->dispatch(held_packet_);
        held_ = false;
        return true;
    }

    void forward(const PacketBuffer& packet, EgressSet) noexcept override {
        Control control{};
        if (packet.payload().empty() || !decodeControl(packet.payload()[0], control)) {
            return;
        }

        const size_t type_index{static_cast<size_t>(control.type)};
        ++message_counts_[type_index];
        capture(packet, last_packet_);

        if (hold_next_segment_ && control.type == MessageType::kSegment) {
            hold_next_segment_ = false;
            held_ = capture(packet, held_packet_);
            return;
        }
        if (drop_remaining_ > 0U && control.type == drop_type_) {
            --drop_remaining_;
            return;
        }
        if (target_ != nullptr) {
            selectLocalHost(target_host_);
            last_result_ = target_->dispatch(packet);
        }
    }

    uint32_t messageCount(MessageType type) const noexcept {
        return message_counts_[static_cast<size_t>(type)];
    }
    const PacketBuffer& lastPacket() const noexcept { return last_packet_; }
    DispatchResult lastResult() const noexcept { return last_result_; }

private:
    static bool capture(const PacketBuffer& source, PacketBuffer& destination) noexcept {
        if (!destination.resize(source.size())) {
            return false;
        }
        destination.header() = source.header();
        if (source.size() > 0U) {
            std::memcpy(destination.payload().data(), source.payload().data(), source.size());
        }
        return true;
    }

    Dispatcher* target_{nullptr};
    HostId target_host_{};
    DispatchResult last_result_{DispatchResult::kNoEndpoint};
    std::array<uint32_t, 7U> message_counts_{};
    MessageType drop_type_{MessageType::kSetup};
    uint8_t drop_remaining_{0U};
    bool hold_next_segment_{false};
    bool held_{false};
    TestPacket held_packet_{};
    TestPacket last_packet_{};
};

/** @brief Records received object bytes, offsets, sideband datagrams, and terminal events. */
class ReceiverRecorder final : public ReceiverCallbacks {
public:
    bool onSegment(uint32_t object_offset, ByteSpan payload) noexcept override {
        if ((object_offset + payload.size()) > object_.size()) {
            return false;
        }
        std::memcpy(object_.data() + object_offset, payload.data(), payload.size());
        received_size_ = std::max(received_size_,
                                  static_cast<uint32_t>(object_offset + payload.size()));
        offsets_.push_back(object_offset);
        ++segment_count_;
        return true;
    }

    void onDatagram(ByteSpan payload) noexcept override {
        datagram_.assign(payload.begin(), payload.end());
    }

    void onTransferComplete() noexcept override { ++completion_count_; }
    void onTransferAborted() noexcept override { ++abort_count_; }

    const std::array<uint8_t, 128U>& object() const noexcept { return object_; }
    uint32_t receivedSize() const noexcept { return received_size_; }
    uint32_t segmentCount() const noexcept { return segment_count_; }
    uint32_t completionCount() const noexcept { return completion_count_; }
    uint32_t abortCount() const noexcept { return abort_count_; }
    const std::vector<uint32_t>& offsets() const noexcept { return offsets_; }
    const std::vector<uint8_t>& datagram() const noexcept { return datagram_; }

private:
    std::array<uint8_t, 128U> object_{};
    std::vector<uint32_t> offsets_{};
    std::vector<uint8_t> datagram_{};
    uint32_t received_size_{0U};
    uint32_t segment_count_{0U};
    uint32_t completion_count_{0U};
    uint32_t abort_count_{0U};
};

/** @brief Records transmitter-side sideband and terminal callbacks. */
class TransmitterRecorder final : public TransmitterCallbacks {
public:
    void onDatagram(ByteSpan payload) noexcept override {
        datagram_.assign(payload.begin(), payload.end());
    }

    void onTransferComplete() noexcept override { ++completion_count_; }

    void onTransferRejected(RejectReason reason) noexcept override {
        rejection_reason_ = reason;
        ++rejection_count_;
    }

    void onTransferAborted() noexcept override { ++abort_count_; }

    const std::vector<uint8_t>& datagram() const noexcept { return datagram_; }
    uint32_t completionCount() const noexcept { return completion_count_; }
    uint32_t rejectionCount() const noexcept { return rejection_count_; }
    RejectReason rejectionReason() const noexcept { return rejection_reason_; }
    uint32_t abortCount() const noexcept { return abort_count_; }

private:
    std::vector<uint8_t> datagram_{};
    RejectReason rejection_reason_{RejectReason::kInvalidArgument};
    uint32_t completion_count_{0U};
    uint32_t rejection_count_{0U};
    uint32_t abort_count_{0U};
};

/** @brief Complete point-to-point BITS fixture routed through core Router and Dispatcher objects. */
class BitsConnectionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        transmitter_forwarder_.setTarget(receiver_dispatcher_, kReceiverHost);
        receiver_forwarder_.setTarget(transmitter_dispatcher_, kTransmitterHost);
    }

    [[nodiscard]] bool pumpUntilTerminal(uint16_t maximum_iterations = 128U) {
        for (uint16_t iteration{0U}; iteration < maximum_iterations; ++iteration) {
            ++now_ms_;
            if (transmitter_.process(now_ms_) == ProcessResult::kError) {
                return false;
            }
            if (receiver_.process() == ProcessResult::kError) {
                return false;
            }
            if (transmitter_.state() == TransferState::kCompleted ||
                transmitter_.state() == TransferState::kRejected ||
                transmitter_.state() == TransferState::kAborted) {
                return true;
            }
        }
        return false;
    }

    DispatchResult injectAck(const Ack& ack) {
        TestPacket packet{};
        EXPECT_TRUE(packet.initialize(kAckSize,
                                      ControlFields{QoS::kNormal, false,
                                                    TransportType::kBits}));
        packet.header().wire = kTestWire;
        packet.header().source = kReceiverHost;
        packet.header().destination = kTransmitterHost;
        packet.header().endpoint = kTestEndpoint;
        EXPECT_TRUE(encodeAck(ack, packet.payload()));
        selectLocalHost(kTransmitterHost);
        return transmitter_dispatcher_.dispatch(packet);
    }

    DispatchResult injectSetup(
        const wirespaces::transport::bits::Setup& setup) {
        TestPacket packet{};
        EXPECT_TRUE(packet.initialize(kSetupSize,
                                      ControlFields{QoS::kNormal, false,
                                                    TransportType::kBits}));
        packet.header().wire = kTestWire;
        packet.header().source = kTransmitterHost;
        packet.header().destination = kReceiverHost;
        packet.header().endpoint = kTestEndpoint;
        EXPECT_TRUE(encodeSetup(setup, packet.payload()));
        selectLocalHost(kReceiverHost);
        return receiver_dispatcher_.dispatch(packet);
    }

    DispatchResult injectReject(const Reject& reject) {
        TestPacket packet{};
        EXPECT_TRUE(packet.initialize(kRejectSize,
                                      ControlFields{QoS::kNormal, false,
                                                    TransportType::kBits}));
        packet.header().wire = kTestWire;
        packet.header().source = kReceiverHost;
        packet.header().destination = kTransmitterHost;
        packet.header().endpoint = kTestEndpoint;
        EXPECT_TRUE(encodeReject(reject, packet.payload()));
        selectLocalHost(kTransmitterHost);
        return transmitter_dispatcher_.dispatch(packet);
    }

    ReceiverRecorder receiver_callbacks_{};
    TransmitterRecorder transmitter_callbacks_{};

    TestPacket receiver_segment_ingress_0_{};
    TestPacket receiver_segment_ingress_1_{};
    TestPacket receiver_segment_ingress_2_{};
    TestPacket receiver_segment_ingress_3_{};
    PacketBuffer* receiver_segment_slots_[4U]{
        &receiver_segment_ingress_0_,
        &receiver_segment_ingress_1_,
        &receiver_segment_ingress_2_,
        &receiver_segment_ingress_3_,
    };
    TestPacket receiver_datagram_ingress_{};
    TestPacket receiver_transmit_packet_{};
    TestPacket transmitter_datagram_ingress_{};
    TestPacket transmitter_transmit_packet_{};

    DispatchForwarder transmitter_forwarder_{};
    DispatchForwarder receiver_forwarder_{};
    RouteTableEntry transmitter_route_{kTestWire, kTestEgress};
    RouteTableEntry receiver_route_{kTestWire, kTestEgress};
    Router transmitter_router_{foundation::Span<const RouteTableEntry>{&transmitter_route_, 1U},
                               transmitter_forwarder_};
    Router receiver_router_{foundation::Span<const RouteTableEntry>{&receiver_route_, 1U},
                            receiver_forwarder_};

    ConnectionConfig transmitter_connection_{kTestWire, kTransmitterHost, kReceiverHost,
                                              kTestEndpoint};
    ConnectionConfig receiver_connection_{kTestWire, kReceiverHost, kTransmitterHost,
                                           kTestEndpoint};
    TimingConfig timing_{10U, 7U, 3U};

    BitsReceiver receiver_{
        receiver_connection_, receiver_router_, receiver_callbacks_,
        foundation::Span<PacketBuffer*>{receiver_segment_slots_},
        receiver_datagram_ingress_, receiver_transmit_packet_};
    BitsTransmitter transmitter_{transmitter_connection_, timing_, transmitter_router_,
                                transmitter_callbacks_, transmitter_datagram_ingress_,
                                transmitter_transmit_packet_};

    DispatchTableEntry receiver_entry_{kTestEndpoint, &receiver_};
    DispatchTableEntry transmitter_entry_{kTestEndpoint, &transmitter_};
    Dispatcher receiver_dispatcher_{
        foundation::Span<const DispatchTableEntry>{&receiver_entry_, 1U}};
    Dispatcher transmitter_dispatcher_{
        foundation::Span<const DispatchTableEntry>{&transmitter_entry_, 1U}};
    uint32_t now_ms_{0U};
};

}  // namespace wirespaces::transport::bits::test
