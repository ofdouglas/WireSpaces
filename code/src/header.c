/**
 * @file header.c
 * @brief WireSpaces header accessor implementation.
 */

#include "header.h"

#include <stddef.h>

QoSCode header_get_qos(const Header* header) {
    if (header == NULL) {
        return QOS_NORMAL;
    }

    return (QoSCode)((header->control & CONTROL_QOS_MASK) >> CONTROL_QOS_SHIFT);
}

bool header_get_has_extensions(const Header* header) {
    if (header == NULL) {
        return false;
    }

    return (header->control & CONTROL_HAS_EXTENSIONS_MASK) != 0U;
}

TransportType header_get_transport_type(const Header* header) {
    if (header == NULL) {
        return TRANSPORT_SIMPLE;
    }

    return (TransportType)(header->control & CONTROL_TRANSPORT_TYPE_MASK);
}

void header_set_qos(Header* header, QoSCode qos) {
    if (header == NULL) {
        return;
    }

    header->control =
        (uint8_t)((header->control & ~CONTROL_QOS_MASK) | ((uint8_t)qos << CONTROL_QOS_SHIFT));
}

void header_set_has_extensions(Header* header, bool has_ext) {
    if (header == NULL) {
        return;
    }

    if (has_ext) {
        header->control |= CONTROL_HAS_EXTENSIONS_MASK;
    } else {
        header->control &= (uint8_t)~CONTROL_HAS_EXTENSIONS_MASK;
    }
}

void header_set_transport_type(Header* header, TransportType type) {
    if (header == NULL) {
        return;
    }

    header->control =
        (uint8_t)((header->control & ~CONTROL_TRANSPORT_TYPE_MASK) | (uint8_t)type);
}

void header_set_control_fields(Header* header, ControlFields control_fields) {
    if (header == NULL) {
        return;
    }

    header_set_qos(header, control_fields.qos);
    header_set_has_extensions(header, control_fields.has_extensions);
    header_set_transport_type(header, control_fields.transport_type);
}

void header_set_endpoint(Header* header, WsNamespace namespace_id, uint16_t endpoint_id) {
    if (header == NULL) {
        return;
    }

    const uint16_t namespace_bits =
        (uint16_t)(((uint16_t)namespace_id << ENDPOINT_NAMESPACE_SHIFT) & ENDPOINT_NAMESPACE_MASK);
    const uint16_t id_bits = (uint16_t)(endpoint_id & ENDPOINT_ID_MASK);
    header->endpoint = (uint16_t)(namespace_bits | id_bits);
}

uint16_t header_get_endpoint_id(const Header* header) {
    if (header == NULL) {
        return 0U;
    }

    return (uint16_t)(header->endpoint & ENDPOINT_ID_MASK);
}

WsNamespace header_get_namespace(const Header* header) {
    if (header == NULL) {
        return WS_NAMESPACE_USER0;
    }

    return (WsNamespace)((header->endpoint & ENDPOINT_NAMESPACE_MASK) >> ENDPOINT_NAMESPACE_SHIFT);
}
