/**
 * @file header.h
 * @brief WireSpaces header definition and accessors API 
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
namespace wirespaces {
extern "C" {
#endif

// Control byte accessor constants
#define CONTROL_QOS_MASK            0xC0
#define CONTROL_QOS_SHIFT           6
#define CONTROL_HAS_EXTENSIONS_MASK 0x08
#define CONTROL_TRANSPORT_TYPE_MASK 0x07

// Endpoint accessor constants
#define ENDPOINT_NAMESPACE_MASK     0xC000
#define ENDPOINT_NAMESPACE_SHIFT    14
#define ENDPOINT_ID_MASK            0x3FFF

typedef enum {
    QOS_CRITICAL = 0,
    QOS_HIGH = 1,
    QOS_NORMAL = 2,
    QOS_BACKGROUND = 3
} QoSCode; // TODO: different name?

typedef enum {
    TRANSPORT_SIMPLE = 0,
} TransportType;

typedef enum {
    WS_NAMESPACE_USER0 = 0,
    WS_NAMESPACE_USER1 = 1,
    WS_NAMESPACE_USER2 = 2,
    WS_NAMESPACE_COMMON = 3
} WsNamespace;

typedef struct {
    uint8_t control;
    uint8_t wire_number;
    uint8_t src_host;
    uint8_t dst_host;
    uint16_t endpoint;
} Header;

// Unpacked representation of the control fields.
typedef struct {
    QoSCode qos;
    bool has_extensions;
    TransportType transport_type;
} ControlFields;

QoSCode header_get_qos(const Header* header);
bool header_get_has_extensions(const Header* header);
TransportType header_get_transport_type(const Header* header);

void header_set_qos(Header* header, QoSCode qos);
void header_set_has_extensions(Header* header, bool has_ext);
void header_set_transport_type(Header* header, TransportType type);
void header_set_control_fields(Header* header, ControlFields control_fields);

void header_set_endpoint(Header* header, WsNamespace namespace_id, uint16_t endpoint_id);
uint16_t header_get_endpoint_id(const Header* header);
WsNamespace header_get_namespace(const Header* header);

#ifdef __cplusplus
} // extern "C"
} // namespace wirespaces
#endif