#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

struct pcap_file_header {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
};

struct pcap_pkthdr {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <interface> [output.pcap]\n", argv[0]);
        return 1;
    }

    char *iface = argv[1];
    char *outfile = (argc > 2) ? argv[2] : NULL;

    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX failed");
        close(sock);
        return 1;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    FILE *f = NULL;
    if (outfile) {
        f = fopen(outfile, "wb");
        if (!f) {
            perror("Failed to open output file");
            close(sock);
            return 1;
        }
        struct pcap_file_header pfh = {
            .magic = 0xa1b2c3d4,
            .version_major = 2,
            .version_minor = 4,
            .thiszone = 0,
            .sigfigs = 0,
            .snaplen = 65535,
            .network = 105 // DLT_IEEE802_11 for monitor mode
        };
        fwrite(&pfh, sizeof(pfh), 1, f);
        fflush(f);
        printf("Writing PCAP capture to %s...\n", outfile);
    } else {
        printf("Listening for raw frames on %s...\n", iface);
    }

    unsigned char buffer[65536];
    long packet_count = 0;

    while (1) {
        ssize_t len = recv(sock, buffer, sizeof(buffer), 0);
        if (len < 0) {
            perror("Receive error");
            break;
        }
        packet_count++;

        if (f) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            struct pcap_pkthdr pkh = {
                .ts_sec = tv.tv_sec,
                .ts_usec = tv.tv_usec,
                .incl_len = len,
                .orig_len = len
            };
            fwrite(&pkh, sizeof(pkh), 1, f);
            fwrite(buffer, 1, len, f);
            fflush(f);
        } else {
            printf("[Packet #%ld] Length: %zd bytes\n", packet_count, len);
            fflush(stdout);
        }
    }

    if (f) fclose(f);
    close(sock);
    return 0;
}
