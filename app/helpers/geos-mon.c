/*
 * geos-mon.c - toggle Broadcom (bcmdhd/cywdhd) monitor mode and set channel
 * using the driver's private ioctl (same mechanism as Broadcom's `wl` tool).
 *
 * Build (static, mipsel musl):
 *   mipsel-unknown-linux-musl-gcc -Os -static -Wall -o geos-mon geos-mon.c
 *
 * Usage: geos-mon <iface> on <channel>
 *        geos-mon <iface> off
 *        geos-mon <iface> status
 *
 * Stop wpa_supplicant first or it will fight you for the interface.
 * Needs root. Prints every step so failures are easy to read.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>

#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE 0x89F0
#endif

/* Broadcom wlioctl.h constants */
#define WLC_UP           2
#define WLC_DOWN         3
#define WLC_SET_CHANNEL  30
#define WLC_GET_MONITOR  107
#define WLC_SET_MONITOR  108

typedef struct {
    uint32_t cmd;
    void    *buf;
    uint32_t len;
    uint8_t  set;
    uint32_t used;
    uint32_t needed;
} wl_ioctl_t;

static int wl_ioctl(int s, const char *ifname, uint32_t cmd, void *buf,
                    uint32_t len, int set)
{
    struct ifreq ifr;
    wl_ioctl_t ioc;
    memset(&ifr, 0, sizeof ifr);
    memset(&ioc, 0, sizeof ioc);
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ioc.cmd = cmd;
    ioc.buf = buf;
    ioc.len = len;
    ioc.set = (uint8_t)set;
    ifr.ifr_data = (void *)&ioc;
    return ioctl(s, SIOCDEVPRIVATE, &ifr);
}

static int step(const char *what, int rc)
{
    printf("%-22s %s%s%s\n", what, rc < 0 ? "FAIL (" : "ok",
           rc < 0 ? strerror(errno) : "", rc < 0 ? ")" : "");
    return rc;
}

static int if_up(int s, const char *ifname)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof ifr);
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) return -1;
    ifr.ifr_flags |= IFF_UP;
    return ioctl(s, SIOCSIFFLAGS, &ifr);
}

static void print_type(const char *ifname)
{
    char path[128], line[32];
    snprintf(path, sizeof path, "/sys/class/net/%s/type", ifname);
    FILE *f = fopen(path, "r");
    if (f && fgets(line, sizeof line, f))
        printf("link type now: %s(1=ether, 802=prism, 803=radiotap)\n", line);
    if (f) fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <iface> on <channel> | off | status\n", argv[0]);
        return 2;
    }
    const char *ifn = argv[1], *cmd = argv[2];
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    uint32_t v;
    if (!strcmp(cmd, "check")) {
        v = 0;
        if (wl_ioctl(s, ifn, WLC_GET_MONITOR, &v, sizeof v, 0) < 0) {
            perror("WLC_GET_MONITOR");
            close(s);
            return 1;
        }
        close(s);
        return v == 1 ? 0 : 1;
    }
    if (!strcmp(cmd, "status")) {
        v = 0;
        int rc = wl_ioctl(s, ifn, WLC_GET_MONITOR, &v, sizeof v, 0);
        step("get monitor", rc);
        if (rc < 0) {
            close(s);
            return 1;
        }
        printf("monitor = %u\n", v);
        print_type(ifn);
        close(s);
        return 0;
    }
    if (!strcmp(cmd, "on")) {
        if (argc < 4) { fprintf(stderr, "need channel\n"); return 2; }
        uint32_t ch = (uint32_t)atoi(argv[3]);
        step("iface up", if_up(s, ifn));
        v = 0; step("WLC_DOWN", wl_ioctl(s, ifn, WLC_DOWN, &v, sizeof v, 1));
        v = 1; if (step("WLC_SET_MONITOR 2", wl_ioctl(s, ifn, WLC_SET_MONITOR, &v, sizeof v, 1)) < 0) return 1;
        v = 0; step("WLC_UP", wl_ioctl(s, ifn, WLC_UP, &v, sizeof v, 1));
        step("WLC_SET_CHANNEL", wl_ioctl(s, ifn, WLC_SET_CHANNEL, &ch, sizeof ch, 1));
        /* bcmdhd may expose the raw 802.11 monitor stream on prism0. The
         * netdev is created by WLC_SET_MONITOR but can remain administratively
         * down; bring it up before the capture process binds to it. */
        if (access("/sys/class/net/prism0/type", F_OK) == 0)
            step("prism0 up", if_up(s, "prism0"));
        print_type(ifn);
        return 0;
    }
    if (!strcmp(cmd, "off")) {
        v = 0; step("WLC_DOWN", wl_ioctl(s, ifn, WLC_DOWN, &v, sizeof v, 1));
        v = 0; step("WLC_SET_MONITOR 0", wl_ioctl(s, ifn, WLC_SET_MONITOR, &v, sizeof v, 1));
        v = 0; step("WLC_UP", wl_ioctl(s, ifn, WLC_UP, &v, sizeof v, 1));
        print_type(ifn);
        return 0;
    }
    fprintf(stderr, "unknown command %s\n", cmd);
    return 2;
}
