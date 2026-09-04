/**
 * @file polling_service_test.cpp
 * @brief Poll-driven Ping and LED Service queue boundaries and responses.
 */

#include <cstring>

#include <gtest/gtest.h>

#include <wirespaces/services/led_control/led_control.h>
#include <wirespaces/services/ping/ping.h>

namespace wirespaces::test {
namespace {

constexpr WireNumber kWire{9U};
constexpr HostId kLocalHost{1U};
constexpr HostId kRemoteHost{2U};
constexpr EgressSet kEgress{1U};

WS_PACKET_BUFFER_DEFINE(ServicePacket, sizeof(ping::PingMessage));

/** @brief Capture response metadata and payload before its stack buffer expires. */
class ResponseRecorder final : public PacketForwarder {
public:
    void forward(const PacketBuffer& packet, EgressSet) noexcept override {
        ++count_;
        header_ = packet.header();
        size_ = packet.size();
        if (size_ > 0U) {
            std::memcpy(payload_, packet.payload().data(), size_);
        }
    }

    [[nodiscard]] uint32_t count() const noexcept { return count_; }
    [[nodiscard]] const Header& header() const noexcept { return header_; }
    [[nodiscard]] ByteSpan payload() const noexcept { return ByteSpan{payload_, size_}; }

private:
    Header header_{};
    uint8_t payload_[sizeof(ping::PingMessage)]{};
    uint16_t size_{0U};
    uint32_t count_{0U};
};

void initializeRequest(PacketBuffer& packet, EndpointAddress endpoint) {
    ASSERT_TRUE(packet.initialize(
        sizeof(ping::PingMessage),
        ControlFields{QoS::kNormal, false, TransportType::kSimple}));
    packet.header().wire = kWire;
    packet.header().source = kRemoteHost;
    packet.header().destination = kLocalHost;
    packet.header().endpoint = endpoint;
}

// Dispatch-time receipt only queues Ping work; run() later emits the response.
TEST(PollingServiceTest, PingProcessesQueuedRequestFromMainLoop) {
    ResponseRecorder responses{};
    const RouteTableEntry route{kWire, kEgress};
    Router router{foundation::Span<const RouteTableEntry>{&route, 1U}, responses};
    ping::PingService service{&router};
    ServicePacket request{};
    const EndpointAddress endpoint{
        EndpointAddress::from(Namespace::kCommon, WS_SERVICE_PING_ENDPOINT_ID)};
    initializeRequest(request, endpoint);
    const ping::PingMessage message{
        ping::PingMessage::kMagic,
        static_cast<uint8_t>(ping::PingMessage::Type::Request),
        0x1234U,
    };
    std::memcpy(request.payload().data(), &message, sizeof(message));

    ASSERT_EQ(service.receiver().receive(request), ReceiveResult::kAccepted);
    EXPECT_EQ(responses.count(), 0U);
    service.run();

    ASSERT_EQ(responses.count(), 1U);
    EXPECT_EQ(responses.header().source, kLocalHost);
    EXPECT_EQ(responses.header().destination, kRemoteHost);
    ping::PingMessage response{};
    std::memcpy(&response, responses.payload().data(), sizeof(response));
    EXPECT_EQ(response.type, static_cast<uint8_t>(ping::PingMessage::Type::Response));
    EXPECT_EQ(response.sequence_number, message.sequence_number);
}

void recordBrightness(void* context, uint8_t brightness) {
    *static_cast<uint8_t*>(context) = brightness;
}

// LED hardware and acknowledgement work are deferred until the Service is polled.
TEST(PollingServiceTest, LedControlProcessesQueuedRequestFromMainLoop) {
    ResponseRecorder responses{};
    const RouteTableEntry route{kWire, kEgress};
    Router router{foundation::Span<const RouteTableEntry>{&route, 1U}, responses};
    uint8_t brightness{0U};
    led_control::LedControlService service{&router, recordBrightness, &brightness};
    ServicePacket request{};
    const EndpointAddress endpoint{EndpointAddress::from(
        Namespace::kCommon, WS_SERVICE_LED_CONTROL_ENDPOINT_ID)};
    initializeRequest(request, endpoint);
    const led_control::LedControlMessage message{
        led_control::LedControlMessage::kMagic,
        static_cast<uint8_t>(led_control::LedControlMessage::Type::Request),
        0x80U,
        7U,
    };
    std::memcpy(request.payload().data(), &message, sizeof(message));

    ASSERT_EQ(service.receiver().receive(request), ReceiveResult::kAccepted);
    EXPECT_EQ(brightness, 0U);
    EXPECT_EQ(responses.count(), 0U);
    service.run();

    EXPECT_EQ(brightness, 0x80U);
    EXPECT_EQ(responses.count(), 1U);
}

}  // namespace
}  // namespace wirespaces::test
