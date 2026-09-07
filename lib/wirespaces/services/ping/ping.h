/**
 * @file ping.h
 * @brief WireSpaces ping service.
 */

#pragma once

#include <cstddef>
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

/** Poll-driven Ping service that sends a directed response for each queued request. */
class PingService final {
public:
    static constexpr std::size_t kReceiveQueueCapacity{2U};
    using ReceiverQueue = wirespaces::EndpointReceiverQueue<sizeof(PingMessage),
                                                            kReceiveQueueCapacity>;

    explicit PingService(wirespaces::Router* router) noexcept : router_{router} {}
    /** @brief Bind replies to the domain's Router for the lifetime of this service. */
    explicit PingService(wirespaces::DomainContext& domain) noexcept : PingService{&domain.router()} {}

    /** @brief Return the receiver registered with the Domain Dispatcher. */
    wirespaces::EndpointReceiver& receiver() noexcept {
        return receive_queue_;
    }

    /** @brief Process every request currently queued for this Service. */
    void run() noexcept {
        PingPacketBuffer packet{};
        while (receive_queue_.dequeue(packet)) {
            processRequest(packet);
        }
    }

private:
    void processRequest(const wirespaces::PacketBuffer& packet) noexcept {
        if ((router_ == nullptr) || (packet.size() != sizeof(PingMessage)) ||
            packet.header().destination.isBroadcast()) {
            return;
        }

        PingMessage request{};
        std::memcpy(&request, packet.payload().data(), sizeof(request));
        if ((request.magic != PingMessage::kMagic) ||
            (request.type != static_cast<uint8_t>(PingMessage::Type::Request))) {
            return;
        }

        sendResponse(packet.header(), request.sequence_number);
    }

    void sendResponse(const wirespaces::Header& request_header, uint16_t sequence_number) noexcept {
        PingPacketBuffer response{};
        static_cast<void>(response.initializeResponseTo(request_header,
                                                        sizeof(PingMessage),
                                                        wirespaces::ControlFields::defaultControlFields()));

        const PingMessage message{
            PingMessage::kMagic,
            static_cast<uint8_t>(PingMessage::Type::Response),
            sequence_number,
        };
        std::memcpy(response.payload().data(), &message, sizeof(message));
        router_->forward(response);
    }

    wirespaces::Router* router_{nullptr};
    ReceiverQueue receive_queue_{};
};

static_assert(sizeof(PingMessage) == 4U);

}  // namespace ping
