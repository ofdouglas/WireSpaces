/**
 * @file host.h
 * @brief WireSpaces Host -- information about the local host.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WS_HOST_INFO_MAX_NUM_WIRES
#define WS_HOST_INFO_MAX_NUM_WIRES 6
#endif

typedef struct {
    uint8_t host_id;
    uint8_t num_wires;
    uint8_t wires[WS_HOST_INFO_MAX_NUM_WIRES];
} ws_host_info_t;

const ws_host_info_t* ws_host_get_info(void);
void ws_host_set_info(const ws_host_info_t* host_info);

#ifdef __cplusplus
}
#endif
