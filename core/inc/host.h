/**
 * @file host.h
 * @brief WireSpaces Host -- Information about the local host.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace wirespaces {
extern "C" {
#endif

// Default suitable for most MCUs, override for larger gateways.
#ifndef WS_HOST_INFO_MAX_NUM_WIRES
#define WS_HOST_INFO_MAX_NUM_WIRES 6
#endif

// Information about the local host: own ID/address, and the wires it is part of.
typedef struct {
    // Host ID sets the Header.src_host field of packets sent from this host
    uint8_t host_id;

    // List of wires this host is part of (some entries may be unused).
    uint8_t num_wires;
    uint8_t wires[WS_HOST_INFO_MAX_NUM_WIRES];
} HostInfo;


const HostInfo* host_get_info(void);

void host_set_info(const HostInfo* host_info);


#ifdef __cplusplus
} // extern "C"
} // namespace wirespaces
#endif