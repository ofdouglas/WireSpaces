/**
 * @file bits_fixture.hpp
 * @brief In-memory routed BITS connection and callback recorders for host tests.
 */

#pragma once

#include <wirespaces/transports/bits/bits.h>

#include <gtest/gtest.h>

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

/** @brief Forwards a packet synchronously into the peer Dispatcher. */
class DispatchForwarder final : public PacketForwarder {
public:
    void setTarget(Dispatcher& target) noexcept { target_ = &target; }

    void forward(const PacketBuffer& packet, EgressSet) noexcept override {
        if (target_ != nullptr) {
            last_result_ = target_->dispatch(packet);
        }
    }

    [[nodiscard]] DispatchResult lastResult() const noexcept { return last_result_; }

private:
    Dispatcher* target_{nullptr};
    DispatchResult last_result_{DispatchResult::kNoEndpoint};
};

/** @brief Records received object bytes and sideband datagrams. */
class ReceiverRecorder final : public ReceiverCallbacks {
public:
    bool onSegment(uint32_t object_offset, ByteSpan payload) noexcept override {
        if ((object_offset + payload.size()) > object_.size()) {
            return false;
        }
        std::memcpy(object_.data() + object_offset, payload.data(), payload.size());
        received_size_ = static_cast<uint32_t>(object_offset + payload.size());
        ++segment_count_;
        return true;
    }

    void onDatagram(ByteSpan payload) noexcept override {
        datagram_.assign(payload.begin(), payload.end());
    }

    void onTransferComplete() noexcept override { ++completion_count_; }

    [[nodiscard]] const std::array<uint8_t, 128U>& object() const noexcept { return object_; }
    [[nodiscard]] uint32_t receivedSize() const noexcept { return received_size_; }
    [[nodiscard]] uint32_t segmentCount() const noexcept { return segment_count_; }
    [[nodiscard]] uint32_t completionCount() const noexcept { return completion_count_; }
    [[nodiscard]] const std::vector<uint8_t>& datagram() const noexcept { return datagram_; }

private:
    std::array<uint8_t, 128U> object_{};
    std::vector<uint8_t> datagram_{};
    uint32_t received_size_{0U};
    uint32_t segment_count_{0U};
    uint32_t completion_count_{0U};
};

/** @brief Records transmitter-side sideband and completion callbacks. */
class TransmitterRecorder final : public TransmitterCallbacks {
public:
    void onDatagram(ByteSpan payload) noexcept override {
        datagram_.assign(payload.begin(), payload.end());
    }

    void onTransferComplete() noexcept override { ++completion_count_; }

    [[nodiscard]] const std::vector<uint8_t>& datagram() const noexcept { return datagram_; }
    [[nodiscard]] uint32_t completionCount() const noexcept { return completion_count_; }

private:
    std::vector<uint8_t> datagram_{};
    uint32_t completion_count_{0U};
};

/** @brief Complete point-to-point BITS fixture routed through core Router and Dispatcher objects. */
class BitsConnectionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        transmitter_forwarder_.setTarget(receiver_dispatcher_);
        receiver_forwarder_.setTarget(transmitter_dispatcher_);
    }

    ReceiverRecorder receiver_callbacks_{};
    TransmitterRecorder transmitter_callbacks_{};

    TestPacket receiver_segment_ingress_{};
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

    BitsReceiver receiver_{receiver_connection_, receiver_router_, receiver_callbacks_,
                           receiver_segment_ingress_, receiver_datagram_ingress_,
                           receiver_transmit_packet_};
    BitsTransmitter transmitter_{transmitter_connection_, transmitter_router_,
                                transmitter_callbacks_, transmitter_datagram_ingress_,
                                transmitter_transmit_packet_};

    DispatchTableEntry receiver_entry_{kReceiverHost, kTestEndpoint, &receiver_};
    DispatchTableEntry transmitter_entry_{kTransmitterHost, kTestEndpoint, &transmitter_};
    Dispatcher receiver_dispatcher_{
        foundation::Span<const DispatchTableEntry>{&receiver_entry_, 1U}};
    Dispatcher transmitter_dispatcher_{
        foundation::Span<const DispatchTableEntry>{&transmitter_entry_, 1U}};
};

}  // namespace wirespaces::transport::bits::test
