/**
 * @file led_control.h
 * @brief WireSpaces directed 8-bit LED brightness control service.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "WireSpaces/cpp/core/wirespaces_core.hpp"

#ifndef WS_SERVICE_LED_CONTROL_ENDPOINT_ID
#define WS_SERVICE_LED_CONTROL_ENDPOINT_ID 0x3FFBU
#endif

namespace led_control {

// TODO: schema and code generation for message types.
struct LedControlMessage {
    static constexpr uint8_t kMagic{0x4CU};

    enum class Type : uint8_t {
        Request = 0x01U,
        Response = 0x02U,
    };

    enum class Brightness : uint8_t {
        Off = 0x00U,
        Full = 0xFFU,
    };

    uint8_t magic{};
    uint8_t type{};
    uint8_t brightness{};
    uint8_t sequence_number{};
};

WS_PACKET_DEFINE(LedControlMessagePacket, sizeof(LedControlMessage));

/**
 * @brief Apply directed LED commands and acknowledge the resulting brightness.
 */
class LedControlService {
public:
    using SetBrightness = void (*)(void* context, uint8_t brightness);

    /**
     * @brief Construct an LED control service.
     *
     * @param[in] route_table Router used to transmit acknowledgements.
     * @param[in] set_brightness Platform callback that applies brightness.
     * @param[in] output_context Context supplied to set_brightness.
     */
    LedControlService(
        wirespaces::RouteTable* route_table,
        SetBrightness set_brightness,
        void* output_context) noexcept
        : route_table_{route_table}
        , set_brightness_{set_brightness}
        , output_context_{output_context} {}

    /**
     * @brief Return the receiver registration for the LED Endpoint.
     */
    wirespaces::EndpointReceiver receiverHandle() noexcept {
        return wirespaces::EndpointReceiver{
            wirespaces::endpoint_receive_thunk<LedControlService>,
            this,
        };
    }

    /**
     * @brief Validate and apply one directed LED command.
     *
     * Invalid payloads and broadcast commands are ignored.
     *
     * @param[in] packet Received canonical request.
     */
    void receive(const wirespaces::PacketBuffer* packet) noexcept {
        if ((packet == nullptr) ||
            (route_table_ == nullptr) ||
            (set_brightness_ == nullptr) ||
            (packet->size != sizeof(LedControlMessage)) ||
            (packet->header.dst_host == WS_HOST_BROADCAST)) {
            return;
        }

        LedControlMessage request{};
        std::memcpy(
            &request,
            ws_packet_payload_bytes(packet),
            sizeof(request));
        if ((request.magic != LedControlMessage::kMagic) ||
            (request.type !=
             static_cast<uint8_t>(LedControlMessage::Type::Request))) {
            return;
        }

        set_brightness_(
            output_context_,
            request.brightness);
        sendResponse(
            packet->header,
            request.brightness,
            request.sequence_number);
    }

private:
    /**
     * @brief Acknowledge an applied LED brightness.
     */
    void sendResponse(
        const wirespaces::Header& request_header,
        uint8_t brightness,
        uint8_t sequence_number) noexcept {
        const wirespaces::ControlFields control_fields{
            wirespaces::kQoSNormal,
            false,
            wirespaces::kTransportSimple};
        LedControlMessagePacket response_packet{};
        auto* response{
            reinterpret_cast<wirespaces::PacketBuffer*>(&response_packet)};
        ws_packet_init(
            response,
            sizeof(LedControlMessage),
            sizeof(LedControlMessage),
            control_fields);
        response_packet.header.wire_number = request_header.wire_number;
        response_packet.header.src_host = request_header.dst_host;
        response_packet.header.dst_host = request_header.src_host;
        ws_packet_set_endpoint(
            &response_packet.header,
            wirespaces::kNamespaceCommon,
            WS_SERVICE_LED_CONTROL_ENDPOINT_ID);

        const LedControlMessage message{
            LedControlMessage::kMagic,
            static_cast<uint8_t>(LedControlMessage::Type::Response),
            brightness,
            sequence_number,
        };
        std::memcpy(
            response_packet.data,
            &message,
            sizeof(message));
        (void)ws_router_forward_packet(route_table_, response);
    }

    wirespaces::RouteTable* route_table_{nullptr};
    SetBrightness set_brightness_{nullptr};
    void* output_context_{nullptr};
};

static_assert(
    sizeof(LedControlMessage) == 4U,
    "LedControlMessage wire representation must remain four bytes");

} // namespace led_control
