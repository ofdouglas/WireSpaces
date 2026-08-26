/**
 * @file packet.h
 * @brief WireSpaces packet definition and accessors API 
 */
#pragma once

#include "header.h"

#ifdef __cplusplus
namespace wirespaces {
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


#ifdef __cplusplus
} // namespace wirespaces
#endif