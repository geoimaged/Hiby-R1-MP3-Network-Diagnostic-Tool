#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "executor.h"
#include "connectivity.h"

static int run_command(
    const char *program,
    char *const argv[],
    char *output,
    size_t output_cap,
    int timeout_ms)
{
    executor_result_t result;

    if (!output || output_cap == 0)
        return -1;

    output[0] = '\0';

    return executor_run(
        program,
        argv,
        output,
        output_cap,
        timeout_ms,
        &result
    );
}

static void copy_line_value(
    const char *text,
    const char *key,
    char *dest,
    size_t dest_cap)
{
    if (!dest || dest_cap == 0)
        return;

    dest[0] = '\0';

    if (!text || !key || !key[0])
        return;

    size_t key_len = strlen(key);
    const char *cursor = text;

    while (*cursor) {
        const char *end = strchr(cursor, '\n');
        size_t line_len = end
            ? (size_t)(end - cursor)
            : strlen(cursor);

        /*
         * Match only when the key begins at the start of
         * a line. This prevents "ssid=" matching "bssid=".
         */
        if (line_len >= key_len &&
            memcmp(cursor, key, key_len) == 0) {

            const char *value = cursor + key_len;
            size_t len = line_len - key_len;

            while (len > 0 &&
                   (value[len - 1] == '\r' ||
                    value[len - 1] == ' ' ||
                    value[len - 1] == '\t')) {
                len--;
            }

            if (len >= dest_cap)
                len = dest_cap - 1;

            memcpy(dest, value, len);
            dest[len] = '\0';
            return;
        }

        if (!end)
            break;

        cursor = end + 1;
    }
}

static void copy_gateway(
    const char *text,
    char *dest,
    size_t dest_cap)
{
    const char *p;

    dest[0] = '\0';

    if (!text || !dest || dest_cap == 0)
        return;

    p = strstr(text, "default via ");
    if (!p)
        return;

    p += strlen("default via ");

    if (sscanf(p, "%63s", dest) != 1)
        dest[0] = '\0';

    dest[dest_cap - 1] = '\0';
}

static void copy_dns(
    const char *text,
    char *dest,
    size_t dest_cap)
{
    const char *p;

    dest[0] = '\0';

    if (!text || !dest || dest_cap == 0)
        return;

    p = strstr(text, "nameserver ");
    if (!p)
        return;

    p += strlen("nameserver ");

    if (sscanf(p, "%63s", dest) != 1)
        dest[0] = '\0';

    dest[dest_cap - 1] = '\0';
}

int connectivity_wifi_on(
    char *output,
    size_t output_cap)
{
    char *argv[] = {
        "wifi_on.sh",
        NULL
    };

    return run_command(
        "/usr/bin/wifi_on.sh",
        argv,
        output,
        output_cap,
        20000
    );
}

int connectivity_wifi_off(
    char *output,
    size_t output_cap)
{
    char *argv[] = {
        "wifi_off.sh",
        NULL
    };

    return run_command(
        "/usr/bin/wifi_off.sh",
        argv,
        output,
        output_cap,
        10000
    );
}

int connectivity_wifi_status(
    geos_wifi_status_t *status)
{
    char wpa_output[4096];
    char route_output[1024];
    char dns_output[1024];
    char ps_output[2048];

    if (!status)
        return -1;

    memset(status, 0, sizeof(*status));

    /*
     * WPA status
     */
    {
        char *argv[] = {
            "wpa_cli",
            "-i",
            "wlan0",
            "status",
            NULL
        };

        if (run_command(
                "/usr/sbin/wpa_cli",
                argv,
                wpa_output,
                sizeof(wpa_output),
                3000
            ) < 0) {
            return -1;
        }
    }

    {
        char state[32];

        copy_line_value(
            wpa_output,
            "wpa_state=",
            state,
            sizeof(state)
        );

        status->connected =
            strcmp(state, "COMPLETED") == 0;

        copy_line_value(
            wpa_output,
            "ssid=",
            status->ssid,
            sizeof(status->ssid)
        );

        copy_line_value(
            wpa_output,
            "ip_address=",
            status->ip,
            sizeof(status->ip)
        );
    }

    /*
     * Default gateway
     */
    {
        char *argv[] = {
            "ip",
            "route",
            NULL
        };

        run_command(
            "/sbin/ip",
            argv,
            route_output,
            sizeof(route_output),
            3000
        );

        copy_gateway(
            route_output,
            status->gateway,
            sizeof(status->gateway)
        );
    }

    /*
     * DNS
     */
    {
        char *argv[] = {
            "cat",
            "/etc/resolv.conf",
            NULL
        };

        run_command(
            "/bin/cat",
            argv,
            dns_output,
            sizeof(dns_output),
            2000
        );

        copy_dns(
            dns_output,
            status->dns,
            sizeof(status->dns)
        );
    }

    /*
     * DHCP process
     */
    {
        char *argv[] = {
            "busybox",
            "ps",
            NULL
        };

        run_command(
            "/bin/busybox",
            argv,
            ps_output,
            sizeof(ps_output),
            2000
        );

        status->dhcp_active =
            strstr(ps_output, "udhcpc") != NULL;

    /*
     * If an IPv4 address was successfully assigned,
     * treat DHCP configuration as successful even if
     * the daemon is no longer visible in ps output.
     */
    if (!status->dhcp_active && status->ip[0] != '\0')
        status->dhcp_active = true;
    }

    return 0;
}

int connectivity_wifi_scan(
    char *output,
    size_t output_cap)
{
    executor_result_t result;
    char scan_output[1024];

    char *scan_argv[] = {
        "wpa_cli",
        "-i",
        "wlan0",
        "scan",
        NULL
    };

    char *results_argv[] = {
        "wpa_cli",
        "-i",
        "wlan0",
        "scan_results",
        NULL
    };

    if (!output || output_cap == 0)
        return -1;

    output[0] = '\0';

    executor_run(
        "/usr/sbin/wpa_cli",
        scan_argv,
        scan_output,
        sizeof(scan_output),
        5000,
        &result
    );

    /*
     * Give the radio a moment to populate results.
     */
    sleep(2);

    return executor_run(
        "/usr/sbin/wpa_cli",
        results_argv,
        output,
        output_cap,
        5000,
        &result
    );
}
