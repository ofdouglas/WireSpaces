/**
 * @file dispatch.h
 * @brief WireSpaces Endpoint Dispatcher -- Finds the right endpoint for a packet and dispatches it.
 *
 * TODO: should this be part of the host.h API?
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "header.h"
#include "packet.h"

#ifdef __cplusplus
namespace wirespaces {
extern "C" {
#endif

typedef enum {
    DISPATCH_OK = 0,
    DISPATCH_NO_ENDPOINT,
} DispatchResult;

typedef void (*receive_callback_t)(void* receiver_context, PacketBufferHeader* packet);

typedef struct {
    receive_callback_t receive;
    void* receiver_context;
} EndpointReceiverHandle;

// Most basic implementation: Linear search of an array
typedef struct {
    uint16_t endpoint;
    EndpointReceiverHandle receiver;
} DispatchTableEntry;

typedef struct {
    DispatchTableEntry* base;
    size_t capacity;
} DispatchTable;

// Deliver the packet to the appropriate endpoint. Only accept the packet if it is addressed to this host
// (either unicast to us or broadcast *to a wire we are part of*).
DispatchResult dispatch_packet(const DispatchTable* table, PacketBufferHeader* packet);

#ifdef __cplusplus
} // extern "C"

// TODO: move to other file?
template <typename T>
void endpoint_receive_thunk(void* context, PacketBufferHeader* packet){
    static_cast<T*>(context)->receive(packet);
}
#endif

#ifdef __cplusplus
} // namespace wirespaces
#endif

