#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "tools.h"

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>

int tool_ping(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result)
{
    if (!target ||
        !target[0] ||
        !output ||
        !result)
        return -1;

    /*
     * Direct argv execution:
     * no shell, no command-string parsing.
     */
    char *argv[] = {
        "ping",
        "-c",
        "4",
        (char *)target,
        NULL
    };

    return executor_run(
        "/bin/ping",
        argv,
        output,
        output_cap,
        12000,
        result
    );
}


int tool_dns_lookup(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result)
{
    char *argv[] = {
        "busybox",
        "nslookup",
        (char *)target,
        NULL
    };

    return executor_run(
        "/bin/busybox",
        argv,
        output,
        output_cap,
        10000,
        result
    );
}




int tool_web_check(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result)
{
    char url[512];

    if (!target ||
        !output ||
        output_cap == 0 ||
        !result) {
        return -1;
    }

    /*
     * Allow either a full URL or a bare host/IP.
     * The current GEOS keyboard can enter IP addresses,
     * while future keyboard versions can enter full URLs.
     */
    if (strncmp(target, "http://", 7) == 0 ||
        strncmp(target, "https://", 8) == 0) {

        snprintf(
            url,
            sizeof(url),
            "%s",
            target
        );

    } else {

        snprintf(
            url,
            sizeof(url),
            "http://%s/",
            target
        );
    }

    char *argv[] = {
        "busybox",
        "wget",
        "-S",
        "--spider",
        "-T",
        "5",
        "-Y",
        "off",
        url,
        NULL
    };

    return executor_run(
        "/bin/busybox",
        argv,
        output,
        output_cap,
        10000,
        result
    );
}




static int get_wlan0_ipv4(
    char *ip,
    size_t ip_cap)
{
    if (!ip || ip_cap == 0)
        return -1;

    ip[0] = '\0';

    int fd = socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    if (fd < 0)
        return -1;

    struct ifreq ifr;

    memset(
        &ifr,
        0,
        sizeof(ifr)
    );

    snprintf(
        ifr.ifr_name,
        sizeof(ifr.ifr_name),
        "wlan0"
    );

    if (ioctl(
            fd,
            SIOCGIFADDR,
            &ifr) < 0) {

        close(fd);
        return -1;
    }

    struct sockaddr_in *addr =
        (struct sockaddr_in *)&ifr.ifr_addr;

    if (!inet_ntop(
            AF_INET,
            &addr->sin_addr,
            ip,
            ip_cap)) {

        close(fd);
        ip[0] = '\0';
        return -1;
    }

    close(fd);

    return ip[0] ? 0 : -1;
}

int tool_nmap_scan_mode(
    const char *target,
    int mode,
    char *output,
    size_t output_cap,
    executor_result_t *result)
{
    if (!target ||
        !output ||
        output_cap == 0 ||
        !result) {

        return -1;
    }

    output[0] = '\0';

    /*
     * Never allow target to become an option.
     */
    if (target[0] == '-')
        return -1;

    /*
     * DISCOVER:
     *
     * We already proved this works on the R1.
     */
    char *discover[] = {
        "nmap",
        "--datadir",
        "/data/mnt/sd_0/nmap-data",
        "-R",
        "-sn",
        (char *)target,
        NULL
    };

    if (mode == 0) {

        return executor_run(
            "/data/mnt/sd_0/nmap",
            discover,
            output,
            output_cap,
            60000,
            result
        );
    }

    /*
     * PORT/SERVICE scans need the R1's actual wlan0
     * source IP because this cross-compiled Nmap build
     * cannot reliably discover its route/interface data.
     */
    char source_ip[64];

    if (get_wlan0_ipv4(
            source_ip,
            sizeof(source_ip)) < 0) {

        snprintf(
            output,
            output_cap,
            "ERROR: wlan0 has no IPv4 address\n"
            "Connect Wi-Fi first."
        );

        result->exit_code = 1;
        result->term_signal = 0;
        result->timed_out = 0;

        return -1;
    }

    /*
     * PORT SCAN
     */
    char *ports[] = {
        "nmap",
        "--datadir",
        "/data/mnt/sd_0/nmap-data",
        "-sT",
        "-Pn",
        "-S",
        source_ip,
        "--top-ports",
        "1000",
        target,
        NULL
    };

    /*
     * SERVICE INFO
     */
    char *services[] = {
        "nmap",
        "--datadir",
        "/data/mnt/sd_0/nmap-data",
        "-sT",
        "-Pn",
        "-S",
        source_ip,
        "-sV",
        "--top-ports",
        "1000",
        target,
        NULL
    };

    if (mode == 1) {

        return executor_run(
            "/data/mnt/sd_0/nmap",
            ports,
            output,
            output_cap,
            90000,
            result
        );
    }

    if (mode == 2) {

        return executor_run(
            "/data/mnt/sd_0/nmap",
            services,
            output,
            output_cap,
            120000,
            result
        );
    }

    return -1;
}


int tool_nmap_scan(
    const char *target,
    char *output,
    size_t output_cap,
    executor_result_t *result)
{
    return tool_nmap_scan_mode(
        target,
        0,
        output,
        output_cap,
        result
    );
}



