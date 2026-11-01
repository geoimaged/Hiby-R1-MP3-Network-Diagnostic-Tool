#ifndef GEOS_EXECUTOR_H
#define GEOS_EXECUTOR_H

#include <stddef.h>

typedef struct {
    int exit_code;
    int term_signal;
    int timed_out;
} executor_result_t;

/*
 * Run one program without a shell.
 *
 * program:
 *     Executable name or path.
 *
 * argv:
 *     NULL-terminated argument vector.
 *
 * output:
 *     Buffer receiving combined stdout + stderr.
 *
 * output_cap:
 *     Size of output buffer.
 *
 * timeout_ms:
 *     0 = no timeout.
 *
 * Returns:
 *     0  = executor completed normally
 *    -1  = executor itself failed
 *
 * The program's own non-zero exit status is stored in result->exit_code.
 */
int executor_run(
    const char *program,
    char *const argv[],
    char *output,
    size_t output_cap,
    int timeout_ms,
    executor_result_t *result
);

#endif
