/**
 * @file heartbeat.h
 * @brief WireSpaces heartbeat service.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "WireSpaces/cpp/hal/clock.h"
#include "WireSpaces/cpp/core/wirespaces_core.hpp"

// TODO: implement a proper service. For now just send a fixed message periodically.
// TODO: schema and code generation for message types.

#ifndef WS_SERVICE_HEARTBEAT_ENDPOINT_ID
#define WS_SERVICE_HEARTBEAT_ENDPOINT_ID 0x3FFEU
#endif


namespace heartbeat {

struct HeartbeatMessage {
    uint32_t uptime_ms{};
};

WS_PACKET_DEFINE(HeartbeatMessagePacket, sizeof(HeartbeatMessage));

/**
 * @brief Heartbeat service that sends a message periodically. The message contains the 
          uptime in milliseconds since the clock was started.
 * @tparam period_ms The period in milliseconds at which to send the heartbeat message.
 */
template <uint32_t period_ms>
class HeartbeatService {
public:
    /**
     * @brief Construct a periodic heartbeat publisher.
     *
     * @param[in] route_table Router used to transmit heartbeat packets.
     * @param[in] wire_number Logical Wire carrying the heartbeat.
     * @param[in] source_participant Canonical source Participant.
     * @param[in] destination_participant Canonical destination Participant.
     */
    HeartbeatService(
        wirespaces::RouteTable* route_table,
        uint8_t wire_number,
        uint8_t source_participant,
        uint8_t destination_participant) noexcept
        : route_table_{route_table}
        , wire_number_{wire_number}
        , source_participant_{source_participant}
        , destination_participant_{destination_participant} {}

    /**
     * @brief Publish a heartbeat when the configured period has elapsed.
     */
    void run() noexcept {
        const auto now_ms{wirespaces::hal::MillisecondClock::now()};
        const auto elapsed_ms{now_ms - last_send_time_ms_};

        if (elapsed_ms >= period_ms) {
            last_send_time_ms_ = now_ms;
            sendHeartbeat();
        }
    }

private:
    using TimePointT = wirespaces::hal::MillisecondClock::TimePoint;

    /**
     * @brief Build and route one heartbeat packet.
     */
    void sendHeartbeat() noexcept {
        if (route_table_ == nullptr) {
            return;
        }

        const auto now_ms{wirespaces::hal::MillisecondClock::now()};
        const wirespaces::ControlFields control_fields{
            wirespaces::kQoSNormal,
            false,
            wirespaces::kTransportSimple};
        HeartbeatMessagePacket packet{};

        auto* packet_buffer{reinterpret_cast<wirespaces::PacketBuffer*>(&packet)};
        ws_packet_init(
            packet_buffer,
            sizeof(HeartbeatMessage),
            sizeof(HeartbeatMessage),
            control_fields);
        packet.header.wire_number = wire_number_;
        packet.header.src_host = source_participant_;
        packet.header.dst_host = destination_participant_;
        ws_packet_set_endpoint(
            &packet.header,
            wirespaces::kNamespaceCommon,
            WS_SERVICE_HEARTBEAT_ENDPOINT_ID);
        std::memcpy(packet.data, &now_ms, sizeof(now_ms));

        const auto result{ws_router_forward_packet(route_table_, packet_buffer)};
        if (result != wirespaces::kDispatchOk) {
            // TODO: handle error
        }
    }

    wirespaces::RouteTable* route_table_{nullptr};
    uint8_t wire_number_{0U};
    uint8_t source_participant_{0U};
    uint8_t destination_participant_{0U};
    TimePointT last_send_time_ms_{};
};

} // namespace heartbeat

