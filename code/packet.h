/**
 * @file packet.h
 * @brief WireSpaces packet definition and accessors API 
 */
#pragma once

#include "header.h"

#ifdef __cplusplus
namespace wirespaces {
extern "C" {
#endif

typedef struct {
    uint16_t length;
    Header header;
} PacketBufferHeader;

#define WS_PACKET_DEFINE(name, capacity) \
    typedef struct {            \
        uint16_t length;        \
        Header header;          \
        uint8_t data[(capacity)]; \
    } name;                     \


void packet_init(PacketBufferHeader* packet, uint16_t length, ControlFields control_fields);

uint8_t* packet_payload_bytes(PacketBufferHeader* packet);
void packet_set_endpoint(Header* header, WsNamespace namespace_id, uint16_t endpoint_id);


#ifdef __cplusplus
} // extern "C"
} // namespace wirespaces
#endif