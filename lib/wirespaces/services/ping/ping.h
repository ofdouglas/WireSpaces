/**
 * @file ping.h
 * @brief WireSpaces ping service.
 */

#pragma once

#include <cstdint>
#include <cstring>

#include <wirespaces/runtime/core.hpp>

#ifndef WS_SERVICE_PING_ENDPOINT_ID
#define WS_SERVICE_PING_ENDPOINT_ID 0x3FFDU
#endif

namespace ping {

// TODO: schema and code generation for message types.
struct PingMessage {
    static constexpr uint8_t kMagic{0xABU};
    enum class Type : uint8_t { Request = 0x01U, Response = 0x02U };

    uint8_t magic{};
    uint8_t type{};
    uint16_t sequence_number{};
};

WS_PACKET_BUFFER_DEFINE(PingPacketBuffer, sizeof(PingMessage));

/** Ping request receiver that sends a directed response. */
class PingService final : public wirespaces::EndpointReceiver {
public:
    explicit PingService(wirespaces::Router* router) noexcept
        : router_{router} {}

    wirespaces::ReceiveResult receive(
        const wirespaces::PacketBuffer& packet) noexcept override {
        if (router_ == nullptr ||
            packet.size() != sizeof(PingMessage) ||
            packet.header().destination.isBroadcast()) {
            return wirespaces::ReceiveResult::kRejected;
        }

        PingMessage request{};
        std::memcpy(&request, packet.payload().data(), sizeof(request));
        if (request.magic != PingMessage::kMagic ||
            request.type != static_cast<uint8_t>(PingMessage::Type::Request)) {
            return wirespaces::ReceiveResult::kRejected;
        }

        sendResponse(packet.header(), request.sequence_number);
        return wirespaces::ReceiveResult::kAccepted;
    }

private:
    void sendResponse(
        const wirespaces::Header& request_header,
        uint16_t sequence_number) noexcept {
        PingPacketBuffer response{};
        static_cast<void>(response.initialize(
            sizeof(PingMessage),
            wirespaces::ControlFields{
                wirespaces::QoS::kNormal,
                false,
                wirespaces::TransportType::kSimple}));
        response.header().wire = request_header.wire;
        response.header().source = request_header.destination;
        response.header().destination = request_header.source;
        response.header().endpoint = wirespaces::EndpointAddress::from(
            wirespaces::Namespace::kCommon,
            WS_SERVICE_PING_ENDPOINT_ID);

        const PingMessage message{
            PingMessage::kMagic,
            static_cast<uint8_t>(PingMessage::Type::Response),
            sequence_number,
        };
        std::memcpy(response.payload().data(), &message, sizeof(message));
        static_cast<void>(router_->forward(response));
    }

    wirespaces::Router* router_{nullptr};
};

static_assert(sizeof(PingMessage) == 4U);

} // namespace ping
