#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "platform.h"
#include "font.h"
#include "executor.h"
#include "keyboard.h"

/* Forward declarations for the NETWORK menu. */
static void show_page(
    bidhata_platform_t *p,
    const char *title,
    const char *line1,
    const char *line2,
    const char *line3);
static int wait_back_or_select(bidhata_platform_t *p);
static void show_wifi_scan(bidhata_platform_t *platform);
static void run_connectivity_screen(bidhata_platform_t *platform);

#include "tools.h"
#include "connectivity.h"

#define COL_BG       0xFF1A1A2E
#define COL_PANEL    0xFF16213E
#define COL_HILIGHT  0xFF0F3460
#define COL_ACCENT   0xFFE94560
#define COL_TEXT     0xFFEAEAEA
#define COL_DIM      0xFF8888A0

#define ROW_H 52
#define MARGIN 20
#define TOP 125

static void draw_header(bidhata_platform_t *p, const char *title)
{
    bidhata_platform_clear(p, COL_BG);

    bidhata_font_draw(p, MARGIN, 35, "GEOS", COL_DIM, 2);
    bidhata_font_draw(p, MARGIN, 65, title, COL_ACCENT, 3);

    bidhata_platform_fill_rect(
        p, MARGIN, 105,
        p->fb_width - MARGIN * 2,
        3,
        COL_ACCENT
    );
}

static void draw_item(
    bidhata_platform_t *p,
    int y,
    int selected,
    const char *label)
{
    u32 bg = selected ? COL_HILIGHT : COL_PANEL;

    bidhata_platform_fill_rect(
        p, MARGIN, y,
        p->fb_width - MARGIN * 2,
        ROW_H - 5,
        bg
    );

    if (selected) {
        bidhata_platform_fill_rect(
            p, MARGIN, y,
            5, ROW_H - 5,
            COL_ACCENT
        );
    }

    bidhata_font_draw(
        p,
        MARGIN + 18,
        y + 15,
        label,
        COL_TEXT,
        2
    );
}

static void draw_network_menu(
    bidhata_platform_t *platform,
    int selected)
{
    static const char *items[] = {
        "CONNECT TO WIFI",
        "WIFI ASSESS"
    };

    draw_header(platform, "NETWORK");

    for (int i = 0; i < 2; i++) {
        draw_item(
            platform,
            TOP + i * ROW_H,
            i == selected,
            items[i]
        );
    }

    bidhata_font_draw(
        platform,
        24,
        platform->fb_height - 28,
        "VOL +/- MOVE   NEXT SELECT",
        COL_DIM,
        1
    );
}

static void run_network_screen(
    bidhata_platform_t *platform)
{
    int selected = 0;
    bool dirty = true;

    for (;;) {
        if (dirty) {
            draw_network_menu(platform, selected);
            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key = bidhata_platform_poll_menu(
            platform, &tapped, &tap_x, &tap_y);

        if (tapped) {
            int row = (tap_y - TOP) / ROW_H;
            if (tap_y >= TOP && row >= 0 && row < 2) {
                selected = row;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {
        case BIDHATA_KEY_UP:
            selected = (selected + 1) % 2;
            dirty = true;
            break;

        case BIDHATA_KEY_DOWN:
            selected = (selected + 1) % 2;
            dirty = true;
            break;

        case BIDHATA_KEY_BACK:
            return;

        case BIDHATA_KEY_SELECT:
            if (selected == 0) {
                /* Existing Wi-Fi controls; connection selection comes next. */
                run_connectivity_screen(platform);
            } else {
                /* Assessment path stays separate from connecting. */
                char output[4096] = "";

                if (connectivity_wifi_on(output, sizeof(output)) < 0) {
                    show_page(
                        platform,
                        "WIFI ASSESS",
                        "WIFI START FAILED",
                        "Could not enable Wi-Fi",
                        "POWER BACK"
                    );
                    wait_back_or_select(platform);
                } else {
                    show_wifi_scan(platform);
                }
            }

            dirty = true;
            break;

        default:
            break;
        }

        usleep(50000);
    }
}

static void draw_main_menu(
    bidhata_platform_t *p,
    int selected)
{
    static const char *items[] = {
        "NETWORK",
        "RECON",
        "SYSTEM",
        "UTILITIES",
        "REPORTS",
        "EXIT"
    };

    draw_header(p, "GEOS CYBER");

    for (int i = 0; i < 6; i++) {
        draw_item(
            p,
            TOP + i * ROW_H,
            i == selected,
            items[i]
        );
    }

    bidhata_font_draw(
        p,
        MARGIN,
        p->fb_height - 28,
        "VOL +/- MOVE   NEXT SELECT",
        COL_DIM,
        1
    );
}

/*
 * Show a simple status/info page.
 * The actual command-driven tools will be added after the UI is proven.
 */
static void show_page(
    bidhata_platform_t *p,
    const char *title,
    const char *line1,
    const char *line2,
    const char *line3)
{
    draw_header(p, title);

    bidhata_font_draw(
        p, MARGIN, 145,
        line1, COL_TEXT, 2
    );

    bidhata_font_draw(
        p, MARGIN, 185,
        line2, COL_TEXT, 2
    );

    bidhata_font_draw(
        p, MARGIN, 225,
        line3, COL_DIM, 2
    );

    bidhata_font_draw(
        p,
        MARGIN,
        p->fb_height - 28,
        "POWER BACK",
        COL_DIM,
        1
    );
}

static int wait_back_or_select(bidhata_platform_t *p)
{
    for (;;) {
        bool tapped = false;
        int tx = 0, ty = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                p, &tapped, &tx, &ty
            );

        if (key == BIDHATA_KEY_BACK)
            return 0;

        if (key == BIDHATA_KEY_SELECT)
            return 1;

        usleep(50000);
    } 
}

static void run_ping_screen(bidhata_platform_t *platform)
{
    char target[64];
    char output[4096];
    executor_result_t result;

    if (!geos_keyboard_target(
            platform,
            target,
            sizeof(target),
            false))
        return;

    memset(output, 0, sizeof(output));

    /* Show running state. */
    bidhata_platform_clear(
        platform,
        0xFF1A1A2E
    );

    bidhata_font_draw(
        platform,
        24, 45,
        "PING",
        0xFFE94560,
        3
    );

    bidhata_font_draw(
        platform,
        24, 105,
        "Running...",
        0xFFEAEAEA,
        2
    );

    bidhata_font_draw(
        platform,
        24, 145,
        target,
        0xFF8888A0,
        2
    );

    tool_ping(
        target,
        output,
        sizeof(output),
        &result
    );

    /* Render result. */
    bidhata_platform_clear(
        platform,
        0xFF1A1A2E
    );

    bidhata_font_draw(
        platform,
        24, 35,
        "PING RESULT",
        0xFFE94560,
        2
    );

    int y = 80;
    const char *line = output;

    while (*line && y < platform->fb_height - 40) {
        char buf[81];
        size_t i = 0;

        while (line[i] &&
               line[i] != '\n' &&
               i < sizeof(buf) - 1) {
            buf[i] = line[i];
            i++;
        }

        buf[i] = '\0';

        bidhata_font_draw(
            platform,
            12,
            y,
            buf,
            0xFFEAEAEA,
            1
        );

        y += 16;

        if (line[i] == '\n')
            line += i + 1;
        else
            line += i;
    }

    bidhata_font_draw(
        platform,
        24,
        platform->fb_height - 28,
        "POWER BACK",
        0xFF8888A0,
        1
    );

    for (;;) {
        bidhata_key_t key =
            bidhata_platform_poll_key(platform);

        if (key == BIDHATA_KEY_BACK)
            return;

        usleep(50000);
    }
}

static void draw_connectivity_menu(
    bidhata_platform_t *platform,
    int selected)
{
    static const char *items[] = {
        "WIFI ON",
        "WIFI OFF",
        "WIFI STATUS",
        "WIFI ASSESS",
        "BACK"
    };

    draw_header(platform, "CONNECTIVITY");

    for (int i = 0; i < 5; i++) {
        draw_item(
            platform,
            TOP + i * ROW_H,
            i == selected,
            items[i]
        );
    }

    bidhata_font_draw(
        platform,
        24,
        platform->fb_height - 28,
        "VOL +/- MOVE   NEXT SELECT",
        0xFF8888A0,
        1
    );
}
static void show_wifi_status(
    bidhata_platform_t *platform)
{
    geos_wifi_status_t status;

    if (connectivity_wifi_status(&status) < 0) {
        show_page(
            platform,
            "WIFI STATUS",
            "STATUS ERROR",
            "Could not read Wi-Fi",
            "POWER BACK"
        );
        wait_back_or_select(platform);
        return;
    }

    char line[96];

    bidhata_platform_clear(platform, COL_BG);
    draw_header(platform, "WIFI STATUS");

    snprintf(
        line,
        sizeof(line),
        "STATE: %s",
        status.connected ? "CONNECTED" : "DISCONNECTED"
    );
    bidhata_font_draw(
        platform,
        24,
        145,
        line,
        status.connected ? COL_ACCENT : COL_DIM,
        2
    );

    snprintf(
        line,
        sizeof(line),
        "SSID: %s",
        status.ssid[0] ? status.ssid : "-"
    );
    bidhata_font_draw(
        platform,
        24,
        205,
        line,
        COL_DIM,
        2
    );

    snprintf(
        line,
        sizeof(line),
        "IP: %s",
        status.ip[0] ? status.ip : "-"
    );
    bidhata_font_draw(
        platform,
        24,
        265,
        line,
        COL_DIM,
        2
    );

    snprintf(
        line,
        sizeof(line),
        "GATEWAY: %s",
        status.gateway[0] ? status.gateway : "-"
    );
    bidhata_font_draw(
        platform,
        24,
        325,
        line,
        COL_DIM,
        2
    );

    snprintf(
        line,
        sizeof(line),
        "DNS: %s",
        status.dns[0] ? status.dns : "-"
    );
    bidhata_font_draw(
        platform,
        24,
        385,
        line,
        COL_DIM,
        2
    );

    /*
     * A valid IPv4 address means the interface has received
     * network configuration. The udhcpc process itself may
     * no longer appear in ps after the lease is established.
     */
    const bool dhcp_leased = status.ip[0] != '\0';

    snprintf(
        line,
        sizeof(line),
        "DHCP: %s",
        dhcp_leased ? "LEASED" : "NOT DETECTED"
    );
    bidhata_font_draw(
        platform,
        24,
        445,
        line,
        status.dhcp_active ? COL_ACCENT : COL_DIM,
        2
    );

    bidhata_font_draw(
        platform,
        24,
        platform->fb_height - 28,
        "POWER BACK",
        COL_DIM,
        1
    );

    wait_back_or_select(platform);
}



#define GEOS_WIFI_MAX_APS 32

struct geos_wifi_ap {
    char bssid[24];
    char frequency[16];
    char signal[16];
    char flags[128];
    char ssid[64];
};

static void geos_wifi_copy_field(
    char *dst,
    size_t cap,
    const char *src)
{
    if (!dst || cap == 0)
        return;

    dst[0] = '\0';

    if (!src)
        return;

    while (*src == ' ' || *src == '\t')
        src++;

    size_t len = strlen(src);

    while (len > 0 &&
           (src[len - 1] == '\r' ||
            src[len - 1] == '\n' ||
            src[len - 1] == ' ')) {
        len--;
    }

    if (len >= cap)
        len = cap - 1;

    memcpy(dst, src, len);
    dst[len] = '\0';
}

static const char *geos_wifi_band(int mhz)
{
    if (mhz >= 2400 && mhz < 2500)
        return "2.4 GHz";

    if (mhz >= 5000 && mhz < 5925)
        return "5 GHz";

    if (mhz >= 5925 && mhz <= 7125)
        return "6 GHz";

    return "Unknown";
}

static int geos_wifi_channel(int mhz)
{
    if (mhz == 2484)
        return 14;

    if (mhz >= 2412 && mhz <= 2472 &&
        (mhz - 2407) % 5 == 0)
        return (mhz - 2407) / 5;

    if (mhz >= 5000 && mhz < 5925 &&
        mhz % 5 == 0)
        return (mhz - 5000) / 5;

    if (mhz >= 5955 && mhz <= 7125 &&
        (mhz - 5950) % 5 == 0)
        return (mhz - 5950) / 5;

    return 0;
}

static void geos_wifi_security(
    const char *flags,
    char *security,
    size_t security_cap,
    char *cipher,
    size_t cipher_cap)
{
    if (!security || security_cap == 0 ||
        !cipher || cipher_cap == 0)
        return;

    snprintf(security, security_cap, "Not reported");
    snprintf(cipher, cipher_cap, "Not reported");

    if (!flags || !flags[0])
        return;

    if (strstr(flags, "SAE")) {
        if (strstr(flags, "WPA2-PSK"))
            snprintf(security, security_cap, "WPA2/WPA3 Personal");
        else
            snprintf(security, security_cap, "WPA3 Personal");
    } else if (strstr(flags, "WPA2-EAP")) {
        snprintf(security, security_cap, "WPA2 Enterprise");
    } else if (strstr(flags, "WPA-EAP")) {
        snprintf(security, security_cap, "WPA Enterprise");
    } else if (strstr(flags, "WPA2-PSK")) {
        snprintf(security, security_cap, "WPA2 Personal");
    } else if (strstr(flags, "WPA-PSK")) {
        snprintf(security, security_cap, "WPA Personal");
    } else if (strstr(flags, "OWE")) {
        snprintf(security, security_cap, "OWE");
    } else if (strstr(flags, "WEP")) {
        snprintf(security, security_cap, "WEP (legacy)");
    } else if (strstr(flags, "[ESS]") ||
               strstr(flags, "[IBSS]")) {
        snprintf(security, security_cap, "Open / no security flags");
    } else {
        snprintf(security, security_cap, "Unknown; inspect flags");
    }

    int ccmp = strstr(flags, "CCMP") != NULL;
    int gcmp = strstr(flags, "GCMP") != NULL;
    int tkip = strstr(flags, "TKIP") != NULL;

    if (ccmp && tkip)
        snprintf(cipher, cipher_cap, "CCMP/AES + TKIP");
    else if (gcmp && ccmp)
        snprintf(cipher, cipher_cap, "GCMP + CCMP/AES");
    else if (gcmp)
        snprintf(cipher, cipher_cap, "GCMP");
    else if (ccmp)
        snprintf(cipher, cipher_cap, "CCMP/AES");
    else if (tkip)
        snprintf(cipher, cipher_cap, "TKIP");
    else if (strstr(flags, "WEP"))
        snprintf(cipher, cipher_cap, "WEP");
    else if (strstr(security, "Open"))
        snprintf(cipher, cipher_cap, "None indicated");
}


static int geos_wifi_save_report(
    const struct geos_wifi_ap *ap,
    char *saved_path,
    size_t saved_path_cap)
{
    if (!ap || !saved_path || saved_path_cap == 0)
        return -1;

    saved_path[0] = '\0';

    geos_wifi_status_t status;
    memset(&status, 0, sizeof(status));

    int status_rc = connectivity_wifi_status(&status);

    time_t now = time(NULL);
    char timestamp[32] = "time-unavailable";
    struct tm *tm_now = localtime(&now);

    if (now != (time_t)-1 && tm_now) {
        if (strftime(
                timestamp,
                sizeof(timestamp),
                "%Y%m%d_%H%M%S",
                tm_now) == 0) {
            snprintf(timestamp, sizeof(timestamp), "time-unavailable");
        }
    }

    char path[192];

    snprintf(
        path,
        sizeof(path),
        "/data/mnt/sd_0/geos_wifi_report_%s.txt",
        timestamp
    );

    int mhz = atoi(ap->frequency);
    int channel = geos_wifi_channel(mhz);

    char security[64];
    char cipher[64];

    geos_wifi_security(
        ap->flags,
        security, sizeof(security),
        cipher, sizeof(cipher)
    );

    FILE *fp = fopen(path, "w");

    if (!fp)
        return -1;

    fprintf(fp, "GEOS WIFI ASSESSMENT REPORT\n");
    fprintf(fp, "========================================\n");
    fprintf(fp, "Report time: %s\n", timestamp);
    fprintf(fp, "Report type: Access-point and network metadata\n\n");

    fprintf(fp, "[SELECTED ACCESS POINT]\n");
    fprintf(fp, "SSID: %s\n", ap->ssid[0] ? ap->ssid : "Unavailable");
    fprintf(fp, "BSSID: %s\n", ap->bssid[0] ? ap->bssid : "Unavailable");
    fprintf(fp, "Signal: %s dBm\n", ap->signal[0] ? ap->signal : "Unavailable");
    fprintf(fp, "Frequency: %s MHz\n", ap->frequency[0] ? ap->frequency : "Unavailable");
    fprintf(fp, "Band: %s\n", geos_wifi_band(mhz));

    if (channel > 0)
        fprintf(fp, "Channel: %d\n", channel);
    else
        fprintf(fp, "Channel: Unknown\n");

    fprintf(fp, "Advertised security summary: %s\n", security);
    fprintf(fp, "Advertised encryption summary: %s\n", cipher);
    fprintf(fp, "Raw advertised flags: %s\n", ap->flags[0] ? ap->flags : "Unavailable");
    fprintf(fp, "Note: Security details reflect advertised scan flags, not a cryptographic audit.\n\n");

    fprintf(fp, "[R1 NETWORK CONTEXT]\n");
    fprintf(fp, "Wi-Fi status query: %s\n", status_rc == 0 ? "OK" : "ERROR");

    if (status_rc == 0) {
        fprintf(fp, "Connection state: %s\n", status.connected ? "CONNECTED" : "DISCONNECTED");
        fprintf(fp, "Connected SSID: %s\n", status.ssid[0] ? status.ssid : "Unavailable");
        fprintf(fp, "Device IP: %s\n", status.ip[0] ? status.ip : "Unavailable");
        fprintf(fp, "Default gateway: %s\n", status.gateway[0] ? status.gateway : "Unavailable");
        fprintf(fp, "DNS server: %s\n", status.dns[0] ? status.dns : "Unavailable");
        fprintf(fp, "DHCP configuration: %s\n", status.dhcp_active ? "LEASED / CONFIGURED" : "NOT CONFIRMED");
    } else {
        fprintf(fp, "Connection state: Unavailable\n");
        fprintf(fp, "Device IP: Unavailable\n");
        fprintf(fp, "Default gateway: Unavailable\n");
        fprintf(fp, "DNS server: Unavailable\n");
        fprintf(fp, "DHCP configuration: Unavailable\n");
    }

    fprintf(fp, "\n[PASSIVE CAPTURE STATUS]\n");
    fprintf(fp, "Raw 802.11 packet capture: NOT RUNNING\n");
    fprintf(fp, "Monitor-mode support: NOT VERIFIED\n");
    fprintf(fp, "Handshake observations: NOT MEASURED\n");
    fprintf(fp, "Reason: This report records network metadata; a raw wireless-frame capture was not active.\n");

    int write_error = ferror(fp);
    if (fclose(fp) != 0)
        write_error = 1;

    if (write_error)
        return -1;

    snprintf(saved_path, saved_path_cap, "%s", path);
    return 0;
}

static void run_active_monitor_screen(bidhata_platform_t *platform, const char *bssid, const char *frequency);

static void show_wireless_monitor(
    bidhata_platform_t *platform,
    const struct geos_wifi_ap *target);

static int show_wifi_ap_details(
    bidhata_platform_t *platform,
    const struct geos_wifi_ap *ap)
{
    if (!ap)
        return 0;

    geos_wifi_status_t status;
    memset(&status, 0, sizeof(status));

    int status_rc = connectivity_wifi_status(&status);
    int connected_network =
        status_rc == 0 &&
        status.connected &&
        strcmp(status.ssid, ap->ssid) == 0;

    int mhz = atoi(ap->frequency);
    int channel = geos_wifi_channel(mhz);

    char security[64];
    char cipher[64];
    geos_wifi_security(
        ap->flags,
        security, sizeof(security),
        cipher, sizeof(cipher)
    );

    int selected_action = 0;
    bool dirty = true;

    for (;;) {
        if (dirty) {
            char line[128];

            bidhata_platform_clear(platform, COL_BG);
            draw_header(platform, "WIFI ASSESS");

            snprintf(line, sizeof(line), "SSID: %.95s", ap->ssid);
            bidhata_font_draw(
                platform, 18, 120, line, COL_ACCENT, 1
            );

            snprintf(line, sizeof(line), "BSSID: %.50s", ap->bssid);
            bidhata_font_draw(
                platform, 18, 146, line, COL_DIM, 1
            );

            snprintf(
                line, sizeof(line),
                "SIGNAL: %.12s dBm", ap->signal
            );
            bidhata_font_draw(
                platform, 18, 172, line, COL_DIM, 1
            );

            snprintf(
                line, sizeof(line),
                "FREQUENCY: %d MHz", mhz
            );
            bidhata_font_draw(
                platform, 18, 198, line, COL_DIM, 1
            );

            if (channel > 0) {
                snprintf(
                    line, sizeof(line),
                    "BAND: %s  CHANNEL: %d",
                    geos_wifi_band(mhz), channel
                );
            } else {
                snprintf(
                    line, sizeof(line),
                    "BAND: %s  CHANNEL: UNKNOWN",
                    geos_wifi_band(mhz)
                );
            }

            bidhata_font_draw(
                platform, 18, 224, line, COL_DIM, 1
            );

            snprintf(
                line, sizeof(line),
                "SECURITY: %.70s", security
            );
            bidhata_font_draw(
                platform, 18, 250, line, COL_DIM, 1
            );

            snprintf(
                line, sizeof(line),
                "ENCRYPTION: %.65s", cipher
            );
            bidhata_font_draw(
                platform, 18, 276, line, COL_DIM, 1
            );

            snprintf(
                line, sizeof(line),
                "FLAGS: %.95s", ap->flags[0] ? ap->flags : "-"
            );
            bidhata_font_draw(
                platform, 18, 302, line, COL_DIM, 1
            );

            snprintf(
                line, sizeof(line),
                "NETWORK: %s",
                connected_network
                    ? "CONNECTED SSID"
                    : "NEARBY / NOT ACTIVE SSID"
            );
            bidhata_font_draw(
                platform, 18, 328, line, COL_DIM, 1
            );

            int save_y = platform->fb_height - 126;
            int monitor_y = platform->fb_height - 96;
            int rescan_y = platform->fb_height - 66;
            int back_y = platform->fb_height - 36;

            bidhata_font_draw(
                platform,
                24, save_y,
                selected_action == 0 ? "> SAVE REPORT" : "  SAVE REPORT",
                selected_action == 0 ? COL_ACCENT : COL_DIM,
                2
            );

            bidhata_font_draw(
                platform,
                24, monitor_y,
                selected_action == 1
                    ? "> MONITOR THIS AP" : "  MONITOR THIS AP",
                selected_action == 1 ? COL_ACCENT : COL_DIM,
                2
            );

            bidhata_font_draw(
                platform,
                24, rescan_y,
                selected_action == 2 ? "> RESCAN" : "  RESCAN",
                selected_action == 2 ? COL_ACCENT : COL_DIM,
                2
            );

            bidhata_font_draw(
                platform,
                24, back_y,
                selected_action == 3
                    ? "> BACK TO NETWORKS"
                    : "  BACK TO NETWORKS",
                selected_action == 3 ? COL_ACCENT : COL_DIM,
                2
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key = bidhata_platform_poll_menu(
            platform, &tapped, &tap_x, &tap_y
        );

        (void)tap_x;

        if (tapped) {
            int height = platform->fb_height;

            if (tap_y >= height - 144 && tap_y < height - 114) {
                selected_action = 0;
                key = BIDHATA_KEY_SELECT;
            } else if (tap_y >= height - 114 && tap_y < height - 84) {
                selected_action = 1;
                key = BIDHATA_KEY_SELECT;
            } else if (tap_y >= height - 84 && tap_y < height - 54) {
                selected_action = 2;
                key = BIDHATA_KEY_SELECT;
            } else if (tap_y >= height - 54 && tap_y < height - 20) {
                selected_action = 3;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {
        case BIDHATA_KEY_UP:
            selected_action = (selected_action + 3) % 4;
            dirty = true;
            break;

        case BIDHATA_KEY_DOWN:
            selected_action = (selected_action + 1) % 4;
            dirty = true;
            break;

        case BIDHATA_KEY_BACK:
            return 0;

        case BIDHATA_KEY_SELECT:
            if (selected_action == 0) {
                char report_path[192] = "";

                if (geos_wifi_save_report(
                        ap,
                        report_path,
                        sizeof(report_path)) == 0) {

                    const char *filename = strrchr(report_path, '/');
                    filename = filename ? filename + 1 : report_path;

                    show_page(
                        platform,
                        "REPORT SAVED",
                        "Saved to SD card",
                        filename,
                        "POWER BACK"
                    );
                } else {
                    show_page(
                        platform,
                        "REPORT ERROR",
                        "Could not save report",
                        "Check SD card mount",
                        "POWER BACK"
                    );
                }

                wait_back_or_select(platform);
                dirty = true;
                break;
            }

            if (selected_action == 1) {
                // Call active monitor tcpdump capture screen
                run_active_monitor_screen(platform, ap->bssid, ap->frequency);
                dirty = true;
                break;
            }

            if (selected_action == 2)
                return 1;

            return 0;

        default:
            break;
        }

        usleep(50000);
    }
}

static void show_wifi_scan(
    bidhata_platform_t *platform)
{
    for (;;) {
        char output[8192] = "";

        if (connectivity_wifi_scan(
                output, sizeof(output)) < 0) {
            show_page(
                platform,
                "WIFI ASSESS",
                "SCAN ERROR",
                "Check Wi-Fi connection",
                "POWER BACK"
            );
            wait_back_or_select(platform);
            return;
        }

        struct geos_wifi_ap aps[GEOS_WIFI_MAX_APS];
        memset(aps, 0, sizeof(aps));

        int count = 0;
        char *save_line = NULL;
        char *line = strtok_r(output, "\n", &save_line);

        while (line && count < GEOS_WIFI_MAX_APS) {
            if (strstr(line, "bssid / frequency") ||
                strstr(line, "----")) {
                line = strtok_r(NULL, "\n", &save_line);
                continue;
            }

            char *save_field = NULL;
            char *bssid = strtok_r(line, "\t", &save_field);
            char *freq = strtok_r(NULL, "\t", &save_field);
            char *signal = strtok_r(NULL, "\t", &save_field);
            char *flags = strtok_r(NULL, "\t", &save_field);
            char *ssid = strtok_r(NULL, "\t", &save_field);

            if (!bssid || !freq || !signal || !flags) {
                line = strtok_r(NULL, "\n", &save_line);
                continue;
            }

            struct geos_wifi_ap *ap = &aps[count];

            geos_wifi_copy_field(
                ap->bssid, sizeof(ap->bssid), bssid
            );
            geos_wifi_copy_field(
                ap->frequency, sizeof(ap->frequency), freq
            );
            geos_wifi_copy_field(
                ap->signal, sizeof(ap->signal), signal
            );
            geos_wifi_copy_field(
                ap->flags, sizeof(ap->flags), flags
            );

            if (ssid && ssid[0]) {
                geos_wifi_copy_field(
                    ap->ssid, sizeof(ap->ssid), ssid
                );
            } else {
                snprintf(ap->ssid, sizeof(ap->ssid), "<HIDDEN>");
            }

            /* Avoid listing the same BSSID twice. */
            int duplicate = 0;
            for (int i = 0; i < count; i++) {
                if (strcmp(aps[i].bssid, ap->bssid) == 0) {
                    duplicate = 1;
                    break;
                }
            }

            if (!duplicate)
                count++;

            line = strtok_r(NULL, "\n", &save_line);
        }

        int selected = 0;
        int scroll = 0;
        int row_height = 28;
        int list_y = 135;

        int visible =
            (platform->fb_height - list_y - 65) / row_height;

        if (visible < 1)
            visible = 1;
        if (visible > GEOS_WIFI_MAX_APS)
            visible = GEOS_WIFI_MAX_APS;

        bool dirty = true;
        bool rescan_requested = false;

        for (;;) {
            if (dirty) {
                bidhata_platform_clear(platform, COL_BG);
                draw_header(platform, "WIFI ASSESS");

                if (count == 0) {
                    bidhata_font_draw(
                        platform, 24, 160,
                        "NO NETWORKS FOUND",
                        COL_DIM, 2
                    );

                    bidhata_font_draw(
                        platform, 18,
                        platform->fb_height - 28,
                        "SELECT RESCAN  POWER BACK",
                        COL_DIM, 1
                    );
                } else {
                    int end = scroll + visible;
                    if (end > count)
                        end = count;

                    for (int i = scroll; i < end; i++) {
                        char row[112];

                        snprintf(
                            row,
                            sizeof(row),
                            "%s dBm  %.76s",
                            aps[i].signal,
                            aps[i].ssid
                        );

                        bidhata_font_draw(
                            platform,
                            18,
                            list_y + (i - scroll) * row_height,
                            row,
                            i == selected ? COL_ACCENT : COL_DIM,
                            1
                        );
                    }

                    char footer[64];
                    snprintf(
                        footer, sizeof(footer),
                        "%d ACCESS POINTS", count
                    );

                    bidhata_font_draw(
                        platform, 18,
                        platform->fb_height - 48,
                        footer, COL_DIM, 1
                    );

                    bidhata_font_draw(
                        platform, 18,
                        platform->fb_height - 26,
                        "TAP NETWORK TO INSPECT  POWER BACK",
                        COL_DIM, 1
                    );
                }

                dirty = false;
            }

            bool tapped = false;
            int tap_x = 0;
            int tap_y = 0;

            bidhata_key_t key = bidhata_platform_poll_menu(
                platform, &tapped, &tap_x, &tap_y
            );

            (void)tap_x;

            if (tapped && count > 0 &&
                tap_y >= list_y &&
                tap_y < list_y + visible * row_height) {
                int row = (tap_y - list_y) / row_height;
                int index = scroll + row;

                if (index >= 0 && index < count) {
                    selected = index;
                    key = BIDHATA_KEY_SELECT;
                }
            }

            switch (key) {
            case BIDHATA_KEY_UP:
                if (count > 0 && selected > 0) {
                    selected--;
                    if (selected < scroll)
                        scroll = selected;
                    dirty = true;
                }
                break;

            case BIDHATA_KEY_DOWN:
                if (count > 0 && selected < count - 1) {
                    selected++;
                    if (selected >= scroll + visible)
                        scroll = selected - visible + 1;
                    dirty = true;
                }
                break;

            case BIDHATA_KEY_BACK:
                return;

            case BIDHATA_KEY_SELECT:
                if (count == 0) {
                    rescan_requested = true;
                    break;
                }

                if (show_wifi_ap_details(
                        platform, &aps[selected]) == 1) {
                    rescan_requested = true;
                    break;
                }

                dirty = true;
                break;

            default:
                break;
            }

            if (rescan_requested)
                break;

            usleep(50000);
        }

        if (!rescan_requested)
            return;
    }
}


static int geos_wifi_parse_monitor_scan(
    char *output,
    struct geos_wifi_ap *aps,
    int max_aps)
{
    memset(aps, 0, sizeof(*aps) * (size_t)max_aps);
    int count = 0;
    char *save_line = NULL;
    char *line = strtok_r(output, "\n", &save_line);

    while (line && count < max_aps) {
        if (strstr(line, "bssid / frequency") ||
            strstr(line, "----")) {
            line = strtok_r(NULL, "\n", &save_line);
            continue;
        }

        char *save_field = NULL;
        char *bssid = strtok_r(line, "\t", &save_field);
        char *freq = strtok_r(NULL, "\t", &save_field);
        char *signal = strtok_r(NULL, "\t", &save_field);
        char *flags = strtok_r(NULL, "\t", &save_field);
        char *ssid = strtok_r(NULL, "\t", &save_field);

        if (!bssid || !freq || !signal || !flags) {
            line = strtok_r(NULL, "\n", &save_line);
            continue;
        }

        struct geos_wifi_ap ap = {0};
        geos_wifi_copy_field(ap.bssid, sizeof(ap.bssid), bssid);
        geos_wifi_copy_field(ap.frequency, sizeof(ap.frequency), freq);
        geos_wifi_copy_field(ap.signal, sizeof(ap.signal), signal);
        geos_wifi_copy_field(ap.flags, sizeof(ap.flags), flags);

        if (ssid && ssid[0])
            geos_wifi_copy_field(ap.ssid, sizeof(ap.ssid), ssid);
        else
            snprintf(ap.ssid, sizeof(ap.ssid), "<HIDDEN>");

        int duplicate = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(aps[i].bssid, ap.bssid) == 0) {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate)
            aps[count++] = ap;

        line = strtok_r(NULL, "\n", &save_line);
    }

    return count;
}

static void geos_wifi_monitor_log(
    FILE *fp,
    const char *event,
    const struct geos_wifi_ap *ap)
{
    time_t now = time(NULL);
    struct tm tm_now;
    char stamp[32] = "time-unknown";

    if (localtime_r(&now, &tm_now))
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_now);

    if (ap) {
        int channel = geos_wifi_channel(atoi(ap->frequency));
        fprintf(fp, "%s | %-16s | %s | %s | %s MHz | CH %d | %s dBm | %s\n",
                stamp, event, ap->bssid, ap->ssid,
                ap->frequency, channel, ap->signal, ap->flags);
    } else {
        fprintf(fp, "%s | %s\n", stamp, event);
    }
    fflush(fp);
}

static void run_active_monitor_screen(bidhata_platform_t *platform, const char *bssid, const char *frequency);

static void show_wireless_monitor(
    bidhata_platform_t *platform,
    const struct geos_wifi_ap *target)
{
    if (!target)
        return;

    struct geos_wifi_ap previous[GEOS_WIFI_MAX_APS];
    struct geos_wifi_ap current[GEOS_WIFI_MAX_APS];
    memset(previous, 0, sizeof(previous));

    int previous_count = 0;
    int scan_number = 0;
    char last_event[96] = "Starting first scan...";
    char output[8192];

    time_t now = time(NULL);
    struct tm tm_now;
    char stamp[32];
    if (!localtime_r(&now, &tm_now) ||
        !strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm_now)) {
        snprintf(stamp, sizeof(stamp), "%ld", (long)now);
    }

    char path[160];
    snprintf(path, sizeof(path),
             "/data/mnt/sd_0/geos_ap_monitor_%s.txt", stamp);

    FILE *fp = fopen(path, "a");
    if (!fp) {
        show_page(platform, "AP MONITOR",
                  "LOG FILE ERROR", "Check SD card mount",
                  "POWER BACK");
        wait_back_or_select(platform);
        return;
    }

    fprintf(fp, "GEOS CYBER - PASSIVE AP MONITOR\n");
    fprintf(fp, "Target SSID: %s\n", target->ssid);
    fprintf(fp, "Target BSSID: %s\n", target->bssid);
    fprintf(fp, "Scan results are snapshots, not raw packet capture.\n\n");
    fflush(fp);

    for (;;) {
        memset(output, 0, sizeof(output));

        if (connectivity_wifi_scan(output, sizeof(output)) < 0) {
            snprintf(last_event, sizeof(last_event), "Scan failed");
            geos_wifi_monitor_log(fp, "SCAN_ERROR", NULL);
        } else {
            int parsed_count = geos_wifi_parse_monitor_scan(
                output, current, GEOS_WIFI_MAX_APS);

            int target_index = -1;
            for (int i = 0; i < parsed_count; i++) {
                if (strcmp(current[i].bssid, target->bssid) == 0) {
                    target_index = i;
                    break;
                }
            }

            int current_count = 0;
            if (target_index >= 0) {
                if (target_index != 0)
                    current[0] = current[target_index];
                current_count = 1;
            }

            scan_number++;
            char summary[80];
            snprintf(summary, sizeof(summary), "SCAN %d | APs: %d",
                     scan_number, current_count);
            geos_wifi_monitor_log(fp, summary, NULL);

            if (scan_number == 1) {
                for (int i = 0; i < current_count; i++)
                    geos_wifi_monitor_log(fp, "AP_SEEN_BASELINE", &current[i]);

                snprintf(last_event, sizeof(last_event),
                         "Baseline recorded: %d APs", current_count);
            } else {
                int changes = 0;

                for (int i = 0; i < current_count; i++) {
                    int found = -1;
                    for (int j = 0; j < previous_count; j++) {
                        if (strcmp(current[i].bssid, previous[j].bssid) == 0) {
                            found = j;
                            break;
                        }
                    }

                    if (found < 0) {
                        geos_wifi_monitor_log(fp, "AP_SEEN", &current[i]);
                        changes++;
                        snprintf(last_event, sizeof(last_event),
                                 "AP appeared: %.30s", current[i].ssid);
                        continue;
                    }

                    if (strcmp(current[i].frequency,
                               previous[found].frequency) != 0) {
                        geos_wifi_monitor_log(fp, "FREQUENCY_CHANGED",
                                              &current[i]);
                        changes++;
                        snprintf(last_event, sizeof(last_event),
                                 "Frequency changed: %.25s", current[i].ssid);
                    }

                    int new_signal = atoi(current[i].signal);
                    int old_signal = atoi(previous[found].signal);
                    if (new_signal && old_signal &&
                        abs(new_signal - old_signal) >= 8) {
                        geos_wifi_monitor_log(fp, "SIGNAL_CHANGED",
                                              &current[i]);
                        changes++;
                        snprintf(last_event, sizeof(last_event),
                                 "Signal changed: %.25s", current[i].ssid);
                    }
                }

                for (int i = 0; i < previous_count; i++) {
                    int found = 0;
                    for (int j = 0; j < current_count; j++) {
                        if (strcmp(previous[i].bssid, current[j].bssid) == 0) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        geos_wifi_monitor_log(fp, "AP_NOT_SEEN", &previous[i]);
                        changes++;
                        snprintf(last_event, sizeof(last_event),
                                 "Not seen this scan: %.24s", previous[i].ssid);
                    }
                }

                if (changes == 0)
                    snprintf(last_event, sizeof(last_event),
                             "No notable changes");
            }

            memcpy(previous, current,
                   sizeof(struct geos_wifi_ap) * (size_t)current_count);
            previous_count = current_count;
        }

        draw_header(platform, "AP MONITOR");

        char line[96];
        snprintf(line, sizeof(line), "SCAN: %d", scan_number);
        bidhata_font_draw(platform, 24, 145, line, COL_TEXT, 2);

        snprintf(line, sizeof(line), "TARGET SEEN: %s",
                 previous_count > 0 ? "YES" : "NO");
        bidhata_font_draw(platform, 24, 190, line, COL_TEXT, 2);

        snprintf(line, sizeof(line), "AP: %.40s", target->ssid);
        bidhata_font_draw(platform, 24, 220, line, COL_DIM, 1);

        if (previous_count > 0) {
            int observed_channel =
                geos_wifi_channel(atoi(previous[0].frequency));
            snprintf(line, sizeof(line), "OBSERVED CHANNEL: %d",
                     observed_channel);
        } else {
            snprintf(line, sizeof(line), "SELECTED CH: %d",
                     geos_wifi_channel(atoi(target->frequency)));
        }
        bidhata_font_draw(platform, 24, 240, line, COL_DIM, 1);

        bidhata_font_draw(platform, 24, 250, "LATEST EVENT", COL_ACCENT, 2);
        bidhata_font_draw(platform, 24, 285, last_event, COL_TEXT, 1);

        bidhata_font_draw(platform, 24, 350, "PASSIVE INVENTORY ONLY", COL_DIM, 2);
        bidhata_font_draw(platform, 24, 390, "Log saved on SD card:", COL_DIM, 1);
        bidhata_font_draw(platform, 24, 415, path, COL_DIM, 1);
        bidhata_font_draw(platform, 24, platform->fb_height - 50,
                          "SELECT RESCAN  POWER BACK", COL_DIM, 1);

        /* Remain responsive during the interval between scans. */
        int rescan_now = 0;
        for (int tick = 0; tick < 100; tick++) {
            bool tapped = false;
            int tap_x = 0, tap_y = 0;
            bidhata_key_t key = bidhata_platform_poll_menu(
                platform, &tapped, &tap_x, &tap_y);

            if (key == BIDHATA_KEY_BACK) {
                fclose(fp);
                return;
            }
            if (key == BIDHATA_KEY_SELECT || tapped) {
                rescan_now = 1;
                break;
            }
            usleep(50000);
        }

        (void)rescan_now;
    }
}

static void run_connectivity_screen(
    bidhata_platform_t *platform)
{
    int selected = 0;
    bool dirty = true;

    char output[4096];

    for (;;) {

        if (dirty) {
            draw_connectivity_menu(
                platform,
                selected
            );
            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                platform,
                &tapped,
                &tap_x,
                &tap_y
            );

        if (tapped) {
            int row = (tap_y - TOP) / ROW_H;

            if (tap_y >= TOP &&
                row >= 0 &&
                row < 6) {

                selected = row;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {

        case BIDHATA_KEY_UP:
            selected =
                (selected + 5) % 6;
            dirty = true;
            break;

        case BIDHATA_KEY_DOWN:
            selected =
                (selected + 1) % 6;
            dirty = true;
            break;

        case BIDHATA_KEY_BACK:
            return;

        case BIDHATA_KEY_SELECT:

            if (selected == 0) {
                memset(
                    output,
                    0,
                    sizeof(output)
                );

                connectivity_wifi_on(
                    output,
                    sizeof(output)
                );

                show_page(
                    platform,
                    "WIFI ON",
                    "REQUEST SENT",
                    "Starting Wi-Fi...",
                    "POWER BACK"
                );

                wait_back_or_select(platform);
                dirty = true;
            }

            else if (selected == 1) {
                memset(
                    output,
                    0,
                    sizeof(output)
                );

                connectivity_wifi_off(
                    output,
                    sizeof(output)
                );

                show_page(
                    platform,
                    "WIFI OFF",
                    "REQUEST SENT",
                    "Stopping Wi-Fi...",
                    "POWER BACK"
                );

                wait_back_or_select(platform);
                dirty = true;
            }

            else if (selected == 2) {
                show_wifi_status(platform);
                dirty = true;
            }
            else if (selected == 3) {
                show_wifi_scan(platform);
                dirty = true;
            }


            else if (selected == 4) {
                return;
            }

            break;

        default:
            break;
        }

        usleep(50000);
    }
}




static void run_dns_screen(
    bidhata_platform_t *platform)
{
    char target[128];
    char output[4096];
    executor_result_t result;

    memset(target, 0, sizeof(target));
    memset(output, 0, sizeof(output));
    memset(&result, 0, sizeof(result));

    /*
     * Enter target using the GEOS keyboard.
     */
    if (geos_keyboard_target(
            platform,
            target,
            sizeof(target),
            false) < 0) {
        return;
    }

    if (!target[0])
        return;

    /*
     * Execute the R1's real BusyBox nslookup.
     */
    tool_dns_lookup(
        target,
        output,
        sizeof(output),
        &result
    );

    /*
     * Parse useful DNS information out of nslookup.
     */
    char server[96] = "";
    char answers[16][128];

    int answer_count = 0;
    bool in_answer = false;

    memset(answers, 0, sizeof(answers));

    char *save = NULL;
    char *line = strtok_r(
        output,
        "\n",
        &save
    );

    while (line) {

        /*
         * Server information.
         */
        if (!server[0] &&
            strstr(line, "Server:")) {

            const char *p = strstr(
                line,
                "Server:"
            );

            p += strlen("Server:");

            while (*p == ' ' ||
                   *p == '\t')
                p++;

            snprintf(
                server,
                sizeof(server),
                "%.90s",
                p
            );
        }

        /*
         * The useful records begin after
         * "Non-authoritative answer:".
         */
        if (strstr(
                line,
                "Non-authoritative answer:")) {

            in_answer = true;
        }

        if (in_answer &&
            answer_count < 16) {

            /*
             * Reverse DNS:
             *
             * 8.8.8.8.in-addr.arpa
             * name = dns.google
             */
            char *ptr = strstr(
                line,
                "name = "
            );

            if (ptr) {

                ptr += strlen("name = ");

                while (*ptr == ' ' ||
                       *ptr == '\t')
                    ptr++;

                snprintf(
                    answers[answer_count],
                    sizeof(answers[answer_count]),
                    "PTR: %.112s",
                    ptr
                );

                answer_count++;
            }

            /*
             * Forward DNS:
             *
             * Name: google.com
             * Address: 64.233.x.x
             */
            else if (strstr(line, "Name:")) {

                char *name =
                    strstr(line, "Name:");

                name += strlen("Name:");

                while (*name == ' ' ||
                       *name == '\t')
                    name++;

                snprintf(
                    answers[answer_count],
                    sizeof(answers[answer_count]),
                    "NAME: %.112s",
                    name
                );

                answer_count++;
            }

            else if (strstr(line, "Address:")) {

                char *addr =
                    strstr(line, "Address:");

                addr += strlen("Address:");

                /*
                 * Skip empty Address lines.
                 */
                while (*addr == ' ' ||
                       *addr == '\t')
                    addr++;

                if (*addr) {

                    snprintf(
                        answers[answer_count],
                        sizeof(answers[answer_count]),
                        "ADDR: %.112s",
                        addr
                    );

                    answer_count++;
                }
            }
        }

        line = strtok_r(
            NULL,
            "\n",
            &save
        );
    }

    /*
     * Build display lines.
     */
    char lines[20][128];
    int line_count = 0;

    memset(lines, 0, sizeof(lines));

    snprintf(
        lines[line_count++],
        sizeof(lines[0]),
        "SERVER: %s",
        server[0] ? server : "-"
    );

    snprintf(
        lines[line_count++],
        sizeof(lines[0]),
        "TARGET: %s",
        target
    );

    if (answer_count == 0) {

        snprintf(
            lines[line_count++],
            sizeof(lines[0]),
            "NO RECORD FOUND"
        );

    } else {

        for (int i = 0;
             i < answer_count &&
             line_count < 20;
             i++) {

            snprintf(
                lines[line_count++],
                sizeof(lines[0]),
                "%s",
                answers[i]
            );
        }
    }

    /*
     * Scrollable result viewer.
     */
    int selected = 0;
    int scroll = 0;

    const int visible = 18;

    bool dirty = true;

    for (;;) {

        if (dirty) {

            bidhata_platform_clear(
                platform,
                COL_BG
            );

            draw_header(
                platform,
                "DNS LOOKUP"
            );

            int end_line =
                scroll + visible;

            if (end_line > line_count)
                end_line = line_count;

            for (
                int i = scroll;
                i < end_line;
                i++
            ) {

                u32 color =
                    (i == selected)
                    ? COL_ACCENT
                    : COL_DIM;

                char display[96];

                snprintf(
                    display,
                    sizeof(display),
                    "%.92s",
                    lines[i]
                );

                bidhata_font_draw(
                    platform,
                    14,
                    135 + (i - scroll) * 30,
                    display,
                    color,
                    1
                );
            }

            bidhata_font_draw(
                platform,
                14,
                platform->fb_height - 28,
                "VOL +/- MOVE   POWER BACK",
                COL_DIM,
                1
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                platform,
                &tapped,
                &tap_x,
                &tap_y
            );

        (void)tapped;
        (void)tap_x;
        (void)tap_y;

        switch (key) {

        case BIDHATA_KEY_UP:

            if (selected > 0) {

                selected--;

                if (selected < scroll)
                    scroll = selected;

                dirty = true;
            }

            break;

        case BIDHATA_KEY_DOWN:

            if (selected < line_count - 1) {

                selected++;

                if (selected >=
                    scroll + visible) {

                    scroll =
                        selected - visible + 1;
                }

                dirty = true;
            }

            break;

        case BIDHATA_KEY_BACK:
            return;

        default:
            break;
        }

        usleep(50000);
    }
}



static void run_web_check_screen(
    bidhata_platform_t *platform)
{
    char target[128];
    char output[4096];
    executor_result_t result;

    memset(target, 0, sizeof(target));
    memset(output, 0, sizeof(output));
    memset(&result, 0, sizeof(result));

    /*
     * Enter IP/host.
     * Numeric keyboard works for IP addresses now.
     */
    if (geos_keyboard_target(
            platform,
            target,
            sizeof(target),
            true) < 0) {
        return;
    }

    if (!target[0])
        return;

    tool_web_check(
        target,
        output,
        sizeof(output),
        &result
    );

    /*
     * Keep the useful wget response lines.
     */
    char lines[32][128];
    int count = 0;

    char *save = NULL;
    char *line = strtok_r(
        output,
        "\n",
        &save
    );

    while (line && count < 32) {

        while (*line == '\r' ||
               *line == '\t' ||
               *line == ' ') {
            line++;
        }

        if (!line[0]) {
            line = strtok_r(
                NULL,
                "\n",
                &save
            );
            continue;
        }

        if (strstr(line, "HTTP/") ||
            strstr(line, "Server:") ||
            strstr(line, "Location:") ||
            strstr(line, "Content-Type:") ||
            strstr(line, "Content-Length:") ||
            strstr(line, "Connecting to") ||
            strstr(line, "remote file") ||
            strstr(line, "ERROR") ||
            strstr(line, "failed") ||
            strstr(line, "Connection") ||
            strstr(line, "certificate")) {

            snprintf(
                lines[count],
                sizeof(lines[count]),
                "%.124s",
                line
            );

            count++;
        }

        line = strtok_r(
            NULL,
            "\n",
            &save
        );
    }

    /*
     * If filtering found nothing, show the raw response.
     */
    if (count == 0) {

        save = NULL;

        line = strtok_r(
            output,
            "\n",
            &save
        );

        while (line && count < 32) {

            while (*line == '\r' ||
                   *line == '\t' ||
                   *line == ' ') {
                line++;
            }

            if (line[0]) {

                snprintf(
                    lines[count],
                    sizeof(lines[count]),
                    "%.124s",
                    line
                );

                count++;
            }

            line = strtok_r(
                NULL,
                "\n",
                &save
            );
        }
    }

    int selected = 0;
    int scroll = 0;

    const int visible = 18;

    bool dirty = true;

    for (;;) {

        if (dirty) {

            bidhata_platform_clear(
                platform,
                COL_BG
            );

            draw_header(
                platform,
                "WEB CHECK"
            );

            char target_line[160];

            snprintf(
                target_line,
                sizeof(target_line),
                "TARGET: %.130s",
                target
            );

            bidhata_font_draw(
                platform,
                14,
                90,
                target_line,
                COL_DIM,
                1
            );

            int end_line =
                scroll + visible;

            if (end_line > count)
                end_line = count;

            for (
                int i = scroll;
                i < end_line;
                i++
            ) {

                u32 color =
                    (i == selected)
                    ? COL_ACCENT
                    : COL_DIM;

                char display[96];

                snprintf(
                    display,
                    sizeof(display),
                    "%.92s",
                    lines[i]
                );

                bidhata_font_draw(
                    platform,
                    14,
                    120 + (i - scroll) * 30,
                    display,
                    color,
                    1
                );
            }

            bidhata_font_draw(
                platform,
                14,
                platform->fb_height - 28,
                "VOL +/- MOVE   POWER BACK",
                COL_DIM,
                1
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                platform,
                &tapped,
                &tap_x,
                &tap_y
            );

        (void)tapped;
        (void)tap_x;
        (void)tap_y;

        switch (key) {

        case BIDHATA_KEY_UP:

            if (selected > 0) {

                selected--;

                if (selected < scroll)
                    scroll = selected;

                dirty = true;
            }

            break;

        case BIDHATA_KEY_DOWN:

            if (selected < count - 1) {

                selected++;

                if (selected >=
                    scroll + visible) {

                    scroll =
                        selected - visible + 1;
                }

                dirty = true;
            }

            break;

        case BIDHATA_KEY_BACK:
            return;

        default:
            break;
        }

        usleep(50000);
    }
}






static int nmap_parse_port_line(
    const char *input,
    char *port,
    size_t port_cap,
    char *service,
    size_t service_cap,
    char *version,
    size_t version_cap)
{
    char line[256];

    if (!input)
        return 0;

    snprintf(
        line,
        sizeof(line),
        "%s",
        input
    );

    char *save = NULL;

    char *porttok =
        strtok_r(
            line,
            " \t\r\n",
            &save
        );

    char *statetok =
        strtok_r(
            NULL,
            " \t\r\n",
            &save
        );

    char *servicetok =
        strtok_r(
            NULL,
            " \t\r\n",
            &save
        );

    if (!porttok ||
        !statetok ||
        !servicetok) {
        return 0;
    }

    if (strcmp(statetok, "open") != 0)
        return 0;

    if (!strchr(porttok, '/'))
        return 0;

    snprintf(
        port,
        port_cap,
        "%s",
        porttok
    );

    snprintf(
        service,
        service_cap,
        "%s",
        servicetok
    );

    char *rest = save;

    while (
        rest &&
        (*rest == ' ' ||
         *rest == '\t' ||
         *rest == '\r' ||
         *rest == '\n')
    ) {
        rest++;
    }

    if (rest && *rest) {

        snprintf(
            version,
            version_cap,
            "%.90s",
            rest
        );

    } else {

        version[0] = '\0';
    }

    return 1;
}


static void run_nmap_results(
    bidhata_platform_t *platform,
    const char *target,
    int mode)
{
    char output[8192];
    executor_result_t result;

    memset(output, 0, sizeof(output));
    memset(&result, 0, sizeof(result));

    if (tool_nmap_scan_mode(
            target,
            mode,
            output,
            sizeof(output),
            &result) < 0) {

        show_page(
            platform,
            "NMAP",
            "SCAN ERROR",
            "Check Wi-Fi connection",
            "POWER BACK"
        );

        wait_back_or_select(platform);
        return;
    }

    char lines[64][128];
    int count = 0;

    memset(lines, 0, sizeof(lines));

    /*
     * DISCOVER mode: show discovered hosts.
     */
    if (mode == 0) {

        char *save = NULL;

        char *line =
            strtok_r(
                output,
                "\n",
                &save
            );

        while (line && count < 60) {

            while (
                *line == ' ' ||
                *line == '\t' ||
                *line == '\r'
            )
                line++;

            const char *prefix =
                "Nmap scan report for ";

            if (strncmp(
                    line,
                    prefix,
                    strlen(prefix)) == 0) {

                snprintf(
                    lines[count++],
                    sizeof(lines[0]),
                    "HOST: %.112s",
                    line + strlen(prefix)
                );
            }

            else if (
                strncmp(
                    line,
                    "Nmap done:",
                    10
                ) == 0) {

                snprintf(
                    lines[count++],
                    sizeof(lines[0]),
                    "%.124s",
                    line
                );
            }

            line =
                strtok_r(
                    NULL,
                    "\n",
                    &save
                );
        }

        if (count == 0) {

            snprintf(
                lines[count++],
                sizeof(lines[0]),
                "NO HOSTS FOUND"
            );
        }
    }

    /*
     * PORT / SERVICE modes.
     */
    else {

        char *save = NULL;

        char *line =
            strtok_r(
                output,
                "\n",
                &save
            );

        int open_count = 0;

        while (line && count < 60) {

            while (
                *line == ' ' ||
                *line == '\t' ||
                *line == '\r'
            )
                line++;

            char port[32] = "";
            char service[64] = "";
            char version[96] = "";

            if (nmap_parse_port_line(
                    line,
                    port,
                    sizeof(port),
                    service,
                    sizeof(service),
                    version,
                    sizeof(version))) {

                if (mode == 1) {

                    snprintf(
                        lines[count++],
                        sizeof(lines[0]),
                        "%s  %s",
                        port,
                        service
                    );

                } else {

                    if (version[0]) {

                        snprintf(
                            lines[count++],
                            sizeof(lines[0]),
                            "%s %s %.75s",
                            port,
                            service,
                            version
                        );

                    } else {

                        snprintf(
                            lines[count++],
                            sizeof(lines[0]),
                            "%s  %s",
                            port,
                            service
                        );
                    }
                }

                open_count++;
            }

            line =
                strtok_r(
                    NULL,
                    "\n",
                    &save
                );
        }

        /*
         * If the parser finds nothing, show the relevant
         * Nmap output instead of falsely claiming there
         * are no ports.
         */
        if (open_count == 0) {

            count = 0;

            save = NULL;

            line =
                strtok_r(
                    output,
                    "\n",
                    &save
                );

            while (line && count < 60) {

                while (
                    *line == ' ' ||
                    *line == '\t' ||
                    *line == '\r'
                )
                    line++;

                if (
                    strchr(line, '/') &&
                    (
                        strstr(line, "open") ||
                        strstr(line, "closed") ||
                        strstr(line, "filtered")
                    )
                ) {

                    snprintf(
                        lines[count++],
                        sizeof(lines[0]),
                        "%.124s",
                        line
                    );
                }

                line =
                    strtok_r(
                        NULL,
                        "\n",
                        &save
                    );
            }

            if (count == 0) {

                snprintf(
                    lines[count++],
                    sizeof(lines[0]),
                    "NO OPEN PORTS"
                );

            } else {

                char summary[64];

                snprintf(
                    summary,
                    sizeof(summary),
                    "OPEN: %d",
                    open_count
                );

                /*
                 * Don't append summary if we are showing
                 * fallback raw lines.
                 */
            }
        }
        else {

            snprintf(
                lines[count++],
                sizeof(lines[0]),
                "OPEN PORTS: %d",
                open_count
            );
        }
    }

    int selected = 0;
    int scroll = 0;

    const int visible = 17;

    bool dirty = true;

    for (;;) {

        if (dirty) {

            bidhata_platform_clear(
                platform,
                COL_BG
            );

            const char *title =
                mode == 0
                    ? "NMAP DISCOVER"
                    : mode == 1
                        ? "NMAP PORTS"
                        : "NMAP SERVICES";

            draw_header(
                platform,
                title
            );

            char target_line[160];

            snprintf(
                target_line,
                sizeof(target_line),
                "TARGET: %.130s",
                target
            );

            bidhata_font_draw(
                platform,
                14,
                86,
                target_line,
                COL_DIM,
                1
            );

            int end_line =
                scroll + visible;

            if (end_line > count)
                end_line = count;

            for (
                int i = scroll;
                i < end_line;
                i++
            ) {

                u32 color =
                    (i == selected)
                        ? COL_ACCENT
                        : COL_DIM;

                char display[96];

                snprintf(
                    display,
                    sizeof(display),
                    "%.92s",
                    lines[i]
                );

                bidhata_font_draw(
                    platform,
                    14,
                    118 +
                        (i - scroll) * 30,
                    display,
                    color,
                    1
                );
            }

            bidhata_font_draw(
                platform,
                14,
                platform->fb_height - 28,
                "VOL +/- MOVE   POWER BACK",
                COL_DIM,
                1
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                platform,
                &tapped,
                &tap_x,
                &tap_y
            );

        (void)tapped;
        (void)tap_x;
        (void)tap_y;

        switch (key) {

        case BIDHATA_KEY_UP:

            if (selected > 0) {

                selected--;

                if (selected < scroll)
                    scroll = selected;

                dirty = true;
            }

            break;

        case BIDHATA_KEY_DOWN:

            if (selected < count - 1) {

                selected++;

                if (selected >=
                    scroll + visible) {

                    scroll =
                        selected -
                        visible + 1;
                }

                dirty = true;
            }

            break;

        case BIDHATA_KEY_BACK:
            return;

        default:
            break;
        }

        usleep(50000);
    }
}



static void draw_assess_progress(
    bidhata_platform_t *platform,
    const char *target,
    const char *ping_status,
    const char *port_status,
    const char *service_status)
{
    bidhata_platform_clear(platform, COL_BG);

    draw_header(platform, "ASSESS");

    char line[192];

    snprintf(line, sizeof(line), "TARGET  %.80s", target);
    bidhata_font_draw(
        platform, 24, 86, line, COL_ACCENT, 2
    );

    snprintf(line, sizeof(line), "PING      %s", ping_status);
    bidhata_font_draw(
        platform, 24, 150, line, COL_DIM, 2
    );

    snprintf(line, sizeof(line), "PORTS     %s", port_status);
    bidhata_font_draw(
        platform, 24, 210, line, COL_DIM, 2
    );

    snprintf(line, sizeof(line), "SERVICES  %s", service_status);
    bidhata_font_draw(
        platform, 24, 270, line, COL_DIM, 2
    );

    bidhata_font_draw(
        platform,
        24,
        platform->fb_height - 28,
        "ASSESSING...",
        COL_DIM,
        1
    );
}


static int assess_ping(
    const char *target,
    char *latency,
    size_t latency_cap)
{
    char output[2048];
    executor_result_t result;

    if (latency && latency_cap)
        latency[0] = '\0';

    if (!target)
        return -1;

    /*
     * HiBy's firmware uses BusyBox extensively.
     * Try the BusyBox applet first, then normal ping paths.
     */
    char *busybox_argv[] = {
        "busybox",
        "ping",
        "-c",
        "1",
        "-W",
        "2",
        (char *)target,
        NULL
    };

    const char *busybox_paths[] = {
        "/bin/busybox",
        "/sbin/busybox",
        "/usr/bin/busybox"
    };

    int rc = -1;

    for (size_t i = 0;
         i < sizeof(busybox_paths) /
             sizeof(busybox_paths[0]);
         i++) {

        memset(output, 0, sizeof(output));
        memset(&result, 0, sizeof(result));

        rc = executor_run(
            busybox_paths[i],
            busybox_argv,
            output,
            sizeof(output),
            5000,
            &result
        );

        if (rc >= 0)
            break;
    }

    /*
     * Fall back to a standalone ping binary.
     */
    if (rc < 0) {

        char *ping_argv[] = {
            "ping",
            "-c",
            "1",
            "-W",
            "2",
            (char *)target,
            NULL
        };

        const char *ping_paths[] = {
            "/bin/ping",
            "/sbin/ping",
            "/usr/bin/ping"
        };

        for (size_t i = 0;
             i < sizeof(ping_paths) /
                 sizeof(ping_paths[0]);
             i++) {

            memset(output, 0, sizeof(output));
            memset(&result, 0, sizeof(result));

            rc = executor_run(
                ping_paths[i],
                ping_argv,
                output,
                sizeof(output),
                5000,
                &result
            );

            if (rc >= 0)
                break;
        }
    }

    if (rc < 0)
        return -1;

    if (result.exit_code != 0 ||
        result.term_signal != 0 ||
        result.timed_out) {

        return 0;
    }

    if (latency && latency_cap) {

        const char *p =
            strstr(output, "time=");

        if (p) {

            p += 5;

            size_t n = 0;

            while (p[n] &&
                   p[n] != ' ' &&
                   p[n] != '\r' &&
                   p[n] != '\n' &&
                   n + 1 < latency_cap) {

                latency[n] = p[n];
                n++;
            }

            latency[n] = '\0';
        }
    }

    return 1;
}



static int count_nmap_open_ports(
    const char *output)
{
    int count = 0;

    if (!output)
        return 0;

    char work[8192];
    snprintf(work, sizeof(work), "%s", output);

    char *save = NULL;
    char *line = strtok_r(
        work,
        "\n",
        &save
    );

    while (line) {

        char port[32] = "";
        char service[64] = "";
        char version[96] = "";

        while (*line == ' ' ||
               *line == '\t' ||
               *line == '\r') {
            line++;
        }

        if (nmap_parse_port_line(
                line,
                port,
                sizeof(port),
                service,
                sizeof(service),
                version,
                sizeof(version))) {

            count++;
        }

        line = strtok_r(
            NULL,
            "\n",
            &save
        );
    }

    return count;
}



static void geos_format_host_label(
    const char *name,
    char *out,
    size_t out_cap)
{
    if (!out || out_cap == 0)
        return;

    out[0] = '\0';

    if (!name || !name[0])
        return;

    snprintf(out, out_cap, "%s", name);

    /* Use the local hostname instead of its full DNS suffix. */
    char *dot = strchr(out, '.');
    if (dot)
        *dot = '\0';

    size_t len = strlen(out);
    size_t limit = out_cap - 1;

    if (limit > 27)
        limit = 27;

    if (len > limit && limit >= 3) {
        out[limit - 3] = '.';
        out[limit - 2] = '.';
        out[limit - 1] = '.';
        out[limit] = '\0';
    }
}


static int geos_nmap_save_report(
    const char *target,
    const char *host_name,
    const char *ping_status,
    const char *port_status,
    const char *service_status,
    int port_rc,
    int service_rc,
    const char *port_output,
    const char *service_output,
    char *saved_path,
    size_t saved_path_cap)
{
    if (!target || !saved_path || saved_path_cap == 0)
        return -1;

    saved_path[0] = '\0';

    time_t now = time(NULL);
    char timestamp[32] = "time-unavailable";
    struct tm *tm_now = localtime(&now);

    if (now != (time_t)-1 && tm_now) {
        if (strftime(timestamp, sizeof(timestamp),
                     "%Y%m%d_%H%M%S", tm_now) == 0)
            snprintf(timestamp, sizeof(timestamp), "time-unavailable");
    }

    char safe_target[48];
    size_t j = 0;

    for (size_t i = 0; target[i] && j < sizeof(safe_target) - 1; i++) {
        unsigned char c = (unsigned char)target[i];

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_') {
            safe_target[j++] = (char)c;
        } else {
            safe_target[j++] = '_';
        }
    }
    safe_target[j] = '\0';

    char path[192];
    snprintf(path, sizeof(path),
             "/data/mnt/sd_0/geos_nmap_report_%s_%s.txt",
             timestamp, safe_target[0] ? safe_target : "target");

    FILE *fp = fopen(path, "w");
    if (!fp)
        return -1;

    fprintf(fp, "GEOS NMAP ASSESSMENT REPORT\n");
    fprintf(fp, "========================================\n");
    fprintf(fp, "Timestamp: %s\n", timestamp);
    fprintf(fp, "Target IP: %s\n", target);
    fprintf(fp, "Hostname: %s\n",
            host_name && host_name[0] ? host_name : "Not supplied");
    fprintf(fp, "Report type: Ping, port scan, and service detection\n\n");

    fprintf(fp, "[SUMMARY]\n");
    fprintf(fp, "Ping: %s\n", ping_status ? ping_status : "Unknown");
    fprintf(fp, "Port scan: %s\n", port_status ? port_status : "Unknown");
    fprintf(fp, "Service detection: %s\n",
            service_status ? service_status : "Unknown");
    fprintf(fp, "Port scan status: %s\n", port_rc >= 0 ? "Completed" : "Error");
    fprintf(fp, "Service detection status: %s\n",
            service_rc >= 0 ? "Completed" : "Error");

    geos_wifi_status_t wifi;
    memset(&wifi, 0, sizeof(wifi));

    fprintf(fp, "\n[LOCAL NETWORK CONTEXT]\n");
    if (connectivity_wifi_status(&wifi) == 0) {
        fprintf(fp, "Connection: %s\n",
                wifi.connected ? "Connected" : "Disconnected");
        fprintf(fp, "SSID: %s\n", wifi.ssid[0] ? wifi.ssid : "Unavailable");
        fprintf(fp, "R1 IP: %s\n", wifi.ip[0] ? wifi.ip : "Unavailable");
        fprintf(fp, "Gateway: %s\n",
                wifi.gateway[0] ? wifi.gateway : "Unavailable");
        fprintf(fp, "DNS: %s\n", wifi.dns[0] ? wifi.dns : "Unavailable");
    } else {
        fprintf(fp, "Network status could not be read.\n");
    }

    fprintf(fp, "\n[PORT SCAN RAW OUTPUT]\n");
    fprintf(fp, "%s\n",
            port_output && port_output[0] ? port_output : "No output captured.");

    fprintf(fp, "\n[SERVICE DETECTION RAW OUTPUT]\n");
    fprintf(fp, "%s\n",
            service_output && service_output[0]
                ? service_output
                : "No output captured.");

    fprintf(fp, "\n[NOTES]\n");
    fprintf(fp, "Results reflect this scan at the recorded time.\n");
    fprintf(fp, "An error or empty result is not proof that a host has no services.\n");

    int write_error = ferror(fp);
    if (fclose(fp) != 0)
        write_error = 1;

    if (write_error)
        return -1;

    snprintf(saved_path, saved_path_cap, "%s", path);
    return 0;
}

static int run_nmap_assessment_once(
    bidhata_platform_t *platform,
    const char *target,
    const char *host_name,
    bool from_host_list)
{
    char ping_latency[32] = "";
    char ping_status[64] = "";

    char port_output[8192] = "";
    char service_output[8192] = "";

    executor_result_t result;

    memset(&result, 0, sizeof(result));

    /*
     * --------------------------------------------------------
     * PING
     * --------------------------------------------------------
     */

    draw_assess_progress(
        platform,
        target,
        "RUNNING",
        "WAITING",
        "WAITING"
    );

    int ping_rc = assess_ping(
        target,
        ping_latency,
        sizeof(ping_latency)
    );

    if (ping_rc < 0) {

        snprintf(
            ping_status,
            sizeof(ping_status),
            "ERROR"
        );

    } else if (ping_rc == 0) {

        snprintf(
            ping_status,
            sizeof(ping_status),
            "NO REPLY"
        );

    } else if (ping_latency[0]) {

        snprintf(
            ping_status,
            sizeof(ping_status),
            "OK %s ms",
            ping_latency
        );

    } else {

        snprintf(
            ping_status,
            sizeof(ping_status),
            "OK"
        );
    }

    /*
     * --------------------------------------------------------
     * PORT SCAN
     * --------------------------------------------------------
     */

    draw_assess_progress(
        platform,
        target,
        ping_status,
        "RUNNING",
        "WAITING"
    );

    int port_rc = tool_nmap_scan_mode(
        target,
        1,
        port_output,
        sizeof(port_output),
        &result
    );

    int open_ports = 0;

    if (port_rc >= 0) {

        open_ports = count_nmap_open_ports(
            port_output
        );
    }

    char port_status[64];

    if (port_rc < 0) {

        snprintf(
            port_status,
            sizeof(port_status),
            "ERROR"
        );

    } else {

        snprintf(
            port_status,
            sizeof(port_status),
            "%d OPEN",
            open_ports
        );
    }

    /*
     * --------------------------------------------------------
     * SERVICE INFO
     * --------------------------------------------------------
     */

    draw_assess_progress(
        platform,
        target,
        ping_status,
        port_status,
        "RUNNING"
    );

    int service_rc = tool_nmap_scan_mode(
        target,
        2,
        service_output,
        sizeof(service_output),
        &result
    );

    int services = 0;

    if (service_rc >= 0) {

        services = count_nmap_open_ports(
            service_output
        );
    }

    char service_status[64];

    if (service_rc < 0) {

        snprintf(
            service_status,
            sizeof(service_status),
            "ERROR"
        );

    } else {

        snprintf(
            service_status,
            sizeof(service_status),
            "%d IDENTIFIED",
            services
        );
    }

    /*
     * --------------------------------------------------------
     * COMBINED RESULT
     * --------------------------------------------------------
     */

    char lines[24][128];
    int count = 0;

    if (host_name && host_name[0]) {
        char display_name[32] = "";

        geos_format_host_label(
            host_name,
            display_name,
            sizeof(display_name)
        );

        snprintf(
            lines[count++],
            sizeof(lines[0]),
            "DEVICE  %.28s",
            display_name
        );

        snprintf(
            lines[count++],
            sizeof(lines[0]),
            "IP      %.90s",
            target
        );
    } else {
        snprintf(
            lines[count++],
            sizeof(lines[0]),
            "TARGET  %.90s",
            target
        );
    }

    snprintf(
        lines[count++],
        sizeof(lines[0]),
        "PING    %s",
        ping_status
    );

    snprintf(
        lines[count++],
        sizeof(lines[0]),
        "PORTS   %s",
        port_status
    );

    snprintf(
        lines[count++],
        sizeof(lines[0]),
        "SVCS    %s",
        service_status
    );

    snprintf(
        lines[count++],
        sizeof(lines[0]),
        ""
    );

    /*
     * Show service results.
     */
    if (service_rc >= 0) {

        char work[8192];

        snprintf(
            work,
            sizeof(work),
            "%s",
            service_output
        );

        char *save = NULL;

        char *line = strtok_r(
            work,
            "\n",
            &save
        );

        int shown = 0;

        while (line &&
               shown < 10 &&
               count < 16) {

            char port[32] = "";
            char service[64] = "";
            char version[96] = "";

            while (*line == ' ' ||
                   *line == '\t' ||
                   *line == '\r') {
                line++;
            }

            if (nmap_parse_port_line(
                    line,
                    port,
                    sizeof(port),
                    service,
                    sizeof(service),
                    version,
                    sizeof(version))) {

                if (version[0]) {

                    snprintf(
                        lines[count++],
                        sizeof(lines[0]),
                        "%.28s  %.75s",
                        port,
                        version
                    );

                } else {

                    snprintf(
                        lines[count++],
                        sizeof(lines[0]),
                        "%.28s  %.75s",
                        port,
                        service
                    );
                }

                shown++;
            }

            line = strtok_r(
                NULL,
                "\n",
                &save
            );
        }
    }

    if (count == (host_name && host_name[0] ? 6 : 5)) {

        snprintf(
            lines[count++],
            sizeof(lines[0]),
            "NO OPEN SERVICES"
        );
    }

    /*
     * Interactive actions at the bottom of the report.
     */
    const int save_index = count;
    snprintf(lines[count++], sizeof(lines[0]), "SAVE REPORT");

    const int rescan_index = count;
    snprintf(lines[count++], sizeof(lines[0]), "RESCAN");

    const int back_index = count;
    snprintf(
        lines[count++],
        sizeof(lines[0]),
        "%s",
        from_host_list ? "BACK TO HOSTS" : "BACK"
    );

    /*
     * --------------------------------------------------------
     * RESULT VIEW
     * --------------------------------------------------------
     */

    int selected = 0;
    int scroll = 0;

    const int visible = 15;

    bool dirty = true;

    for (;;) {

        if (dirty) {

            bidhata_platform_clear(
                platform,
                COL_BG
            );

            draw_header(
                platform,
                "ASSESS RESULT"
            );

            for (int i = 0;
                 i < visible &&
                 scroll + i < count;
                 i++) {

                bidhata_font_draw(
                    platform,
                    24,
                    118 + i * 42,
                    lines[scroll + i],
                    (scroll + i == selected)
                        ? COL_ACCENT
                        : COL_DIM,
                    1
                );
            }

            bidhata_font_draw(
                platform,
                24,
                platform->fb_height - 28,
                "VOL +/- MOVE   NEXT/BACK",
                COL_DIM,
                1
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                platform,
                &tapped,
                &tap_x,
                &tap_y
            );

        if (tapped) {

            int row =
                (tap_y - 104) / 42;

            if (tap_y >= 104 &&
                row >= 0 &&
                row < visible) {

                int idx =
                    scroll + row;

                if (idx >= 0 &&
                    idx < count) {

                    selected = idx;
                    key = BIDHATA_KEY_SELECT;
                }
            }
        }

        switch (key) {

        case BIDHATA_KEY_UP:

            if (selected > 0) {

                selected--;

                if (selected < scroll)
                    scroll = selected;

                dirty = true;
            }

            break;

        case BIDHATA_KEY_DOWN:

            if (selected < count - 1) {

                selected++;

                if (selected >= scroll + visible)
                    scroll =
                        selected - visible + 1;

                dirty = true;
            }

            break;

        case BIDHATA_KEY_BACK:
            return 0;

        case BIDHATA_KEY_SELECT:
            if (selected == save_index) {
                char report_path[192] = "";

                if (geos_nmap_save_report(
                        target,
                        host_name,
                        ping_status,
                        port_status,
                        service_status,
                        port_rc,
                        service_rc,
                        port_output,
                        service_output,
                        report_path,
                        sizeof(report_path)) == 0) {

                    const char *filename = strrchr(report_path, '/');
                    filename = filename ? filename + 1 : report_path;

                    show_page(
                        platform,
                        "REPORT SAVED",
                        "NMAP report saved",
                        filename,
                        "POWER BACK"
                    );
                } else {
                    show_page(
                        platform,
                        "REPORT ERROR",
                        "Could not save Nmap report",
                        "Check SD card mount",
                        "POWER BACK"
                    );
                }

                wait_back_or_select(platform);
                dirty = true;
                break;
            }

            if (selected == rescan_index)
                return 1;

            if (selected == back_index)
                return 0;

            dirty = true;
            break;

        default:
            break;
        }

        usleep(50000);
    }
}



/*
 * Parse an IPv4 literal from an Nmap host-report line.
 * Supports both:
 *   Nmap scan report for 192.168.1.210
 *   Nmap scan report for hostname (192.168.1.210)
 */

/*
 * Repeat the complete assessment when RESCAN is selected.
 * Return to the caller when BACK is selected.
 */
static void run_nmap_assessment(
    bidhata_platform_t *platform,
    const char *target,
    const char *host_name,
    bool from_host_list)
{
    for (;;) {
        int action = run_nmap_assessment_once(
            platform,
            target,
            host_name,
            from_host_list
        );

        if (action <= 0)
            return;
    }
}


static int geos_valid_ipv4(const char *s)
{
    int dots = 0;
    int digits = 0;
    int value = 0;

    if (!s || !*s)
        return 0;

    for (const unsigned char *p = (const unsigned char *)s; ; p++) {
        if (*p >= '0' && *p <= '9') {
            digits++;
            value = value * 10 + (*p - '0');

            if (digits > 3 || value > 255)
                return 0;
        } else if (*p == '.') {
            if (digits == 0 || dots >= 3)
                return 0;

            dots++;
            digits = 0;
            value = 0;
        } else if (*p == '\0') {
            return dots == 3 && digits > 0 && digits <= 3;
        } else {
            return 0;
        }
    }
}

static void geos_nmap_report_name(
    const char *line,
    char *name,
    size_t name_cap)
{
    static const char prefix[] = "Nmap scan report for ";

    if (!name || name_cap == 0)
        return;

    name[0] = '\0';

    if (!line ||
        strncmp(line, prefix, sizeof(prefix) - 1) != 0)
        return;

    const char *candidate = line + sizeof(prefix) - 1;
    const char *open = strchr(candidate, '(');

    /*
     * Nmap format with a resolved name:
     * Nmap scan report for hostname (192.168.1.10)
     *
     * If there is no parenthesized address, there is no
     * separate hostname to store.
     */
    if (!open)
        return;

    const char *end = open;

    while (end > candidate &&
           (end[-1] == ' ' ||
            end[-1] == '\t' ||
            end[-1] == '\r')) {
        end--;
    }

    size_t len = (size_t)(end - candidate);

    if (len == 0 || len >= name_cap)
        return;

    memcpy(name, candidate, len);
    name[len] = '\0';

    if (geos_valid_ipv4(name))
        name[0] = '\0';
}

static int geos_nmap_report_ipv4(
    const char *line,
    char *ip,
    size_t ip_cap)
{
    static const char prefix[] = "Nmap scan report for ";

    if (!line || !ip || ip_cap < 8)
        return 0;

    if (strncmp(line, prefix, sizeof(prefix) - 1) != 0)
        return 0;

    const char *candidate = line + sizeof(prefix) - 1;
    const char *open = strchr(candidate, '(');
    const char *end = NULL;

    if (open) {
        candidate = open + 1;
        end = strchr(candidate, ')');

        if (!end)
            return 0;
    } else {
        end = candidate;

        while (*end &&
               *end != ' ' &&
               *end != '\t' &&
               *end != '\r' &&
               *end != '\n') {
            end++;
        }
    }

    size_t len = (size_t)(end - candidate);

    if (len == 0 || len >= ip_cap)
        return 0;

    memcpy(ip, candidate, len);
    ip[len] = '\0';

    return geos_valid_ipv4(ip);
}

/*
 * DISCOVER network -> display selectable IPv4 hosts -> assess
 * whichever host the user selects.
 */
static void run_nmap_discover_hosts(
    bidhata_platform_t *platform,
    const char *target)
{
    char output[8192] = "";
    struct geos_discovered_host {
        char ip[32];
        char name[96];
    } hosts[60];
    executor_result_t result;

    memset(hosts, 0, sizeof(hosts));
    memset(&result, 0, sizeof(result));

    bidhata_platform_clear(platform, COL_BG);
    draw_header(platform, "HOST DISCOVERY");

    bidhata_font_draw(
        platform, 24, TOP + 10,
        "Scanning network...",
        COL_ACCENT, 2
    );

    bidhata_font_draw(
        platform, 24, TOP + 52,
        target,
        COL_DIM, 1
    );

    if (tool_nmap_scan_mode(
            target,
            0,
            output,
            sizeof(output),
            &result) < 0) {

        show_page(
            platform,
            "HOST DISCOVERY",
            "SCAN ERROR",
            "Check Wi-Fi connection",
            "POWER BACK"
        );

        wait_back_or_select(platform);
        return;
    }

    int count = 0;
    char *save = NULL;
    char *line = strtok_r(output, "\n", &save);

    while (line && count < 60) {
        while (*line == ' ' ||
               *line == '\t' ||
               *line == '\r') {
            line++;
        }

        char ip[32];

        if (geos_nmap_report_ipv4(
                line,
                ip,
                sizeof(ip))) {

            int duplicate = 0;

            for (int i = 0; i < count; i++) {
                if (strcmp(hosts[i].ip, ip) == 0) {
                    duplicate = 1;
                    break;
                }
            }

            if (!duplicate) {
                snprintf(
                    hosts[count].ip,
                    sizeof(hosts[count].ip),
                    "%s",
                    ip
                );

                geos_nmap_report_name(
                    line,
                    hosts[count].name,
                    sizeof(hosts[count].name)
                );

                count++;
            }
        }

        line = strtok_r(NULL, "\n", &save);
    }

    if (count == 0) {
        show_page(
            platform,
            "HOST DISCOVERY",
            "NO HOSTS FOUND",
            "Try your subnet, e.g. /24",
            "POWER BACK"
        );

        wait_back_or_select(platform);
        return;
    }

    int selected = 0;
    int first_visible = 0;
    int visible_rows =
        (platform->fb_height - TOP - 50) / ROW_H;

    if (visible_rows < 1)
        visible_rows = 1;

    if (visible_rows > 60)
        visible_rows = 60;

    bool dirty = true;

    for (;;) {
        if (dirty) {
            bidhata_platform_clear(platform, COL_BG);
            draw_header(platform, "SELECT HOST");

            for (int row = 0; row < visible_rows; row++) {
                int index = first_visible + row;

                if (index >= count)
                    break;

                char label[32] = "";

                if (hosts[index].name[0]) {
                    geos_format_host_label(
                        hosts[index].name,
                        label,
                        sizeof(label)
                    );
                } else {
                    snprintf(
                        label,
                        sizeof(label),
                        "%s",
                        hosts[index].ip
                    );
                }

                draw_item(
                    platform,
                    TOP + row * ROW_H,
                    index == selected,
                    label
                );
            }

            char footer[64];

            snprintf(
                footer,
                sizeof(footer),
                "%d HOSTS FOUND",
                count
            );

            bidhata_font_draw(
                platform,
                24,
                platform->fb_height - 48,
                footer,
                COL_DIM,
                1
            );

            bidhata_font_draw(
                platform,
                24,
                platform->fb_height - 28,
                "TAP DEVICE TO ASSESS  POWER BACK",
                COL_DIM,
                1
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key = bidhata_platform_poll_menu(
            platform,
            &tapped,
            &tap_x,
            &tap_y
        );

        (void)tap_x;

        if (tapped &&
            tap_y >= TOP &&
            tap_y < TOP + visible_rows * ROW_H) {

            int row = (tap_y - TOP) / ROW_H;
            int index = first_visible + row;

            if (index >= 0 && index < count) {
                selected = index;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {
        case BIDHATA_KEY_UP:
            if (selected > 0) {
                selected--;
                dirty = true;
            }
            break;

        case BIDHATA_KEY_DOWN:
            if (selected < count - 1) {
                selected++;
                dirty = true;
            }
            break;

        case BIDHATA_KEY_BACK:
            return;

        case BIDHATA_KEY_SELECT:
            /*
             * No second keyboard prompt: assess the selected IP.
             */
            run_nmap_assessment(
                platform,
                hosts[selected].ip,
                hosts[selected].name,
                true
            );

            dirty = true;
            break;

        default:
            break;
        }

        if (selected < first_visible) {
            first_visible = selected;
            dirty = true;
        }

        if (selected >= first_visible + visible_rows) {
            first_visible = selected - visible_rows + 1;
            dirty = true;
        }

        usleep(50000);
    }
}


/*
 * NMAP menu
 */
static void run_nmap_screen(
    bidhata_platform_t *platform)
{
    static const char *items[] = {
        "DISCOVER",
        "ASSESS",
        "PORT SCAN",
        "SERVICE INFO",
        "BACK"
    };

    int selected = 0;
    bool dirty = true;

    for (;;) {
        if (dirty) {
            bidhata_platform_clear(platform, COL_BG);
            draw_header(platform, "NMAP");

            for (int i = 0; i < 5; i++) {
                draw_item(
                    platform,
                    TOP + i * ROW_H,
                    i == selected,
                    items[i]
                );
            }

            bidhata_font_draw(
                platform,
                24,
                platform->fb_height - 28,
                "VOL +/- MOVE   NEXT SELECT",
                COL_DIM,
                1
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key = bidhata_platform_poll_menu(
            platform,
            &tapped,
            &tap_x,
            &tap_y
        );

        (void)tap_x;

        if (tapped) {
            int row = (tap_y - TOP) / ROW_H;

            if (tap_y >= TOP && row >= 0 && row < 5) {
                selected = row;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {
        case BIDHATA_KEY_UP:
            if (selected > 0) {
                selected--;
                dirty = true;
            }
            break;

        case BIDHATA_KEY_DOWN:
            if (selected < 4) {
                selected++;
                dirty = true;
            }
            break;

        case BIDHATA_KEY_BACK:
            return;

        case BIDHATA_KEY_SELECT:
            if (selected == 4)
                return;

            {
                char target[128] = "";

                if (geos_keyboard_target(
                        platform,
                        target,
                        sizeof(target),
                        true) < 0 ||
                    !target[0]) {

                    dirty = true;
                    break;
                }

                if (selected == 0) {
                    /*
                     * Enter a subnet/range once, then select
                     * an individual discovered IP.
                     */
                    run_nmap_discover_hosts(
                        platform,
                        target
                    );
                } else if (selected == 1) {
                    run_nmap_assessment(platform, target, NULL, false);
                } else {
                    run_nmap_results(
                        platform,
                        target,
                        selected - 1
                    );
                }

                dirty = true;
            }
            break;

        default:
            break;
        }

        usleep(50000);
    }
}

static void run_recon_screen(
    bidhata_platform_t *platform)
{
    static const char *items[] = {
        "PING",
        "DNS LOOKUP",
        "WEB CHECK",
        "NMAP SCAN",
        "BACK"
    };

    int selected = 0;
    bool dirty = true;

    for (;;) {

        if (dirty) {

            bidhata_platform_clear(
                platform,
                COL_BG
            );

            draw_header(
                platform,
                "RECON"
            );

            for (int i = 0; i < 5; i++) {
                draw_item(
                    platform,
                    TOP + i * ROW_H,
                    i == selected,
                    items[i]
                );
            }

            bidhata_font_draw(
                platform,
                24,
                platform->fb_height - 28,
                "VOL +/- MOVE   NEXT SELECT",
                COL_DIM,
                1
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                platform,
                &tapped,
                &tap_x,
                &tap_y
            );

        if (tapped) {

            int row =
                (tap_y - TOP) / ROW_H;

            if (tap_y >= TOP &&
                row >= 0 &&
                row < 5) {

                selected = row;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {

        case BIDHATA_KEY_UP:
            if (selected > 0) {
                selected--;
                dirty = true;
            }
            break;

        case BIDHATA_KEY_DOWN:
            if (selected < 4) {
                selected++;
                dirty = true;
            }
            break;

        case BIDHATA_KEY_BACK:
            return;

        case BIDHATA_KEY_SELECT:

            switch (selected) {

            case 0:
                run_ping_screen(platform);
                dirty = true;
                break;

            case 1:
                run_dns_screen(platform);
                dirty = true;
                break;

            case 2:
                run_web_check_screen(platform);
                dirty = true;
                break;

            case 3:
                run_nmap_screen(platform);
                dirty = true;
                break;

            case 4:
                return;

            default:
                break;
            }

            break;

        default:
            break;
        }

        usleep(50000);
    }
}



#define GEOS_MAX_SAVED_REPORTS 64

typedef struct {
    char filename[192];
    char path[256];
    char type[8];
    char label[80];
    time_t modified;
} geos_saved_report_t;

static int geos_report_read_value(
    const char *path,
    const char *key,
    char *out,
    size_t out_cap)
{
    if (!path || !key || !out || out_cap == 0)
        return 0;

    out[0] = '\0';

    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0;

    char line[512];
    size_t key_len = strlen(key);
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, key_len) != 0)
            continue;

        char *value = line + key_len;
        value[strcspn(value, "\r\n")] = '\0';

        snprintf(out, out_cap, "%s", value);
        found = out[0] != '\0';
        break;
    }

    fclose(fp);
    return found;
}

static int geos_report_is_txt(const char *name)
{
    size_t len;

    if (!name)
        return 0;

    len = strlen(name);

    return len >= 4 &&
           strcmp(name + len - 4, ".txt") == 0;
}

static int geos_report_is_pcap(const char *name)
{
    size_t len;

    if (!name)
        return 0;
    len = strlen(name);
    return len >= 5 && strcmp(name + len - 5, ".pcap") == 0;
}

static void geos_report_make_label(geos_saved_report_t *r)
{
    char value[128] = "";

    if (strcmp(r->type, "PCAP") == 0) {
        snprintf(value, sizeof(value), "Wi-Fi packet capture");
    } else if (strcmp(r->type, "NMAP") == 0) {
        if (!geos_report_read_value(
                r->path, "Hostname: ", value, sizeof(value)) ||
            !value[0] ||
            strcmp(value, "Not supplied") == 0) {
            geos_report_read_value(
                r->path, "Target IP: ", value, sizeof(value));
        }
    } else if (strcmp(r->type, "APMON") == 0) {
        geos_report_read_value(
            r->path, "Target SSID: ", value, sizeof(value));
        if (!value[0] || strcmp(value, "Unavailable") == 0)
            snprintf(value, sizeof(value), "AP Monitor Log");
    } else {
        geos_report_read_value(
            r->path, "SSID: ", value, sizeof(value));

        if (!value[0] || strcmp(value, "Unavailable") == 0)
            snprintf(value, sizeof(value), "Unknown network");
    }

    if (!value[0])
        snprintf(value, sizeof(value), "Unnamed report");

    snprintf(r->label, sizeof(r->label), "%s", value);
}

static int geos_report_compare(
    const void *left,
    const void *right)
{
    const geos_saved_report_t *a = left;
    const geos_saved_report_t *b = right;

    if (a->modified < b->modified)
        return 1;

    if (a->modified > b->modified)
        return -1;

    return strcmp(b->filename, a->filename);
}


static int geos_report_confirm_delete(
    bidhata_platform_t *platform,
    const geos_saved_report_t *r)
{
    int selected = 1; /* Default to CANCEL. */

    for (;;) {
        bidhata_platform_clear(platform, COL_BG);
        draw_header(platform, "DELETE REPORT");

        bidhata_font_draw(
            platform, 18, 120,
            r->label,
            COL_ACCENT, 1
        );

        bidhata_font_draw(
            platform, 18, 165,
            "Delete this report permanently?",
            COL_DIM, 1
        );

        int yes_y = platform->fb_height - 76;
        int cancel_y = platform->fb_height - 40;

        bidhata_font_draw(
            platform, 24, yes_y,
            selected == 0 ? "> YES, DELETE" : "  YES, DELETE",
            selected == 0 ? COL_ACCENT : COL_DIM,
            2
        );

        bidhata_font_draw(
            platform, 24, cancel_y,
            selected == 1 ? "> CANCEL" : "  CANCEL",
            selected == 1 ? COL_ACCENT : COL_DIM,
            2
        );

        bool tapped = false;
        int tx = 0, ty = 0;

        bidhata_key_t key = bidhata_platform_poll_menu(
            platform, &tapped, &tx, &ty
        );

        if (tapped) {
            int h = platform->fb_height;

            if (ty >= h - 95 && ty < h - 54) {
                selected = 0;
                key = BIDHATA_KEY_SELECT;
            } else if (ty >= h - 54 && ty < h - 18) {
                selected = 1;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {
        case BIDHATA_KEY_UP:
        case BIDHATA_KEY_DOWN:
            selected = 1 - selected;
            break;

        case BIDHATA_KEY_BACK:
            return 0;

        case BIDHATA_KEY_SELECT:
            return selected == 0 ? 1 : 0;

        default:
            break;
        }

        usleep(50000);
    }
}

static void geos_report_show_summary(
    bidhata_platform_t *platform,
    const geos_saved_report_t *r)
{
    char lines[12][112];
    int count = 0;
    char value[160] = "";
    char when[48] = "Unknown";

    struct tm *tm_info = localtime(&r->modified);

    if (tm_info) {
        if (strftime(
                when, sizeof(when),
                "%Y-%m-%d %H:%M", tm_info) == 0) {
            snprintf(when, sizeof(when), "Unknown");
        }
    }

    snprintf(
        lines[count++], sizeof(lines[0]),
        "DATE: %.30s", when
    );

    if (strcmp(r->type, "PCAP") == 0) {
        struct stat st;
        long long bytes = 0;
        if (stat(r->path, &st) == 0)
            bytes = (long long)st.st_size;
        snprintf(lines[count++], sizeof(lines[0]), "FILE: %.83s", r->filename);
        snprintf(lines[count++], sizeof(lines[0]), "SIZE: %lld bytes", bytes);
        snprintf(lines[count++], sizeof(lines[0]), "FORMAT: PCAP packet capture");
    } else if (strcmp(r->type, "NMAP") == 0) {
        if (geos_report_read_value(
                r->path, "Hostname: ", value, sizeof(value))) {
            snprintf(
                lines[count++], sizeof(lines[0]),
                "HOST: %.45s", value
            );
        }

        value[0] = '\0';
        geos_report_read_value(
            r->path, "Target IP: ", value, sizeof(value));

        snprintf(
            lines[count++], sizeof(lines[0]),
            "IP: %.48s", value[0] ? value : "Unknown"
        );

        value[0] = '\0';
        geos_report_read_value(
            r->path, "Ping: ", value, sizeof(value));

        snprintf(
            lines[count++], sizeof(lines[0]),
            "PING: %.45s", value[0] ? value : "Unknown"
        );

        value[0] = '\0';
        geos_report_read_value(
            r->path, "Port scan: ", value, sizeof(value));

        snprintf(
            lines[count++], sizeof(lines[0]),
            "PORTS: %.44s", value[0] ? value : "Unknown"
        );

        value[0] = '\0';
        geos_report_read_value(
            r->path, "Service detection: ", value, sizeof(value));

        snprintf(
            lines[count++], sizeof(lines[0]),
            "SERVICES: %.40s", value[0] ? value : "Unknown"
        );
    } else {
        const char *keys[] = {
            "SSID: ",
            "BSSID: ",
            "Signal: ",
            "Frequency: ",
            "Band: ",
            "Channel: ",
            "Advertised security summary: ",
            "Advertised encryption summary: ",
            "Device IP: ",
            "Default gateway: ",
            "DNS server: "
        };

        const char *labels[] = {
            "SSID: ",
            "BSSID: ",
            "SIGNAL: ",
            "FREQ: ",
            "BAND: ",
            "CHANNEL: ",
            "SECURITY: ",
            "ENCRYPTION: ",
            "DEVICE IP: ",
            "GATEWAY: ",
            "DNS: "
        };

        for (int i = 0; i < 11 && count < 12; i++) {
            value[0] = '\0';

            if (!geos_report_read_value(
                    r->path, keys[i], value, sizeof(value))) {
                snprintf(value, sizeof(value), "Unavailable");
            }

            snprintf(
                lines[count++], sizeof(lines[0]),
                "%.13s%.38s", labels[i], value
            );
        }
    }

    int selected_action = 1; /* Default to BACK. */

    for (;;) {
        bidhata_platform_clear(platform, COL_BG);

        draw_header(
            platform,
            strcmp(r->type, "NMAP") == 0 ? "NMAP REPORT" :
            strcmp(r->type, "PCAP") == 0 ? "PACKET CAPTURE" : "WIFI REPORT"
        );

        /*
         * Keep the report name well below the red header divider.
         */
        bidhata_font_draw(
            platform, 18, 116,
            r->label,
            COL_ACCENT, 1
        );

        for (int i = 0; i < count; i++) {
            bidhata_font_draw(
                platform,
                18,
                145 + i * 22,
                lines[i],
                COL_DIM,
                1
            );
        }

        int delete_y = platform->fb_height - 76;
        int back_y = platform->fb_height - 40;

        bidhata_font_draw(
            platform, 24, delete_y,
            selected_action == 0
                ? "> DELETE REPORT"
                : "  DELETE REPORT",
            selected_action == 0 ? COL_ACCENT : COL_DIM,
            2
        );

        bidhata_font_draw(
            platform, 24, back_y,
            selected_action == 1 ? "> BACK" : "  BACK",
            selected_action == 1 ? COL_ACCENT : COL_DIM,
            2
        );

        bool tapped = false;
        int tx = 0, ty = 0;

        bidhata_key_t key = bidhata_platform_poll_menu(
            platform, &tapped, &tx, &ty
        );

        if (tapped) {
            int h = platform->fb_height;

            if (ty >= h - 95 && ty < h - 54) {
                selected_action = 0;
                key = BIDHATA_KEY_SELECT;
            } else if (ty >= h - 54 && ty < h - 18) {
                selected_action = 1;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {
        case BIDHATA_KEY_UP:
        case BIDHATA_KEY_DOWN:
            selected_action = 1 - selected_action;
            break;

        case BIDHATA_KEY_BACK:
            return;

        case BIDHATA_KEY_SELECT:
            if (selected_action == 1)
                return;

            if (geos_report_confirm_delete(platform, r)) {
                char filename[192];
                snprintf(
                    filename, sizeof(filename),
                    "%s", r->filename
                );

                if (unlink(r->path) == 0) {
                    show_page(
                        platform,
                        "REPORT DELETED",
                        "Removed from SD card",
                        filename,
                        "POWER BACK"
                    );
                } else {
                    show_page(
                        platform,
                        "DELETE FAILED",
                        "Could not remove report",
                        "Check SD card permissions",
                        "POWER BACK"
                    );
                }

                wait_back_or_select(platform);
                return;
            }

            break;

        default:
            break;
        }

        usleep(50000);
    }
}

static void run_reports_screen(bidhata_platform_t *platform)
{
    geos_saved_report_t reports[GEOS_MAX_SAVED_REPORTS];
    int selected = 0;
    int scroll = 0;

    for (;;) {
        memset(reports, 0, sizeof(reports));
        int count = 0;

        DIR *dir = opendir("/data/mnt/sd_0");

        if (dir) {
            struct dirent *entry;

            while ((entry = readdir(dir)) != NULL &&
                   count < GEOS_MAX_SAVED_REPORTS) {
                geos_saved_report_t candidate;
                memset(&candidate, 0, sizeof(candidate));

                if (geos_report_is_pcap(entry->d_name) &&
                    strncmp(entry->d_name, "geos_tcpdump_", 13) == 0) {
                    snprintf(candidate.type,
                             sizeof(candidate.type), "PCAP");
                } else if (geos_report_is_txt(entry->d_name) &&
                    strncmp(entry->d_name,
                            "geos_wifi_report_", 17) == 0) {
                    snprintf(candidate.type,
                             sizeof(candidate.type), "WIFI");
                } else if (geos_report_is_txt(entry->d_name) &&
                           strncmp(entry->d_name,
                                   "geos_nmap_report_", 17) == 0) {
                    snprintf(candidate.type,
                             sizeof(candidate.type), "NMAP");
                } else {
                    continue;
                }

                snprintf(candidate.filename,
                         sizeof(candidate.filename), "%s",
                         entry->d_name);

                snprintf(candidate.path,
                         sizeof(candidate.path),
                         "/data/mnt/sd_0/%s",
                         entry->d_name);

                struct stat st;
                if (stat(candidate.path, &st) != 0 ||
                    !S_ISREG(st.st_mode))
                    continue;

                candidate.modified = st.st_mtime;
                geos_report_make_label(&candidate);
                reports[count++] = candidate;
            }

            closedir(dir);
        }

        qsort(
            reports, count,
            sizeof(reports[0]),
            geos_report_compare
        );

        if (count == 0) {
            show_page(
                platform,
                "REPORTS",
                "NO REPORTS FOUND",
                "Save a Wi-Fi or Nmap",
                "assessment first"
            );
            wait_back_or_select(platform);
            return;
        }

        if (selected >= count)
            selected = count - 1;
        if (selected < 0)
            selected = 0;

        int visible =
            (platform->fb_height - TOP - 60) / ROW_H;

        if (visible < 1)
            visible = 1;

        if (visible > count)
            visible = count;

        if (scroll > selected)
            scroll = selected;

        if (selected >= scroll + visible)
            scroll = selected - visible + 1;

        bool dirty = true;

        for (;;) {
            if (dirty) {
                bidhata_platform_clear(platform, COL_BG);
                draw_header(platform, "REPORTS");

                for (int row = 0; row < visible; row++) {
                    int index = scroll + row;

                    char label[112];
                    snprintf(
                        label,
                        sizeof(label),
                        "%s  %.65s",
                        reports[index].type,
                        reports[index].label
                    );

                    draw_item(
                        platform,
                        TOP + row * ROW_H,
                        index == selected,
                        label
                    );
                }

                char footer[64];
                snprintf(footer, sizeof(footer),
                         "%d REPORTS SAVED", count);

                bidhata_font_draw(
                    platform, 18,
                    platform->fb_height - 48,
                    footer, COL_DIM, 1
                );

                bidhata_font_draw(
                    platform, 18,
                    platform->fb_height - 28,
                    "SELECT VIEW  POWER BACK",
                    COL_DIM, 1
                );

                dirty = false;
            }

            bool tapped = false;
            int tx = 0, ty = 0;

            bidhata_key_t key = bidhata_platform_poll_menu(
                platform, &tapped, &tx, &ty
            );

            if (tapped) {
                int row = (ty - TOP) / ROW_H;

                if (ty >= TOP && row >= 0 && row < visible) {
                    selected = scroll + row;

                    geos_report_show_summary(
                        platform, &reports[selected]
                    );
                    break;
                }
            }

            switch (key) {
            case BIDHATA_KEY_UP:
                if (selected > 0) {
                    selected--;
                    if (selected < scroll)
                        scroll = selected;
                    dirty = true;
                }
                break;

            case BIDHATA_KEY_DOWN:
                if (selected < count - 1) {
                    selected++;
                    if (selected >= scroll + visible)
                        scroll = selected - visible + 1;
                    dirty = true;
                }
                break;

            case BIDHATA_KEY_SELECT:
                geos_report_show_summary(
                    platform, &reports[selected]
                );
                break;

            case BIDHATA_KEY_BACK:
                return;

            default:
                break;
            }

            usleep(50000);
        }
    }
}


int main(void)
{
    bidhata_platform_t platform;

    if (bidhata_platform_init(&platform) < 0)
        return 1;

    int selected = 0;
    bool dirty = true;

    for (;;) {
        /*
         * Only redraw when something changed.
         * Constant framebuffer clears cause visible flicker on the R1.
         */
        if (dirty) {
            draw_main_menu(&platform, selected);
            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                &platform,
                &tapped,
                &tap_x,
                &tap_y
            );

        if (tapped) {
            int row = (tap_y - TOP) / ROW_H;

            if (tap_y >= TOP &&
                row >= 0 &&
                row < 6) {
                selected = row;
                key = BIDHATA_KEY_SELECT;
            }
        }

        switch (key) {
        case BIDHATA_KEY_UP:
            selected = (selected + 5) % 6;
            dirty = true;
            break;

        case BIDHATA_KEY_DOWN:
            selected = (selected + 1) % 6;
            dirty = true;
            break;

        case BIDHATA_KEY_BACK:
            bidhata_platform_destroy(&platform);
            return 0;

        case BIDHATA_KEY_SELECT:
            switch (selected) {

            case 0:
                run_network_screen(&platform);
                dirty = true;
                break;

            case 1:
                run_recon_screen(&platform);
                dirty = true;
                break;

            case 2:
                show_page(
                    &platform,
                    "SYSTEM",
                    "PROCESSES",
                    "STORAGE",
                    "System diagnostics."
                );
                wait_back_or_select(&platform);
                dirty = true;
                break;

            case 3:
                show_page(
                    &platform,
                    "UTILITIES",
                    "PING",
                    "SYSTEM INFO",
                    "GEOS CYBER LAB"
                );
                wait_back_or_select(&platform);
                dirty = true;
                break;

            case 4:
                run_reports_screen(&platform);
                dirty = true;
                break;

            case 5:
                bidhata_platform_destroy(&platform);
                return 0;
            }
            break;

        default:
            break;
        }

        usleep(50000);
    }
}

/* ===== GEOS active monitor: geos-mon (monitor mode) + geos-dump (capture) ===== */
#define GEOS_SD_DIR   "/data/mnt/sd_0"
#define GEOS_DUMP_BIN GEOS_SD_DIR "/geos-dump"
#define GEOS_MON_BIN  GEOS_SD_DIR "/geos-mon"
#define GEOS_MON_LOG  GEOS_SD_DIR "/geos_monitor.log"
#define GEOS_WLAN_IF  "wlan0"

static char geos_wpa_restore_cmd[1024];

static int geos_freq_to_channel(const char *freq)
{
    int f = freq ? atoi(freq) : 0;

    if (f == 2484)
        return 14;
    if (f >= 2412 && f <= 2472)
        return (f - 2407) / 5;
    if (f >= 5000 && f <= 5900)
        return (f - 5000) / 5;
    return 0;
}

static int geos_iface_link_type(const char *iface)
{
    int t = -1;
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/type", iface);
    FILE *f = fopen(path, "r");

    if (f) {
        if (fscanf(f, "%d", &t) != 1)
            t = -1;
        fclose(f);
    }
    return t;
}

static pid_t geos_spawn(char *const argv[])
{
    pid_t pid = fork();

    if (pid < 0)
        return -1;
    if (pid == 0) {
        int fd = open(GEOS_MON_LOG, O_WRONLY | O_CREAT | O_APPEND, 0644);

        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
        }
        execv(argv[0], argv);
        _exit(127);
    }
    return pid;
}

static int geos_run_wait(char *const argv[])
{
    int st = 0;
    pid_t pid = geos_spawn(argv);

    if (pid < 0)
        return -1;
    while (waitpid(pid, &st, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/*
 * wpa_supplicant fights monitor mode, so stop it. Remember its exact command
 * line so it can be relaunched identically afterwards.
 */
static void geos_wpa_stop(void)
{
    int pid = 0;
    FILE *p;

    geos_wpa_restore_cmd[0] = '\0';
    p = popen("pidof wpa_supplicant 2>/dev/null", "r");
    if (p) {
        char b[32];

        if (fgets(b, sizeof(b), p))
            pid = atoi(b);
        pclose(p);
    }
    if (pid <= 0)
        return;

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");

    if (f) {
        char raw[768];
        size_t n = fread(raw, 1, sizeof(raw) - 1, f);
        size_t off = 0, used = 0;
        int ok = (n > 0 && n < sizeof(raw) - 1);

        fclose(f);
        raw[n] = '\0';
        while (ok && off < n) {
            const char *a = raw + off;
            size_t l = strlen(a);
            int w;

            if (strchr(a, 39)) {
                ok = 0;
                break;
            }
            w = snprintf(geos_wpa_restore_cmd + used,
                         sizeof(geos_wpa_restore_cmd) - used,
                         "'%s' ", a);
            if (w < 0 || (size_t)w >= sizeof(geos_wpa_restore_cmd) - used) {
                ok = 0;
                break;
            }
            used += (size_t)w;
            off += l + 1;
        }
        if (!ok)
            geos_wpa_restore_cmd[0] = '\0';
    }
    kill(pid, SIGTERM);
    usleep(400000);
}

static void geos_wpa_start(void)
{
    if (geos_wpa_restore_cmd[0]) {
        char cmd[1100];

        snprintf(cmd, sizeof(cmd), "%s >/dev/null 2>&1 &", geos_wpa_restore_cmd);
        if (system(cmd) != 0) {
            /* best effort; the Wi-Fi menu or a reboot restores it */
        }
        geos_wpa_restore_cmd[0] = '\0';
    }
}

static void geos_monitor_off(void)
{
    char *mon_off[] = {
        (char *)GEOS_MON_BIN, (char *)GEOS_WLAN_IF, (char *)"off", NULL
    };

    (void)geos_run_wait(mon_off);
    geos_wpa_start();
}

static void run_active_monitor_screen(
    bidhata_platform_t *platform,
    const char *bssid,
    const char *frequency)
{
    char timestamp[32];
    time_t t0 = time(NULL);
    struct tm *tm0 = localtime(&t0);

    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm0);

    char filepath[256];
    snprintf(filepath, sizeof(filepath),
             GEOS_SD_DIR "/geos_tcpdump_%s.pcap", timestamp);

    if (access(GEOS_DUMP_BIN, X_OK) != 0) {
        show_page(
            platform,
            "ACTIVE MONITOR",
            "Error: geos-dump missing",
            "Copy it to the SD card",
            "PRESS BACK TO RETURN"
        );
        wait_back_or_select(platform);
        return;
    }

    int channel = geos_freq_to_channel(frequency);
    bool monitor_attempted = false;
    bool monitor_on = false;
    int linktype = 1;
    const char *capture_iface = GEOS_WLAN_IF;
    char mode_label[24];

    snprintf(mode_label, sizeof(mode_label), "MANAGED");

    if (channel > 0 && bssid && bssid[0] && access(GEOS_MON_BIN, X_OK) == 0) {
        char chbuf[8];
        char *mon_on[5];
        int rc, type;

        monitor_attempted = true;
        show_page(
            platform,
            "ACTIVE MONITOR",
            "Entering monitor mode",
            bssid,
            "PLEASE WAIT"
        );
        geos_wpa_stop();

        snprintf(chbuf, sizeof(chbuf), "%d", channel);
        mon_on[0] = (char *)GEOS_MON_BIN;
        mon_on[1] = (char *)GEOS_WLAN_IF;
        mon_on[2] = (char *)"on";
        mon_on[3] = chbuf;
        mon_on[4] = NULL;
        rc = geos_run_wait(mon_on);
        type = geos_iface_link_type(GEOS_WLAN_IF);

        char *mon_check[] = {
            (char *)GEOS_MON_BIN, (char *)GEOS_WLAN_IF,
            (char *)"check", NULL
        };
        if (rc == 0 && geos_run_wait(mon_check) == 0) {
            monitor_on = true;
            /* Prefer the driver's dedicated raw monitor netdev when it
             * creates one. Capturing on wlan0 can record Broadcom firmware
             * event packets (EtherType 0x886c), not over-the-air 802.11. */
            int prism_type = geos_iface_link_type("prism0");
            if (prism_type == 802) {
                capture_iface = "prism0";
                linktype = 119;
            } else if (type == 803)
                linktype = 127;
            else if (type == 802)
                linktype = 119;
            else
                linktype = 1;
            snprintf(mode_label, sizeof(mode_label), "MON ch%d", channel);
        } else {
            geos_monitor_off();
        }
    }

    char lt_buf[8];
    char *dump_argv[12];
    int na = 0;

    snprintf(lt_buf, sizeof(lt_buf), "%d", linktype);
    dump_argv[na++] = (char *)GEOS_DUMP_BIN;
    dump_argv[na++] = (char *)"-i";
    dump_argv[na++] = (char *)capture_iface;
    dump_argv[na++] = (char *)"-w";
    dump_argv[na++] = filepath;
    if (monitor_on) {
        dump_argv[na++] = (char *)"-L";
        dump_argv[na++] = lt_buf;
        /* The Broadcom driver only exposes monitor frames to AF_PACKET
         * when the interface is subscribed to promiscuous traffic. */
        dump_argv[na++] = (char *)"-p";
        if (linktype == 127 || linktype == 119 || linktype == 105) {
            dump_argv[na++] = (char *)"-b";
            dump_argv[na++] = (char *)bssid;
        }
    }
    dump_argv[na] = NULL;

    pid_t dump_pid = geos_spawn(dump_argv);
    bool failed = (dump_pid < 0);

    if (!failed) {
        int st0 = 0;

        usleep(400000);
        if (waitpid(dump_pid, &st0, WNOHANG) == dump_pid)
            failed = true;      /* exited immediately: no root, bad iface... */
    }
    if (failed) {
        if (monitor_on)
            geos_monitor_off();
        show_page(
            platform,
            "ACTIVE MONITOR",
            "Error: capture failed",
            "See geos_monitor.log",
            "PRESS BACK TO RETURN"
        );
        wait_back_or_select(platform);
        return;
    }

    time_t start_time = time(NULL);
    bool running = true;
    bool died = false;

    while (running) {
        struct stat st;
        long long size = 0;
        char status_line1[80];
        char status_line2[80];
        int elapsed = (int)(time(NULL) - start_time);

        if (stat(filepath, &st) == 0)
            size = (long long)st.st_size;

        snprintf(status_line1, sizeof(status_line1),
                 "%s  %d sec", mode_label, elapsed);
        if (size >= 1024LL * 1024LL)
            snprintf(status_line2, sizeof(status_line2),
                     "File: %.1f MB", (double)size / 1048576.0);
        else if (size >= 1024)
            snprintf(status_line2, sizeof(status_line2),
                     "File: %lld KB", size / 1024);
        else
            snprintf(status_line2, sizeof(status_line2),
                     "File: %lld bytes", size);

        show_page(
            platform,
            "ACTIVE MONITOR",
            status_line1,
            status_line2,
            "PRESS BACK OR SELECT TO STOP"
        );

        bool tapped = false;
        int tx = 0, ty = 0;
        bidhata_key_t key = bidhata_platform_poll_menu(platform, &tapped, &tx, &ty);

        if (key == BIDHATA_KEY_BACK || key == BIDHATA_KEY_SELECT)
            running = false;

        if (running) {
            int wst = 0;

            if (waitpid(dump_pid, &wst, WNOHANG) == dump_pid) {
                died = true;
                dump_pid = -1;
                running = false;
            }
        }

        usleep(500000);
    }

    if (dump_pid > 0) {
        int wst = 0, reaped = 0;

        kill(dump_pid, SIGTERM);
        for (int tries = 0; tries < 20 && !reaped; tries++) {
            if (waitpid(dump_pid, &wst, WNOHANG) != 0)
                reaped = 1;
            else
                usleep(100000);
        }
        if (!reaped) {
            kill(dump_pid, SIGKILL);
            waitpid(dump_pid, &wst, 0);
        }
    }

    if (monitor_on)
        geos_monitor_off();

    struct stat fst;
    long long final_size = 0;
    char saved[80];

    if (stat(filepath, &fst) == 0)
        final_size = (long long)fst.st_size;

    if (final_size <= 24)
        snprintf(saved, sizeof(saved), "No packets captured");
    else if (monitor_attempted && !monitor_on)
        snprintf(saved, sizeof(saved), "Monitor failed, managed");
    else if (died)
        snprintf(saved, sizeof(saved), "Capture ended early");
    else
        snprintf(saved, sizeof(saved), "Saved to SD card");

    show_page(
        platform,
        "CAPTURE COMPLETE",
        saved,
        filepath,
        "PRESS BACK TO RETURN"
    );
    wait_back_or_select(platform);
}
