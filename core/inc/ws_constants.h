/**
 * @file ws_constants.h
 * @brief Provisional WireSpaces wire and host sentinel values.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace wirespaces {
#endif

/** @brief Deliver within the Endpoint Domain; does not enter a Link Interface. */
#define WS_WIRE_LOCAL_DOMAIN ((uint8_t)0x00U)

/** @brief Broadcast destination host sentinel (invalid as source). */
#define WS_HOST_BROADCAST ((uint8_t)0xFFU)

/** @brief Default single-slot mailbox payload capacity for prototypes. */
#define WS_MAILBOX_DEFAULT_CAPACITY 64U

#ifdef __cplusplus
} // namespace wirespaces
#endif
