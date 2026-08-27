/**
 * @file host.c
 * @brief WireSpaces host information.
 */

#include "host.h"

#include <stddef.h>

static ws_host_info_t g_host_info = {
    0x00U,
    0U,
    {0U},
};

const ws_host_info_t* ws_host_get_info(void) {
    return &g_host_info;
}

void ws_host_set_info(const ws_host_info_t* host_info) {
    if (host_info == NULL) {
        return;
    }

    g_host_info = *host_info;
}
