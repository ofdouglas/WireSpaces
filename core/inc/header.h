/**
 * @file header.h
 * @brief WireSpaces header definition and accessors API.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ws_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WS_QOS_CRITICAL = 0,
    WS_QOS_HIGH = 1,
    WS_QOS_NORMAL = 2,
    WS_QOS_BACKGROUND = 3
} ws_qos_t;

typedef enum {
    WS_TRANSPORT_SIMPLE = 0,
} ws_transport_type_t;

typedef enum {
    WS_NAMESPACE_USER0 = 0,
    WS_NAMESPACE_USER1 = 1,
    WS_NAMESPACE_USER2 = 2,
    WS_NAMESPACE_COMMON = 3
} ws_namespace_t;

typedef struct {
    uint8_t control;
    uint8_t wire_number;
    uint8_t src_host;
    uint8_t dst_host;
    uint16_t endpoint;
} WS_PACKED ws_header_t;

typedef struct {
    ws_qos_t qos;
    bool has_extensions;
    ws_transport_type_t transport_type;
} ws_control_fields_t;

ws_qos_t ws_header_get_qos(const ws_header_t* header);
bool ws_header_get_has_extensions(const ws_header_t* header);
ws_transport_type_t ws_header_get_transport_type(const ws_header_t* header);

void ws_header_set_qos(ws_header_t* header, ws_qos_t qos);
void ws_header_set_has_extensions(ws_header_t* header, bool has_ext);
void ws_header_set_transport_type(ws_header_t* header, ws_transport_type_t type);
void ws_header_set_control_fields(ws_header_t* header, ws_control_fields_t control_fields);

void ws_header_set_endpoint(ws_header_t* header, ws_namespace_t namespace_id, uint16_t endpoint_id);
uint16_t ws_header_get_endpoint_id(const ws_header_t* header);
ws_namespace_t ws_header_get_namespace(const ws_header_t* header);

#ifdef __cplusplus
}
#endif
