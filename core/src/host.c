/**
 * @file host.c
 * @brief WireSpaces host information.
 */

#include "host.h"

#include <stddef.h>

static HostInfo g_host_info = {
    0x00U,
    0U,
    {0U},
};

const HostInfo* host_get_info(void) {
    return &g_host_info;
}

void host_set_info(const HostInfo* host_info) {
    if (host_info == NULL) {
        return;
    }

    g_host_info = *host_info;
}
