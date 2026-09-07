/**
 * @file heartbeat.h
 * @brief WireSpaces heartbeat service.
 */

#pragma once

#include <wirespaces/hal/clock.h>

#include <cstdint>
#include <cstring>
#include <wirespaces/runtime/core.hpp>

#ifndef WS_SERVICE_HEARTBEAT_ENDPOINT_ID
#define WS_SERVICE_HEARTBEAT_ENDPOINT_ID 0x3FFEU
#endif

namespace heartbeat {

// TODO: implement a proper service. For now just send a fixed message periodically.
// TODO: schema and code generation for message types.
struct HeartbeatMessage {
    uint32_t uptime_ms{};
};

WS_PACKET_BUFFER_DEFINE(HeartbeatPacketBuffer, sizeof(HeartbeatMessage));

template <uint32_t period_ms>
class HeartbeatService {
public:
    /** @brief Bind source identity and routing to an explicitly owned domain. */
    HeartbeatService(wirespaces::DomainContext& domain, wirespaces::PublicationConfig publication) noexcept
        : HeartbeatService{&domain.router(), publication.wire, domain.hostInfo().id, publication.destination} {}

    HeartbeatService(wirespaces::Router* router, wirespaces::WireNumber wire,
                     wirespaces::HostId source_host, wirespaces::HostId destination_host) noexcept
        : router_{router},
          wire_{wire},
          source_host_{source_host},
          destination_host_{destination_host} {}

    void run() noexcept {
        const auto now_ms{wirespaces::hal::MillisecondClock::now()};
        if ((now_ms - last_send_time_ms_) >= period_ms) {
            last_send_time_ms_ = now_ms;
            sendHeartbeat();
        }
    }

private:
    using TimePointT = wirespaces::hal::MillisecondClock::TimePoint;

    void sendHeartbeat() noexcept {
        if (router_ == nullptr) {
            return;
        }

        const auto now_ms{wirespaces::hal::MillisecondClock::now()};
        HeartbeatPacketBuffer packet{};
        static_cast<void>(
            packet.initialize(sizeof(HeartbeatMessage), wirespaces::ControlFields::defaultControlFields()));
        packet.header().wire = wire_;
        packet.header().source = source_host_;
        packet.header().destination = destination_host_;
        packet.header().endpoint = wirespaces::EndpointAddress::from(
            wirespaces::Namespace::kCommon, WS_SERVICE_HEARTBEAT_ENDPOINT_ID);
        std::memcpy(packet.payload().data(), &now_ms, sizeof(now_ms));

        if (router_->forward(packet) != wirespaces::RouteResult::kAccepted) {
            // TODO: handle error
        }
    }

    wirespaces::Router* router_{nullptr};
    wirespaces::WireNumber wire_{};
    wirespaces::HostId source_host_{};
    wirespaces::HostId destination_host_{};
    TimePointT last_send_time_ms_{};
};

}  // namespace heartbeat
