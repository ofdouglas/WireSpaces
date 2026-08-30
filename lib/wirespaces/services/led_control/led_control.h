/**
 * @file led_control.h
 * @brief WireSpaces directed 8-bit LED brightness control service.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <wirespaces/runtime/core.hpp>

#ifndef WS_SERVICE_LED_CONTROL_ENDPOINT_ID
#define WS_SERVICE_LED_CONTROL_ENDPOINT_ID 0x3FFBU
#endif

namespace led_control {

// TODO: schema and code generation for message types.
struct LedControlMessage {
    static constexpr uint8_t kMagic{0x4CU};
    enum class Type : uint8_t { Request = 0x01U, Response = 0x02U };
    enum class Brightness : uint8_t { Off = 0x00U, Full = 0xFFU };

    uint8_t magic{};
    uint8_t type{};
    uint8_t brightness{};
    uint8_t sequence_number{};
};

WS_PACKET_BUFFER_DEFINE(LedControlPacketBuffer, sizeof(LedControlMessage));

/** Directed LED command receiver that acknowledges the applied brightness. */
class LedControlService final : public wirespaces::EndpointReceiver {
public:
    using SetBrightness = void (*)(void* context, uint8_t brightness);

    LedControlService(wirespaces::Router* router, SetBrightness set_brightness,
                      void* output_context) noexcept
        : router_{router}, set_brightness_{set_brightness}, output_context_{output_context} {}

    wirespaces::ReceiveResult receive(const wirespaces::PacketBuffer& packet) noexcept override {
        if (router_ == nullptr || set_brightness_ == nullptr ||
            packet.size() != sizeof(LedControlMessage) ||
            packet.header().destination.isBroadcast()) {
            return wirespaces::ReceiveResult::kRejected;
        }

        LedControlMessage request{};
        std::memcpy(&request, packet.payload().data(), sizeof(request));
        if (request.magic != LedControlMessage::kMagic ||
            request.type != static_cast<uint8_t>(LedControlMessage::Type::Request)) {
            return wirespaces::ReceiveResult::kRejected;
        }

        set_brightness_(output_context_, request.brightness);
        sendResponse(packet.header(), request.brightness, request.sequence_number);
        return wirespaces::ReceiveResult::kAccepted;
    }

private:
    void sendResponse(const wirespaces::Header& request_header, uint8_t brightness,
                      uint8_t sequence_number) noexcept {
        LedControlPacketBuffer response{};
        static_cast<void>(
            response.initialize(sizeof(LedControlMessage),
                                wirespaces::ControlFields{wirespaces::QoS::kNormal, false,
                                                          wirespaces::TransportType::kSimple}));
        response.header().wire = request_header.wire;
        response.header().source = request_header.destination;
        response.header().destination = request_header.source;
        response.header().endpoint = wirespaces::EndpointAddress::from(
            wirespaces::Namespace::kCommon, WS_SERVICE_LED_CONTROL_ENDPOINT_ID);

        const LedControlMessage message{
            LedControlMessage::kMagic,
            static_cast<uint8_t>(LedControlMessage::Type::Response),
            brightness,
            sequence_number,
        };
        std::memcpy(response.payload().data(), &message, sizeof(message));
        static_cast<void>(router_->forward(response));
    }

    wirespaces::Router* router_{nullptr};
    SetBrightness set_brightness_{nullptr};
    void* output_context_{nullptr};
};

static_assert(sizeof(LedControlMessage) == 4U);

}  // namespace led_control
