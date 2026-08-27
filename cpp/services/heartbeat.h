/**
 * @file heartbeat.h
 * @brief WireSpaces heartbeat service.
 */
#pragma once

#include <cstdint>
#include <cstddef>

#include "WireSpaces/cpp/hal/clock.h"
#include "WireSpaces/cpp/core/wirespaces_core.hpp"

// TODO: implement a proper service. For now just send a fixed message periodically.
// TODO: schema and code generation for message types.

#ifndef WS_SERVICE_HEARTBEAT_ENDPOINT_ID
#define WS_SERVICE_HEARTBEAT_ENDPOINT_ID 0xFFFE
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

    HeartbeatService() noexcept = default;

    void run() noexcept {
        const auto now_ms{wirespaces::hal::MillisecondClock::now_ms()};
        const auto elapsed_ms{now_ms - last_send_time_ms_};

        if (elapsed_ms >= period_ms) {
            last_send_time_ms_ = now_ms;
            send_heartbeat();
        }
    }

private:
    using TimePointT = wirespaces::hal::MillisecondClock::TimePoint;

    void send_heartbeat() noexcept {
        const auto now_ms{wirespaces::hal::MillisecondClock::now_ms()};
        const ControlFields control_fields{wirespaces::kQoSNormal, false, wirespaces::kTransportSimple};
        HeartbeatMessagePacket packet{};
    
        ws_packet_init(&packet, sizeof(HeartbeatMessagePacket), sizeof(HeartbeatMessagePacket), control_fields);
        packet.header.endpoint = WS_SERVICE_HEARTBEAT_ENDPOINT_ID;
        packet.payload.uptime_ms = now_ms;

        const auto result = wirespaces::send_packet(&packet);
        if (result != wirespaces::kDispatchOk) {
            // TODO: handle error
        }
        last_send_time_ms_ = now_ms;
    }

    TimePointT last_send_time_ms_{};
};

} // namespace heartbeat

