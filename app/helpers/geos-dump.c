/*
 * geos-dump.c - minimal dependency-free packet capture to .pcap
 *
 * Build (static, mipsel musl):
 *   mipsel-unknown-linux-musl-gcc -Os -static -Wall -o geos-dump geos-dump.c
 *
 * Usage: geos-dump -w out.pcap [-i iface] [-c count] [-s snaplen] [-L linktype]
 *                  [-b BSSID] [-p]
 *   -w file      output pcap file (required)
 *   -i iface     interface to bind to (default: all interfaces)
 *   -c count     stop after this many saved packets
 *   -s snaplen   max bytes saved per packet (default 65535)
 *   -L linktype  pcap link type (default 1 = Ethernet;
 *                127 = 802.11 + radiotap, 119 = 802.11 + prism, 105 = raw 802.11)
 *   -b BSSID     keep only 802.11 frames to/from/within this BSSID
 *                (aa:bb:cc:dd:ee:ff). Needs -L 127, 119 or 105.
 *   -p           enable promiscuous mode on the interface
 *
 * Stops cleanly on SIGINT/SIGTERM. Every packet is written straight to the
 * file with write(), so a UI polling stat() sees the size grow in real time.
 * Needs root (CAP_NET_RAW).
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>

#ifndef ETH_P_ALL
#define ETH_P_ALL 0x0003
#endif

struct pcap_file_hdr {
    uint32_t magic;      /* 0xa1b2c3d4, written in host byte order */
    uint16_t ver_major;
    uint16_t ver_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t linktype;
};

struct pcap_pkt_hdr {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/*
 * Does this 802.11 frame belong to the given BSSID?
 * Data frames carry the BSSID in addr1 or addr2, management frames in addr3
 * (and often addr1/addr2), ACK/CTS control frames only have addr1. So match
 * the BSSID in any address field that exists in the frame.
 * Returns 1 to keep, 0 to drop. Non-802.11 link types are never filtered.
 */
static int frame_matches(const uint8_t *p, ssize_t n, long linktype,
                         const uint8_t *bssid)
{
    ssize_t off;

    if (linktype == 127) {                 /* radiotap: it_len at bytes 2-3, LE */
        if (n < 8 || p[0] != 0) return 0;
        off = (ssize_t)(p[2] | (p[3] << 8));
    } else if (linktype == 119) {          /* prism: fixed 144-byte header */
        off = 144;
    } else if (linktype == 105) {          /* raw 802.11 */
        off = 0;
    } else {
        return 1;
    }
    if (off < 0 || off >= n) return 0;
    p += off;
    n -= off;
    if (n < 10) return 0;

    int type = (p[0] >> 2) & 3;            /* 0 mgmt, 1 control, 2 data */
    if (!memcmp(p + 4, bssid, 6)) return 1;                       /* addr1 */
    if (n >= 16 && !memcmp(p + 10, bssid, 6)) return 1;           /* addr2 */
    if (type != 1 && n >= 22 && !memcmp(p + 16, bssid, 6)) return 1; /* addr3 */
    return 0;
}

static int parse_mac(const char *s, uint8_t out[6])
{
    unsigned v[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
        return -1;
    for (int i = 0; i < 6; i++) {
        if (v[i] > 255) return -1;
        out[i] = (uint8_t)v[i];
    }
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s -w out.pcap [-i iface] [-c count] [-s snaplen] "
            "[-L linktype] [-b BSSID] [-p]\n", argv0);
}

#ifndef GEOS_DUMP_NO_MAIN
int main(int argc, char **argv)
{
    const char *outpath = NULL, *iface = NULL;
    long count = 0, snaplen = 65535, linktype = 1;
    int promisc = 0, have_bssid = 0, opt;
    uint8_t bssid[6] = {0};

    while ((opt = getopt(argc, argv, "w:i:c:s:L:b:ph")) != -1) {
        switch (opt) {
        case 'w': outpath = optarg; break;
        case 'i': iface = optarg; break;
        case 'c': count = atol(optarg); break;
        case 's': snaplen = atol(optarg); break;
        case 'L': linktype = atol(optarg); break;
        case 'b':
            if (parse_mac(optarg, bssid) < 0) {
                fprintf(stderr, "bad BSSID: %s\n", optarg);
                return 2;
            }
            have_bssid = 1;
            break;
        case 'p': promisc = 1; break;
        default: usage(argv[0]); return opt == 'h' ? 0 : 2;
        }
    }
    if (!outpath) { usage(argv[0]); return 2; }
    if (snaplen < 64) snaplen = 64;
    if (snaplen > 262144) snaplen = 262144;
    if (have_bssid && linktype != 127 && linktype != 119 && linktype != 105) {
        fprintf(stderr, "warning: -b ignored, link type %ld is not 802.11\n", linktype);
        have_bssid = 0;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;           /* no SA_RESTART: recv must return EINTR */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        fprintf(stderr, "socket(AF_PACKET): %s (need root)\n", strerror(errno));
        return 1;
    }

    if (iface) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof ifr);
        strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
            fprintf(stderr, "interface %s: %s\n", iface, strerror(errno));
            return 1;
        }
        int ifindex = ifr.ifr_ifindex;

        struct sockaddr_ll sll;
        memset(&sll, 0, sizeof sll);
        sll.sll_family = AF_PACKET;
        sll.sll_protocol = htons(ETH_P_ALL);
        sll.sll_ifindex = ifindex;
        if (bind(sock, (struct sockaddr *)&sll, sizeof sll) < 0) {
            fprintf(stderr, "bind: %s\n", strerror(errno));
            return 1;
        }
        if (promisc) {
            struct packet_mreq mr;
            memset(&mr, 0, sizeof mr);
            mr.mr_ifindex = ifindex;
            mr.mr_type = PACKET_MR_PROMISC;
            if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof mr) < 0)
                fprintf(stderr, "warning: promisc failed: %s\n", strerror(errno));
        }
    }

    int fd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", outpath, strerror(errno));
        return 1;
    }

    struct pcap_file_hdr fh = {
        .magic = 0xa1b2c3d4, .ver_major = 2, .ver_minor = 4,
        .thiszone = 0, .sigfigs = 0,
        .snaplen = (uint32_t)snaplen, .linktype = (uint32_t)linktype,
    };
    if (write_all(fd, &fh, sizeof fh) < 0) {
        fprintf(stderr, "write header: %s\n", strerror(errno));
        return 1;
    }

    uint8_t *buf = malloc(262144);
    if (!buf) return 1;

    long captured = 0;
    while (!g_stop && (count == 0 || captured < count)) {
        ssize_t n = recvfrom(sock, buf, 262144, 0, NULL, NULL);
        if (n < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "recvfrom: %s\n", strerror(errno));
            break;
        }
        if (have_bssid && !frame_matches(buf, n, linktype, bssid))
            continue;

        struct timeval tv;
        gettimeofday(&tv, NULL);

        uint32_t incl = (uint32_t)(n > snaplen ? snaplen : n);
        struct pcap_pkt_hdr ph = {
            .ts_sec = (uint32_t)tv.tv_sec, .ts_usec = (uint32_t)tv.tv_usec,
            .incl_len = incl, .orig_len = (uint32_t)n,
        };
        /* write() goes straight to the kernel, so file size is current */
        if (write_all(fd, &ph, sizeof ph) < 0 || write_all(fd, buf, incl) < 0) {
            fprintf(stderr, "write: %s\n", strerror(errno));
            break;
        }
        captured++;
    }

    fsync(fd);
    close(fd);
    close(sock);
    free(buf);
    fprintf(stderr, "%ld packets captured\n", captured);
    return 0;
}
#endif /* GEOS_DUMP_NO_MAIN */
