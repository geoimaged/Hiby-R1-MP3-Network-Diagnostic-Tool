#ifndef GEOS_CONNECTIVITY_H
#define GEOS_CONNECTIVITY_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    bool connected;
    bool dhcp_active;

    char ssid[64];
    char ip[64];
    char gateway[64];
    char dns[64];
} geos_wifi_status_t;

int connectivity_wifi_on(
    char *output,
    size_t output_cap
);

int connectivity_wifi_off(
    char *output,
    size_t output_cap
);

int connectivity_wifi_status(
    geos_wifi_status_t *status
);

int connectivity_wifi_scan(
    char *output,
    size_t output_cap
);

#endif
