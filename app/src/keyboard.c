#include "keyboard.h"
#include "font.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define KB_ACCENT   0xFFE94560
#define KB_PANEL    0xFF16213E
#define KB_HILIGHT  0xFF0F3460
#define KB_BG       0xFF1A1A2E
#define KB_TEXT     0xFFEAEAEA
#define KB_DIM      0xFF8888A0

#define KB_COLS          3
#define KB_CHAR_KEYS     12
#define KB_ACTION_KEYS   4
#define KB_TOTAL_KEYS    16

#define KB_MULTI_TAP_MS  900

/* Shared keyboard geometry: drawing and touch hit-testing use these. */
#define KB_GAP              5
#define KB_X0               9
#define KB_BUTTON_W         150
#define KB_BUTTON_H         120
#define KB_Y0               130

#define KB_ACTION_GAP       5
#define KB_ACTION_X0        2
#define KB_ACTION_W         115
#define KB_ACTION_H         100

static const char *alpha_labels[KB_CHAR_KEYS] = {
    "ABC", "DEF", "GHI",
    "JKL", "MNO", "PQRS",
    "TUV", "WXYZ", ".",
    "0", "/", ""
};

static const char *numeric_labels[KB_CHAR_KEYS] = {
    "1", "2", "3",
    "4", "5", "6",
    "7", "8", "9",
    ".", "0", "/"
};

static long long now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return
        (long long)tv.tv_sec * 1000LL +
        tv.tv_usec / 1000;
}

static const char *get_key_label(
    int index,
    bool alpha_mode)
{
    if (index < 0 || index >= KB_CHAR_KEYS)
        return "";

    return alpha_mode
        ? alpha_labels[index]
        : numeric_labels[index];
}

static void draw_button(
    bidhata_platform_t *p,
    int x,
    int y,
    int w,
    int h,
    const char *label,
    bool selected)
{
    u32 bg =
        selected
        ? KB_HILIGHT
        : KB_PANEL;

    bidhata_platform_fill_rect(
        p,
        x,
        y,
        w,
        h,
        bg
    );

    if (selected) {
        bidhata_platform_fill_rect(
            p,
            x,
            y,
            4,
            h,
            KB_ACCENT
        );
    }

    int scale = 2;

    int tw =
        bidhata_font_width(
            label,
            scale
        );

    int tx =
        x + (w - tw) / 2;

    if (tx < x + 8)
        tx = x + 8;

    int ty =
        y +
        (h - BIDHATA_FONT_H * scale) /
        2;

    bidhata_font_draw(
        p,
        tx,
        ty,
        label,
        KB_TEXT,
        scale
    );
}

static void draw_keyboard(
    bidhata_platform_t *p,
    const char *target,
    int selected,
    bool alpha_mode)
{
    bidhata_platform_clear(
        p,
        KB_BG
    );

    bidhata_font_draw(
        p,
        24,
        14,
        "GEOS / INPUT",
        KB_DIM,
        2
    );

    bidhata_font_draw(
        p,
        24,
        42,
        alpha_mode
            ? "ABC MODE"
            : "123 MODE",
        KB_ACCENT,
        2
    );

    bidhata_platform_fill_rect(
        p,
        24,
        68,
        p->fb_width - 48,
        52,
        KB_PANEL
    );

    char shown[96];

    snprintf(
        shown,
        sizeof(shown),
        "%s_",
        target
    );

    bidhata_font_draw(
        p,
        36,
        84,
        shown,
        KB_TEXT,
        2
    );

    const int gap = KB_GAP;
    const int x0 = KB_X0;
    const int button_w = KB_BUTTON_W;
    const int button_h = KB_BUTTON_H;
    const int y0 = KB_Y0;

    for (int i = 0; i < KB_CHAR_KEYS; i++) {

        int row = i / KB_COLS;
        int col = i % KB_COLS;

        int x =
            x0 +
            col * (button_w + gap);

        int y =
            y0 +
            row * (button_h + gap);

        draw_button(
            p,
            x,
            y,
            button_w,
            button_h,
            get_key_label(
                i,
                alpha_mode
            ),
            selected == i
        );
    }

    /*
     * Four action buttons.
     */
    const int action_gap = KB_ACTION_GAP;
    const int action_x0 = KB_ACTION_X0;
    const int action_w = KB_ACTION_W;
    const int action_h = KB_ACTION_H;

    const char *actions[KB_ACTION_KEYS] = {
        alpha_mode ? "123" : "ABC",
        "SPACE",
        "DEL",
        "RUN"
    };

    int action_y =
        y0 +
        4 * (button_h + gap);

    for (int i = 0; i < KB_ACTION_KEYS; i++) {

        int x =
            action_x0 +
            i * (action_w + action_gap);

        draw_button(
            p,
            x,
            action_y,
            action_w,
            action_h,
            actions[i],
            selected ==
                KB_CHAR_KEYS + i
        );
    }

    bidhata_font_draw(
        p,
        24,
        p->fb_height - 34,
        "VOL +/- MOVE   NEXT SELECT   POWER BACK",
        KB_DIM,
        1
    );
}

static void append_char(
    char *output,
    size_t output_cap,
    char c)
{
    size_t len;

    if (!output || output_cap == 0)
        return;

    len = strlen(output);

    if (len + 1 >= output_cap)
        return;

    output[len] = c;
    output[len + 1] = '\0';
}

static void multitap_key(
    char *output,
    size_t output_cap,
    int key_index,
    bool alpha_mode,
    int *last_key,
    int *tap_index,
    long long *last_tap_ms)
{
    /*
     * ABC mode has one intentionally blank slot so the
     * numeric keypad can retain all 12 positions.
     */
    if (alpha_mode && key_index == 11)
        return;


    const char *chars;
    long long now;
    size_t len;
    int char_count;

    if (!output ||
        output_cap == 0 ||
        !last_key ||
        !tap_index ||
        !last_tap_ms)
        return;

    /*
     * Numeric mode: every key is one character.
     */
    if (!alpha_mode) {

        chars =
            numeric_labels[key_index];

        if (key_index == 11 &&
            strlen(chars) > 0) {
            /*
             * Slash is key 11.
             */
        }

        if (chars[0] == '/' &&
            !output_cap) {
            return;
        }

        append_char(
            output,
            output_cap,
            chars[0]
        );

        *last_key = -1;
        *tap_index = 0;
        *last_tap_ms = 0;

        return;
    }

    /*
     * Alpha mode.
     */
    switch (key_index) {

    case 9:
        /*
         * 0 key:
         * first tap = space
         * second tap = 0
         */
        chars = " 0";
        break;

    default:
        chars =
            alpha_labels[key_index];
        break;
    }

    char_count =
        (int)strlen(chars);

    if (char_count <= 0)
        return;

    now = now_ms();
    len = strlen(output);

    /*
     * Same key quickly:
     * replace the last character.
     */
    if (*last_key == key_index &&
        len > 0 &&
        now - *last_tap_ms <
            KB_MULTI_TAP_MS) {

        *tap_index =
            (*tap_index + 1) %
            char_count;

        output[len - 1] =
            chars[*tap_index];

    } else {

        /*
         * New key:
         * append first character.
         */
        *tap_index = 0;

        append_char(
            output,
            output_cap,
            chars[0]
        );

        *last_key = key_index;
    }

    *last_tap_ms = now;
}

static void handle_touch_selection(
    bidhata_platform_t *p,
    int tap_x,
    int tap_y,
    int *selected)
{
    const int gap = KB_GAP;
    const int x0 = KB_X0;
    const int button_w = KB_BUTTON_W;
    const int button_h = KB_BUTTON_H;
    const int y0 = KB_Y0;

    if (!selected)
        return;

    /*
     * No hit until a valid button is found.
     */
    *selected = -1;

    /*
     * Character keypad.
     */
    if (tap_y >= y0 &&
        tap_y <
            y0 +
            4 * (button_h + gap)) {

        int col =
            (tap_x - x0) /
            (button_w + gap);

        int row =
            (tap_y - y0) /
            (button_h + gap);

        int local_x =
            (tap_x - x0) %
            (button_w + gap);

        int local_y =
            (tap_y - y0) %
            (button_h + gap);

        if (col >= 0 &&
            col < 3 &&
            row >= 0 &&
            row < 4 &&
            local_x < button_w &&
            local_y < button_h &&
            (row * KB_COLS + col) < KB_CHAR_KEYS) {

            *selected =
                row * KB_COLS + col;

            return;
        }
    }

    /*
     * Four action buttons.
     */
    const int action_gap = KB_ACTION_GAP;
    const int action_x0 = KB_ACTION_X0;
    const int action_w = KB_ACTION_W;
    const int action_h = KB_ACTION_H;

    int action_y =
        y0 +
        4 * (button_h + gap);

    if (tap_y >= action_y &&
        tap_y <
            action_y + action_h) {

        int col =
            (tap_x - action_x0) /
            (action_w + action_gap);

        int local_x =
            (tap_x - action_x0) %
            (action_w + action_gap);

        if (col >= 0 &&
            col < KB_ACTION_KEYS &&
            local_x < action_w) {

            *selected =
                KB_CHAR_KEYS + col;
        }
    }

    (void)p;
}

int geos_keyboard_target(
    bidhata_platform_t *p,
    char *output,
    size_t output_cap,
    bool allow_slash)
{
    if (!output ||
        output_cap == 0)
        return 0;

    output[0] = '\0';

    /*
     * Slash is always available in GEOS input because targets may
     * include subnet notation such as 192.168.1.0/24.
     */
    (void)allow_slash;

    int selected = 0;
    bool dirty = true;
    bool alpha_mode = true;

    int last_key = -1;
    int tap_index = 0;
    long long last_tap_ms = 0;

    for (;;) {

        if (dirty) {

            draw_keyboard(
                p,
                output,
                selected,
                alpha_mode
            );

            dirty = false;
        }

        bool tapped = false;
        int tap_x = 0;
        int tap_y = 0;

        bidhata_key_t key =
            bidhata_platform_poll_menu(
                p,
                &tapped,
                &tap_x,
                &tap_y
            );

        if (tapped) {

            handle_touch_selection(
                p,
                tap_x,
                tap_y,
                &selected
            );

            /*
             * Touch selects a key.
             */
            key =
                BIDHATA_KEY_SELECT;
        }

        switch (key) {

        case BIDHATA_KEY_UP:

            selected =
                (selected +
                 KB_TOTAL_KEYS - 1) %
                KB_TOTAL_KEYS;

            dirty = true;
            break;

        case BIDHATA_KEY_DOWN:

            selected =
                (selected + 1) %
                KB_TOTAL_KEYS;

            dirty = true;
            break;

        case BIDHATA_KEY_BACK:

            return 0;

        case BIDHATA_KEY_SELECT:

            /*
             * CHARACTER KEYS: 0-11
             */
            if (selected < KB_CHAR_KEYS) {

                /*
                 * Index 11 is intentionally blank in ABC mode.
                 */
                if (alpha_mode && selected == 11) {
                    last_key = -1;
                    tap_index = 0;
                    last_tap_ms = 0;
                    dirty = true;
                    break;
                }

                multitap_key(
                    output,
                    output_cap,
                    selected,
                    alpha_mode,
                    &last_key,
                    &tap_index,
                    &last_tap_ms
                );

                dirty = true;
            }

            /*
             * ACTION 12 = MODE
             */
            else if (selected == 12) {

                alpha_mode = !alpha_mode;

                last_key = -1;
                tap_index = 0;
                last_tap_ms = 0;

                dirty = true;
            }

            /*
             * ACTION 13 = SPACE
             */
            else if (selected == 13) {

                append_char(
                    output,
                    output_cap,
                    ' '
                );

                last_key = -1;
                tap_index = 0;
                last_tap_ms = 0;

                dirty = true;
            }

            /*
             * ACTION 14 = DELETE
             */
            else if (selected == 14) {

                size_t len = strlen(output);

                if (len > 0)
                    output[len - 1] = '\0';

                last_key = -1;
                tap_index = 0;
                last_tap_ms = 0;

                dirty = true;
            }

            /*
             * ACTION 15 = RUN
             */
            else if (selected == 15) {

                if (output[0] != '\0')
                    return 1;
            }

            break;

        default:
            break;
        }

        usleep(50000);
    }
}
