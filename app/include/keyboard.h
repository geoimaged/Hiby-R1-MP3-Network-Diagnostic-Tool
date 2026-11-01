#ifndef GEOS_KEYBOARD_H
#define GEOS_KEYBOARD_H

#include <stddef.h>
#include "platform.h"

/*
 * Reusable numeric/target keyboard.
 *
 * Returns:
 *   1 = user pressed RUN
 *   0 = user pressed BACK
 *
 * allow_slash:
 *   false for Ping/IP address
 *   true later for CIDR targets such as 192.168.1.0/24
 */
int geos_keyboard_target(
    bidhata_platform_t *platform,
    char *output,
    size_t output_cap,
    bool allow_slash
);

#endif
