/**
 * @file ping.h
 * @brief WireSpaces ping service.
 */
 #pragma once

 #include <cstddef>
 #include <cstdint>
 #include <cstring>
 
 #include "WireSpaces/cpp/hal/clock.h"
 #include "WireSpaces/cpp/core/wirespaces_core.hpp"
 

 #ifndef WS_SERVICE_PING_ENDPOINT_ID
 #define WS_SERVICE_PING_ENDPOINT_ID 0x3FFDU
 #endif
 
 
 namespace ping {
 
// TODO: schema and code generation for message types.
struct PingMessage {
    constexpr static uint8_t kMagic = 0xAB;
    enum class Type : uint8_t {
        Request = 0x01,
        Response = 0x02,
    };

    uint8_t magic{};
    uint8_t type{};
    uint16_t sequence_number{}; // Response echoes the request's sequence number.
};

 WS_PACKET_DEFINE(PingMessagePacket, sizeof(PingMessage));
 
 /**
  * @brief Ping service that responds to ping requests.
  */
 class PingService {
 public:
     /**
      * @brief Construct a ping service.
      * @param route_table The route table to use for sending ping messages.
      */
     PingService(wirespaces::RouteTable* route_table) noexcept
         : route_table_{route_table} {}

     /**
      * @brief Return the Endpoint receiver registration for this service.
      *
      * @return Receiver callback and context suitable for a dispatch table.
      */
     wirespaces::EndpointReceiver receiverHandle() noexcept {
         return wirespaces::EndpointReceiver{
             wirespaces::endpoint_receive_thunk<PingService>,
             this,
         };
     }

     /**
      * @brief Validate a ping request and send its matching response.
      *
      * Invalid sizes, magic values, message types, and broadcast destinations
      * are ignored.
      *
      * @param[in] packet Received canonical ping request.
      */
     void receive(const wirespaces::PacketBuffer* packet) noexcept {
         if ((packet == nullptr) ||
             (route_table_ == nullptr) ||
             (packet->size != sizeof(PingMessage)) ||
             (packet->header.dst_host == WS_HOST_BROADCAST)) {
             return;
         }

         PingMessage request{};
         std::memcpy(
             &request,
             ws_packet_payload_bytes(packet),
             sizeof(request));
         if ((request.magic != PingMessage::kMagic) ||
             (request.type != static_cast<uint8_t>(PingMessage::Type::Request))) {
             return;
         }

         sendResponse(packet->header, request.sequence_number);
     }

 private:
     /**
      * @brief Build and route a response to one validated request.
      *
      * @param[in] request_header Canonical request addressing.
      * @param[in] sequence_number Sequence copied from the request.
      */
     void sendResponse(
         const wirespaces::Header& request_header,
         uint16_t sequence_number) noexcept {
         const wirespaces::ControlFields control_fields{
             wirespaces::kQoSNormal,
             false,
             wirespaces::kTransportSimple};
         PingMessagePacket response_packet{};
         auto* response{
             reinterpret_cast<wirespaces::PacketBuffer*>(&response_packet)};
         ws_packet_init(
             response,
             sizeof(PingMessage),
             sizeof(PingMessage),
             control_fields);
         response_packet.header.wire_number = request_header.wire_number;
         response_packet.header.src_host = request_header.dst_host;
         response_packet.header.dst_host = request_header.src_host;
         ws_packet_set_endpoint(
             &response_packet.header,
             wirespaces::kNamespaceCommon,
             WS_SERVICE_PING_ENDPOINT_ID);

         const PingMessage message{
             PingMessage::kMagic,
             static_cast<uint8_t>(PingMessage::Type::Response),
             sequence_number,
         };
         std::memcpy(
             response_packet.data,
             &message,
             sizeof(message));
         (void)ws_router_forward_packet(route_table_, response);
     }

     wirespaces::RouteTable* route_table_{nullptr};
 };

 static_assert(
     sizeof(PingMessage) == 4U,
     "PingMessage wire representation must remain four bytes");

 } // namespace ping    