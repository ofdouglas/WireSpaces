/**
 * @file stack_report.h
 * @brief WireSpaces peak stack-utilization reporting service.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "WireSpaces/cpp/core/wirespaces_core.hpp"
#include "WireSpaces/cpp/hal/clock.h"

#ifndef WS_SERVICE_STACK_REPORT_ENDPOINT_ID
#define WS_SERVICE_STACK_REPORT_ENDPOINT_ID 0x3FFCU
#endif

namespace stack_report {

// TODO: schema and code generation for message types.
struct StackReportMessage {
    uint16_t peak_used_bytes{};
    uint16_t capacity_bytes{};
};

WS_PACKET_DEFINE(StackReportMessagePacket, sizeof(StackReportMessage));

/**
 * @brief Periodically publish peak stack utilization.
 *
 * @tparam period_ms Minimum interval between reports.
 */
template <uint32_t period_ms>
class StackReportService {
public:
    /**
     * @brief Construct a stack reporting publisher.
     *
     * @param[in] route_table Router used to transmit reports.
     * @param[in] wire_number Logical Wire carrying reports.
     * @param[in] source_participant Canonical source Participant.
     * @param[in] destination_participant Canonical destination Participant.
     */
    StackReportService(
        wirespaces::RouteTable* route_table,
        uint8_t wire_number,
        uint8_t source_participant,
        uint8_t destination_participant) noexcept
        : route_table_{route_table}
        , wire_number_{wire_number}
        , source_participant_{source_participant}
        , destination_participant_{destination_participant} {}

    /**
     * @brief Publish the latest peak measurement when the period elapses.
     *
     * @param[in] peak_used_bytes Maximum observed stack consumption.
     * @param[in] capacity_bytes Stack region available after static allocation.
     */
    void run(
        uint16_t peak_used_bytes,
        uint16_t capacity_bytes) noexcept {
        const auto now_ms{wirespaces::hal::MillisecondClock::now()};
        if ((now_ms - last_send_time_ms_) < period_ms) {
            return;
        }

        last_send_time_ms_ = now_ms;
        sendReport(peak_used_bytes, capacity_bytes);
    }

private:
    using TimePointT = wirespaces::hal::MillisecondClock::TimePoint;

    /**
     * @brief Build and route one stack report.
     */
    void sendReport(
        uint16_t peak_used_bytes,
        uint16_t capacity_bytes) noexcept {
        if (route_table_ == nullptr) {
            return;
        }

        const wirespaces::ControlFields control_fields{
            wirespaces::kQoSBackground,
            false,
            wirespaces::kTransportSimple};
        StackReportMessagePacket packet{};
        auto* packet_buffer{
            reinterpret_cast<wirespaces::PacketBuffer*>(&packet)};
        ws_packet_init(
            packet_buffer,
            sizeof(StackReportMessage),
            sizeof(StackReportMessage),
            control_fields);
        packet.header.wire_number = wire_number_;
        packet.header.src_host = source_participant_;
        packet.header.dst_host = destination_participant_;
        ws_packet_set_endpoint(
            &packet.header,
            wirespaces::kNamespaceCommon,
            WS_SERVICE_STACK_REPORT_ENDPOINT_ID);

        const StackReportMessage message{
            peak_used_bytes,
            capacity_bytes,
        };
        std::memcpy(packet.data, &message, sizeof(message));
        (void)ws_router_forward_packet(route_table_, packet_buffer);
    }

    wirespaces::RouteTable* route_table_{nullptr};
    uint8_t wire_number_{0U};
    uint8_t source_participant_{0U};
    uint8_t destination_participant_{0U};
    TimePointT last_send_time_ms_{};
};

static_assert(
    sizeof(StackReportMessage) == 4U,
    "StackReportMessage wire representation must remain four bytes");

} // namespace stack_report
