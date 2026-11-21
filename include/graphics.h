#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "gba.h"

/* Graphics Initialization */
void gfx_init(void);

/* Drawing Primitives */
void gfx_clear(u16 color);
void gfx_draw_pixel(int x, int y, u16 color);
void gfx_draw_rect(int x, int y, int w, int h, u16 color);
void gfx_draw_rect_outline(int x, int y, int w, int h, u16 color);
void gfx_draw_line_h(int x, int y, int len, u16 color);
void gfx_draw_line_v(int x, int y, int len, u16 color);
void gfx_draw_line(int x0, int y0, int x1, int y1, u16 color);
void gfx_draw_line_clipped(int x0, int y0, int x1, int y1, int x_min, int y_min, int x_max, int y_max, u16 color);
void gfx_draw_circle(int xc, int yc, int r, u16 color, bool filled);

/* High-level UI Components */
void gfx_draw_card(int x, int y, int w, int h, const char* title, u16 border_color);
void gfx_draw_header(const char* title, const char* subtitle);
void gfx_draw_header_chaser(int tick, const char* label, bool is_cart, bool force_redraw);
void gfx_draw_footer(const char* hints);
void gfx_draw_progress_bar(int x, int y, int w, int h, int progress, int max, u16 fg_color, u16 bg_color);

/* Typography */
void gfx_draw_char(int x, int y, char c, u16 fg_color, u16 bg_color, bool transparent);
void gfx_draw_text(int x, int y, const char* text, u16 fg_color, u16 bg_color, bool transparent);
void gfx_draw_text_clipped(int x, int y, int max_x, const char* text, u16 fg_color, u16 bg_color, bool transparent);
void gfx_draw_text_fmt(int x, int y, u16 fg_color, u16 bg_color, bool transparent, const char* fmt, ...);

#endif /* GRAPHICS_H */
