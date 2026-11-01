#include "executor.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static long long monotonic_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return 0;

    return (long long)ts.tv_sec * 1000LL +
           (long long)ts.tv_nsec / 1000000LL;
}

int executor_run(
    const char *program,
    char *const argv[],
    char *output,
    size_t output_cap,
    int timeout_ms,
    executor_result_t *result)
{
    int pipefd[2];
    pid_t pid;
    int status = 0;
    int child_reaped = 0;
    int pipe_closed = 0;
    size_t output_len = 0;
    long long start_ms;

    if (!program || !argv || !output || output_cap == 0 || !result)
        return -1;

    memset(result, 0, sizeof(*result));
    result->exit_code = -1;

    output[0] = '\0';

    if (pipe(pipefd) < 0)
        return -1;

    pid = fork();

    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /*
         * Child:
         * stdout and stderr both go into the same pipe.
         */
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(126);

        if (dup2(pipefd[1], STDERR_FILENO) < 0)
            _exit(126);

        close(pipefd[1]);

        /*
         * No shell is involved.
         * User input becomes argv[] rather than shell syntax.
         */
        execvp(program, argv);

        /*
         * execvp failed.
         * The error will be captured by the parent.
         */
        dprintf(STDERR_FILENO,
                "GEOS: exec failed for %s: %s\n",
                program,
                strerror(errno));

        _exit(127);
    }

    close(pipefd[1]);

    /*
     * Non-blocking read lets us capture output while monitoring
     * the child and enforcing a timeout.
     */
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0)
        fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    start_ms = monotonic_ms();

    while (!pipe_closed || !child_reaped) {
        if (!child_reaped) {
            pid_t w = waitpid(pid, &status, WNOHANG);

            if (w == pid) {
                child_reaped = 1;
            } else if (w < 0 && errno != EINTR) {
                close(pipefd[0]);
                return -1;
            }
        }

        if (!pipe_closed) {
            struct pollfd pfd;

            pfd.fd = pipefd[0];
            pfd.events = POLLIN | POLLHUP;
            pfd.revents = 0;

            int wait_ms = 100;

            if (timeout_ms > 0 && !child_reaped) {
                long long elapsed = monotonic_ms() - start_ms;
                long long remaining = (long long)timeout_ms - elapsed;

                if (remaining <= 0) {
                    kill(pid, SIGKILL);
                    result->timed_out = 1;

                    while (waitpid(pid, &status, 0) < 0) {
                        if (errno != EINTR)
                            break;
                    }

                    child_reaped = 1;
                } else if (remaining < wait_ms) {
                    wait_ms = (int)remaining;
                }
            }

            if (!pipe_closed) {
                int pr = poll(&pfd, 1, wait_ms);

                if (pr < 0) {
                    if (errno != EINTR) {
                        close(pipefd[0]);
                        return -1;
                    }
                }

                if (pr > 0 &&
                    (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {

                    char buf[512];

                    for (;;) {
                        ssize_t n = read(pipefd[0], buf, sizeof(buf));

                        if (n > 0) {
                            size_t space = output_cap - 1 - output_len;
                            size_t copy = ((size_t)n < space)
                                        ? (size_t)n
                                        : space;

                            if (copy > 0) {
                                memcpy(output + output_len, buf, copy);
                                output_len += copy;
                                output[output_len] = '\0';
                            }
                        } else if (n == 0) {
                            pipe_closed = 1;
                            close(pipefd[0]);
                            break;
                        } else if (errno == EAGAIN ||
                                   errno == EWOULDBLOCK ||
                                   errno == EINTR) {
                            break;
                        } else {
                            pipe_closed = 1;
                            close(pipefd[0]);
                            break;
                        }
                    }
                }
            }
        } else if (!child_reaped) {
            /*
             * Pipe is closed but child hasn't been reaped yet.
             */
            while (waitpid(pid, &status, 0) < 0) {
                if (errno != EINTR)
                    break;
            }

            child_reaped = 1;
        }
    }

    if (WIFEXITED(status)) {
        result->exit_code = WEXITSTATUS(status);
        result->term_signal = 0;
    } else if (WIFSIGNALED(status)) {
        result->exit_code = -1;
        result->term_signal = WTERMSIG(status);
    }

    return 0;
}
