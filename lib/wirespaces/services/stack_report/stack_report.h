/**
 * @file stack_report.h
 * @brief WireSpaces peak stack-utilization reporting service.
 */

#pragma once

#include <wirespaces/hal/clock.h>

#include <cstdint>
#include <cstring>
#include <wirespaces/runtime/core.hpp>

#ifndef WS_SERVICE_STACK_REPORT_ENDPOINT_ID
#define WS_SERVICE_STACK_REPORT_ENDPOINT_ID 0x3FFCU
#endif

namespace stack_report {

// TODO: schema and code generation for message types.
struct StackReportMessage {
    uint16_t peak_used_bytes{};
    uint16_t capacity_bytes{};
};

WS_PACKET_BUFFER_DEFINE(StackReportPacketBuffer, sizeof(StackReportMessage));

template <uint32_t period_ms>
class StackReportService {
public:
    /** @brief Bind source identity and routing to an explicitly owned domain. */
    StackReportService(wirespaces::DomainContext& domain, wirespaces::PublicationConfig publication) noexcept
        : StackReportService{&domain.router(), publication.wire, domain.hostInfo().id, publication.destination} {}

    StackReportService(wirespaces::Router* router, wirespaces::WireNumber wire,
                       wirespaces::HostId source_host, wirespaces::HostId destination_host) noexcept
        : router_{router},
          wire_{wire},
          source_host_{source_host},
          destination_host_{destination_host} {}

    void run(uint16_t peak_used_bytes, uint16_t capacity_bytes) noexcept {
        const auto now_ms{wirespaces::hal::MillisecondClock::now()};
        if ((now_ms - last_send_time_ms_) < period_ms) {
            return;
        }
        last_send_time_ms_ = now_ms;
        sendReport(peak_used_bytes, capacity_bytes);
    }

private:
    using TimePointT = wirespaces::hal::MillisecondClock::TimePoint;

    void sendReport(uint16_t peak_used_bytes, uint16_t capacity_bytes) noexcept {
        if (router_ == nullptr) {
            return;
        }

        StackReportPacketBuffer packet{};
        static_cast<void>(
            packet.initialize(sizeof(StackReportMessage),
                              wirespaces::ControlFields{wirespaces::QoS::kBackground, false,
                                                        wirespaces::TransportType::kSimple}));
        packet.header().wire = wire_;
        packet.header().source = source_host_;
        packet.header().destination = destination_host_;
        packet.header().endpoint = wirespaces::EndpointAddress::from(
            wirespaces::Namespace::kCommon, WS_SERVICE_STACK_REPORT_ENDPOINT_ID);

        const StackReportMessage message{peak_used_bytes, capacity_bytes};
        std::memcpy(packet.payload().data(), &message, sizeof(message));
        router_->forward(packet);
    }

    wirespaces::Router* router_{nullptr};
    wirespaces::WireNumber wire_{};
    wirespaces::HostId source_host_{};
    wirespaces::HostId destination_host_{};
    TimePointT last_send_time_ms_{};
};

static_assert(sizeof(StackReportMessage) == 4U);

}  // namespace stack_report
