#include "../include/graphics.h"
#include "../include/font.h"
#include <stdarg.h>

void gfx_init(void) {
    REG_DISPCNT = MODE_3 | BG2_ENABLE;
}

void gfx_clear(u16 color) {
    u32 color32 = (u32)color | ((u32)color << 16);
    u32* vram32 = (u32*)MEM_VRAM;
    int total_words = (SCREEN_WIDTH * SCREEN_HEIGHT) / 2;
    for (int i = 0; i < total_words; i++) {
        vram32[i] = color32;
    }
}

void gfx_draw_pixel(int x, int y, u16 color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        VRAM[y * SCREEN_WIDTH + x] = color;
    }
}

void gfx_draw_line_h(int x, int y, int len, u16 color) {
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    int x1 = x;
    int x2 = x + len;
    if (x1 < 0) x1 = 0;
    if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
    u16* row = &VRAM[y * SCREEN_WIDTH];
    for (int px = x1; px < x2; px++) {
        row[px] = color;
    }
}

void gfx_draw_line_v(int x, int y, int len, u16 color) {
    if (x < 0 || x >= SCREEN_WIDTH) return;
    int y1 = y;
    int y2 = y + len;
    if (y1 < 0) y1 = 0;
    if (y2 > SCREEN_HEIGHT) y2 = SCREEN_HEIGHT;
    for (int py = y1; py < y2; py++) {
        VRAM[py * SCREEN_WIDTH + x] = color;
    }
}

void gfx_draw_rect(int x, int y, int w, int h, u16 color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH)  w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int r = 0; r < h; r++) {
        u16* row = &VRAM[(y + r) * SCREEN_WIDTH + x];
        for (int c = 0; c < w; c++) {
            row[c] = color;
        }
    }
}

void gfx_draw_rect_outline(int x, int y, int w, int h, u16 color) {
    gfx_draw_line_h(x, y, w, color);
    gfx_draw_line_h(x, y + h - 1, w, color);
    gfx_draw_line_v(x, y, h, color);
    gfx_draw_line_v(x + w - 1, y, h, color);
}

void gfx_draw_line(int x0, int y0, int x1, int y1, u16 color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0); /* dy is negative */
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        if (x0 >= 0 && x0 < SCREEN_WIDTH && y0 >= 0 && y0 < SCREEN_HEIGHT) {
            VRAM[y0 * SCREEN_WIDTH + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_draw_line_clipped(int x0, int y0, int x1, int y1, int x_min, int y_min, int x_max, int y_max, u16 color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        if (x0 >= x_min && x0 <= x_max && y0 >= y_min && y0 <= y_max) {
            VRAM[y0 * SCREEN_WIDTH + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_draw_circle(int xc, int yc, int r, u16 color, bool filled) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    while (y >= x) {
        if (filled) {
            gfx_draw_line_h(xc - x, yc - y, 2 * x + 1, color);
            gfx_draw_line_h(xc - x, yc + y, 2 * x + 1, color);
            gfx_draw_line_h(xc - y, yc - x, 2 * y + 1, color);
            gfx_draw_line_h(xc - y, yc + x, 2 * y + 1, color);
        } else {
            gfx_draw_pixel(xc + x, yc + y, color);
            gfx_draw_pixel(xc - x, yc + y, color);
            gfx_draw_pixel(xc + x, yc - y, color);
            gfx_draw_pixel(xc - x, yc - y, color);
            gfx_draw_pixel(xc + y, yc + x, color);
            gfx_draw_pixel(xc - y, yc + x, color);
            gfx_draw_pixel(xc + y, yc - x, color);
            gfx_draw_pixel(xc - y, yc - x, color);
        }
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void gfx_draw_char(int x, int y, char c, u16 fg_color, u16 bg_color, bool transparent) {
    if (c < 32 || c > 126) c = '?';
    int glyph_idx = c - 32;

    for (int col = 0; col < 5; col++) {
        int px = x + col;
        if (px < 0 || px >= SCREEN_WIDTH) continue;
        u8 col_bits = font_5x7[glyph_idx][col];

        for (int row = 0; row < 7; row++) {
            int py = y + row;
            if (py < 0 || py >= SCREEN_HEIGHT) continue;
            if (col_bits & (1 << row)) {
                VRAM[py * SCREEN_WIDTH + px] = fg_color;
            } else if (!transparent) {
                VRAM[py * SCREEN_WIDTH + px] = bg_color;
            }
        }
    }

    /* 1-pixel horizontal spacing column */
    int sp_x = x + 5;
    if (!transparent && sp_x >= 0 && sp_x < SCREEN_WIDTH) {
        for (int row = 0; row < 7; row++) {
            int py = y + row;
            if (py >= 0 && py < SCREEN_HEIGHT) {
                VRAM[py * SCREEN_WIDTH + sp_x] = bg_color;
            }
        }
    }
}

void gfx_draw_text(int x, int y, const char* text, u16 fg_color, u16 bg_color, bool transparent) {
    int cur_x = x;
    int cur_y = y;
    while (*text) {
        if (*text == '\n') {
            cur_x = x;
            cur_y += FONT_STEP_Y;
        } else {
            gfx_draw_char(cur_x, cur_y, *text, fg_color, bg_color, transparent);
            cur_x += FONT_STEP_X;
        }
        text++;
    }
}

void gfx_draw_text_clipped(int x, int y, int max_x, const char* text, u16 fg_color, u16 bg_color, bool transparent) {
    int cur_x = x;
    int cur_y = y;
    while (*text) {
        if (*text == '\n') {
            cur_x = x;
            cur_y += FONT_STEP_Y;
        } else {
            if (cur_x + 5 >= max_x) {
                break;
            }
            gfx_draw_char(cur_x, cur_y, *text, fg_color, bg_color, transparent);
            cur_x += FONT_STEP_X;
        }
        text++;
    }
}

static void int_to_str(char* buf, unsigned int uval, int radix, bool is_signed,
                       int pad_zero, int min_width, bool left_align, bool force_sign) {
    char temp[32];
    int t_idx = 0;
    bool neg = false;

    if (is_signed && radix == 10 && (int)uval < 0) {
        neg = true;
        uval = (unsigned int)(-(int)uval);
    }

    if (uval == 0) {
        temp[t_idx++] = '0';
    } else {
        while (uval > 0) {
            unsigned int rem = uval % (unsigned int)radix;
            temp[t_idx++] = (char)((rem < 10) ? ('0' + rem) : ('A' + rem - 10));
            uval /= (unsigned int)radix;
        }
    }

    char sign_ch = 0;
    if (neg) {
        sign_ch = '-';
    } else if (force_sign && radix == 10 && is_signed) {
        sign_ch = '+';
    }

    int total_len = t_idx + (sign_ch ? 1 : 0);
    int pad_cnt = (min_width > total_len) ? (min_width - total_len) : 0;
    int out_idx = 0;

    if (!left_align && !pad_zero) {
        while (pad_cnt-- > 0) buf[out_idx++] = ' ';
    }
    if (sign_ch) {
        buf[out_idx++] = sign_ch;
    }
    if (!left_align && pad_zero) {
        while (pad_cnt-- > 0) buf[out_idx++] = '0';
    }

    for (int i = t_idx - 1; i >= 0; i--) {
        buf[out_idx++] = temp[i];
    }

    if (left_align) {
        while (pad_cnt-- > 0) buf[out_idx++] = ' ';
    }
    buf[out_idx] = '\0';
}

void gfx_draw_text_fmt(int x, int y, u16 fg_color, u16 bg_color, bool transparent, const char* fmt, ...) {
    char buffer[128];
    int buf_idx = 0;

    va_list args;
    va_start(args, fmt);

    while (*fmt && buf_idx < 127) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == '\0') break;

            int pad_zero = 0;
            int width = 0;
            int precision = -1;
            bool force_sign = false;
            bool left_align = false;

            /* Parse flags in any order */
            while (*fmt == '-' || *fmt == '+' || *fmt == '0') {
                if (*fmt == '-') left_align = true;
                else if (*fmt == '+') force_sign = true;
                else if (*fmt == '0') pad_zero = 1;
                fmt++;
            }
            if (left_align) pad_zero = 0;

            /* Parse minimum field width */
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            /* Parse optional precision */
            if (*fmt == '.') {
                fmt++;
                precision = 0;
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }

            if (*fmt == 'd' || *fmt == 'i') {
                int v = va_arg(args, int);
                char num_buf[36];
                int_to_str(num_buf, (unsigned int)v, 10, true, pad_zero, width, left_align, force_sign);
                for (char* p = num_buf; *p && buf_idx < 127; p++) buffer[buf_idx++] = *p;
            } else if (*fmt == 'u') {
                unsigned int v = va_arg(args, unsigned int);
                char num_buf[36];
                int_to_str(num_buf, v, 10, false, pad_zero, width, left_align, false);
                for (char* p = num_buf; *p && buf_idx < 127; p++) buffer[buf_idx++] = *p;
            } else if (*fmt == 'x' || *fmt == 'X') {
                unsigned int v = va_arg(args, unsigned int);
                char num_buf[36];
                int_to_str(num_buf, v, 16, false, pad_zero, width, left_align, false);
                for (char* p = num_buf; *p && buf_idx < 127; p++) buffer[buf_idx++] = *p;
            } else if (*fmt == 's') {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                int len = 0;
                while (s[len] && (precision < 0 || len < precision)) len++;
                int pad = (width > len) ? (width - len) : 0;
                if (!left_align) {
                    while (pad-- > 0 && buf_idx < 127) buffer[buf_idx++] = ' ';
                }
                for (int i = 0; i < len && buf_idx < 127; i++) {
                    buffer[buf_idx++] = s[i];
                }
                if (left_align) {
                    while (pad-- > 0 && buf_idx < 127) buffer[buf_idx++] = ' ';
                }
            } else if (*fmt == 'c') {
                char c = (char)va_arg(args, int);
                if (buf_idx < 127) buffer[buf_idx++] = c;
            } else if (*fmt == '%') {
                if (buf_idx < 127) buffer[buf_idx++] = '%';
            } else if (*fmt == '\0') {
                break;
            }
        } else {
            buffer[buf_idx++] = *fmt;
        }
        fmt++;
    }
    buffer[buf_idx] = '\0';
    va_end(args);

    gfx_draw_text(x, y, buffer, fg_color, bg_color, transparent);
}

void gfx_draw_card(int x, int y, int w, int h, const char* title, u16 border_color) {
    /* Fill card interior */
    gfx_draw_rect(x, y, w, h, COLOR_CARD_BG);
    /* Outer border */
    gfx_draw_rect_outline(x, y, w, h, border_color);

    if (title && title[0]) {
        /* Title pill header */
        gfx_draw_rect(x, y, w, 11, border_color);
        gfx_draw_text(x + 4, y + 2, title, COLOR_WHITE, border_color, false);
    }
}

void gfx_draw_header(const char* title, const char* subtitle) {
    /* Header background bar */
    gfx_draw_rect(0, 0, SCREEN_WIDTH, 14, COLOR_HEADER_BG);
    gfx_draw_line_h(0, 14, SCREEN_WIDTH, COLOR_CYAN);

    /* Left title */
    int title_len = 0;
    if (title) {
        gfx_draw_text(4, 3, title, COLOR_CYAN_GLOW, COLOR_HEADER_BG, true);
        const char* tp = title;
        while (*tp++) title_len++;
    }

    /* Right subtitle */
    if (subtitle) {
        int sub_len = 0;
        const char* p = subtitle;
        while (*p++) sub_len++;
        int sub_x = SCREEN_WIDTH - (sub_len * FONT_STEP_X) - 10;
        /* Only draw if subtitle does not collide with title */
        if (sub_x > (title_len * FONT_STEP_X + 8)) {
            gfx_draw_text(sub_x, 3, subtitle, COLOR_AMBER, COLOR_HEADER_BG, true);
        }
    }
}

void gfx_draw_header_chaser(int tick, const char* label, bool is_cart, bool force_redraw) {
    static int prev_step = -1;
    if (force_redraw) prev_step = -1;
    int step = (tick / 5) % 6;
    if (step == prev_step && !force_redraw) return;
    prev_step = step;

    /* Draw retro label if provided */
    if (label) {
        gfx_draw_text(154, 3, label, COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    }

    /* 6-segment LED chaser at x=194, 201, 208, 215, 222, 229 (each 5x6 px, y=4..9) */
    int trail1 = (step - 1 + 6) % 6;
    int trail2 = (step - 2 + 6) % 6;

    u16 head_col   = is_cart ? RGB555(4, 31, 14) : RGB555(0, 31, 31);
    u16 trail1_col = is_cart ? RGB555(2, 18, 8)  : RGB555(0, 18, 20);
    u16 trail2_col = is_cart ? RGB555(1, 10, 4)  : RGB555(0, 9, 10);
    u16 off_col    = RGB555(1, 4, 3);
    u16 bezel_col  = COLOR_CARD_BORDER;

    for (int i = 0; i < 6; i++) {
        int x = 194 + i * 7;
        int y = 4;
        u16 inner_col = off_col;
        if (i == step) {
            inner_col = head_col;
        } else if (i == trail1) {
            inner_col = trail1_col;
        } else if (i == trail2) {
            inner_col = trail2_col;
        }

        /* 5x6 LED with 1px outer bezel */
        gfx_draw_rect(x, y, 5, 6, bezel_col);
        gfx_draw_rect(x + 1, y + 1, 3, 4, inner_col);
        if (i == step) {
            /* Active LED convex highlight glint pixel */
            gfx_draw_pixel(x + 1, y + 1, COLOR_WHITE);
        }
    }
}

void gfx_draw_footer(const char* hints) {
    /* Footer background bar */
    gfx_draw_rect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12, COLOR_HEADER_BG);
    gfx_draw_line_h(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, COLOR_DARK_GRAY);

    if (hints) {
        gfx_draw_text(4, SCREEN_HEIGHT - 10, hints, COLOR_LIGHT_GRAY, COLOR_HEADER_BG, true);
    }
}

void gfx_draw_progress_bar(int x, int y, int w, int h, int progress, int max, u16 fg_color, u16 bg_color) {
    gfx_draw_rect(x, y, w, h, bg_color);
    gfx_draw_rect_outline(x, y, w, h, COLOR_GRAY);

    if (max > 0 && progress > 0) {
        if (progress > max) progress = max;
        int bar_w = (progress * (w - 2)) / max;
        if (bar_w > 0) {
            gfx_draw_rect(x + 1, y + 1, bar_w, h - 2, fg_color);
        }
    }
}
