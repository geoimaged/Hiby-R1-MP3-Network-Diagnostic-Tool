#ifndef GEOS_TOOLS_H
#define GEOS_TOOLS_H

#include <stddef.h>
#include "executor.h"

/*
 * Ping one target using the R1's BusyBox /bin/ping.
 *
 * Returns:
 *   0  = executor completed
 *  -1  = executor failed
 */
int tool_ping(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result
);


int tool_dns_lookup(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result);


int tool_web_check(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result);


int tool_nmap_scan(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result);


int tool_nmap_scan_mode(
    const char *target,
    int mode,
    char *output,
    size_t output_cap,
    executor_result_t *result);

#endif
