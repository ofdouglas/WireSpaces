/**
 * @file header.c
 * @brief WireSpaces header accessor implementation.
 */

#include "header.h"

#include <stddef.h>

#define WS_CONTROL_QOS_MASK 0xC0U
#define WS_CONTROL_QOS_SHIFT 6U
#define WS_CONTROL_HAS_EXTENSIONS_MASK 0x08U
#define WS_CONTROL_TRANSPORT_TYPE_MASK 0x07U
#define WS_ENDPOINT_NAMESPACE_MASK 0xC000U
#define WS_ENDPOINT_NAMESPACE_SHIFT 14U
#define WS_ENDPOINT_ID_MASK 0x3FFFU

ws_qos_t ws_header_get_qos(const ws_header_t* header) {
    if (header == NULL) {
        return WS_QOS_NORMAL;
    }

    return (ws_qos_t)((header->control & WS_CONTROL_QOS_MASK) >> WS_CONTROL_QOS_SHIFT);
}

bool ws_header_get_has_extensions(const ws_header_t* header) {
    if (header == NULL) {
        return false;
    }

    return (header->control & WS_CONTROL_HAS_EXTENSIONS_MASK) != 0U;
}

ws_transport_type_t ws_header_get_transport_type(const ws_header_t* header) {
    if (header == NULL) {
        return WS_TRANSPORT_SIMPLE;
    }

    return (ws_transport_type_t)(header->control & WS_CONTROL_TRANSPORT_TYPE_MASK);
}

void ws_header_set_qos(ws_header_t* header, ws_qos_t qos) {
    if (header == NULL) {
        return;
    }

    header->control = (uint8_t)((header->control & (uint8_t)~WS_CONTROL_QOS_MASK) |
                                ((uint8_t)qos << WS_CONTROL_QOS_SHIFT));
}

void ws_header_set_has_extensions(ws_header_t* header, bool has_ext) {
    if (header == NULL) {
        return;
    }

    if (has_ext) {
        header->control |= WS_CONTROL_HAS_EXTENSIONS_MASK;
    } else {
        header->control &= (uint8_t)~WS_CONTROL_HAS_EXTENSIONS_MASK;
    }
}

void ws_header_set_transport_type(ws_header_t* header, ws_transport_type_t type) {
    if (header == NULL) {
        return;
    }

    header->control = (uint8_t)((header->control & (uint8_t)~WS_CONTROL_TRANSPORT_TYPE_MASK) |
                                (uint8_t)type);
}

void ws_header_set_control_fields(ws_header_t* header, ws_control_fields_t control_fields) {
    if (header == NULL) {
        return;
    }

    ws_header_set_qos(header, control_fields.qos);
    ws_header_set_has_extensions(header, control_fields.has_extensions);
    ws_header_set_transport_type(header, control_fields.transport_type);
}

void ws_header_set_endpoint(ws_header_t* header, ws_namespace_t namespace_id, uint16_t endpoint_id) {
    if (header == NULL) {
        return;
    }

    const uint16_t namespace_bits =
        (uint16_t)(((uint16_t)namespace_id << WS_ENDPOINT_NAMESPACE_SHIFT) & WS_ENDPOINT_NAMESPACE_MASK);
    const uint16_t id_bits = (uint16_t)(endpoint_id & WS_ENDPOINT_ID_MASK);
    header->endpoint = (uint16_t)(namespace_bits | id_bits);
}

uint16_t ws_header_get_endpoint_id(const ws_header_t* header) {
    if (header == NULL) {
        return 0U;
    }

    return (uint16_t)(header->endpoint & WS_ENDPOINT_ID_MASK);
}

ws_namespace_t ws_header_get_namespace(const ws_header_t* header) {
    if (header == NULL) {
        return WS_NAMESPACE_USER0;
    }

    return (ws_namespace_t)((header->endpoint & WS_ENDPOINT_NAMESPACE_MASK) >> WS_ENDPOINT_NAMESPACE_SHIFT);
}
