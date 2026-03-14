// render.cpp — Gouraud triangle rasterizer + depth-sorted draw list

#include "render.h"
#include <string.h>

// Standard RGB565 for RGB parallel LCD
#define PACK565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))

static uint16_t *framebuf;
static int scr_w, scr_h;

// Draw list
static Tri draw_list[MAX_DRAW_TRIS];
static int tri_count = 0;

// ============================================================
// Perspective projection
// ============================================================

#define FOCAL  FIX(320.0f)
#define Z_OFF  FIX(240.0f)

void project(fix16_t wx, fix16_t wy, fix16_t wz,
             int &sx, int &sy, int32_t &depth) {
    fix16_t d = wz + Z_OFF;
    if (d < FIX(40.0f)) d = FIX(40.0f);

    fix16_t scale = (fix16_t)(((int64_t)FOCAL << 16) / d);

    sx = scr_w / 2 + FIX2INT(fix_mul(wx, scale));
    sy = scr_h / 2 + FIX2INT(fix_mul(wy, scale));
    depth = d;
}

// ============================================================
// Gouraud-shaded triangle rasterizer
// Interpolates R,G,B per-channel at 8-bit precision
// ============================================================

static void tri_gouraud(const Tri &tri) {
    // Copy to local for sorting
    int x0 = tri.x0, y0 = tri.y0;
    int x1 = tri.x1, y1 = tri.y1;
    int x2 = tri.x2, y2 = tri.y2;
    int cr0 = tri.r0, cg0 = tri.g0, cb0 = tri.b0;
    int cr1 = tri.r1, cg1 = tri.g1, cb1 = tri.b1;
    int cr2 = tri.r2, cg2 = tri.g2, cb2 = tri.b2;

    // Sort vertices by Y (y0 <= y1 <= y2)
    {
        int t;
        #define SWAP7(A,B) \
            t=x##A; x##A=x##B; x##B=t; \
            t=y##A; y##A=y##B; y##B=t; \
            t=cr##A; cr##A=cr##B; cr##B=t; \
            t=cg##A; cg##A=cg##B; cg##B=t; \
            t=cb##A; cb##A=cb##B; cb##B=t;

        if (y0 > y1) { SWAP7(0,1) }
        if (y0 > y2) { SWAP7(0,2) }
        if (y1 > y2) { SWAP7(1,2) }

        #undef SWAP7
    }

    if (y2 == y0) return;

    // Check if flat-shaded (all vertex colors identical) — use fast path
    bool flat = (cr0 == cr1 && cr1 == cr2 && cg0 == cg1 && cg1 == cg2 && cb0 == cb1 && cb1 == cb2);
    uint16_t flat_col = 0;
    if (flat) {
        flat_col = PACK565(cr0, cg0, cb0);
    }

    int ystart = y0 < 0 ? 0 : y0;
    int yend   = y2 >= scr_h ? scr_h - 1 : y2;

    int dy_long = y2 - y0;

    for (int y = ystart; y <= yend; y++) {
        int t_long = y - y0;

        int xa = x0 + (x2 - x0) * t_long / dy_long;
        int ra, ga, ba;
        if (!flat) {
            ra = cr0 + (cr2 - cr0) * t_long / dy_long;
            ga = cg0 + (cg2 - cg0) * t_long / dy_long;
            ba = cb0 + (cb2 - cb0) * t_long / dy_long;
        }

        int xb, rb, gb, bb;
        if (y < y1) {
            int dy_short = y1 - y0;
            if (dy_short == 0) {
                xb = x0; rb = cr0; gb = cg0; bb = cb0;
            } else {
                int t_short = y - y0;
                xb = x0 + (x1 - x0) * t_short / dy_short;
                if (!flat) {
                    rb = cr0 + (cr1 - cr0) * t_short / dy_short;
                    gb = cg0 + (cg1 - cg0) * t_short / dy_short;
                    bb = cb0 + (cb1 - cb0) * t_short / dy_short;
                }
            }
        } else {
            int dy_short = y2 - y1;
            if (dy_short == 0) {
                xb = x1; rb = cr1; gb = cg1; bb = cb1;
            } else {
                int t_short = y - y1;
                xb = x1 + (x2 - x1) * t_short / dy_short;
                if (!flat) {
                    rb = cr1 + (cr2 - cr1) * t_short / dy_short;
                    gb = cg1 + (cg2 - cg1) * t_short / dy_short;
                    bb = cb1 + (cb2 - cb1) * t_short / dy_short;
                }
            }
        }

        if (xa > xb) {
            int t = xa; xa = xb; xb = t;
            if (!flat) {
                t = ra; ra = rb; rb = t;
                t = ga; ga = gb; gb = t;
                t = ba; ba = bb; bb = t;
            }
        }

        if (xa < 0) {
            if (!flat && xb > xa) {
                int span = xb - xa;
                ra = ra + (rb - ra) * (0 - xa) / span;
                ga = ga + (gb - ga) * (0 - xa) / span;
                ba = ba + (bb - ba) * (0 - xa) / span;
            }
            xa = 0;
        }
        if (xb >= scr_w) xb = scr_w - 1;
        if (xa > xb) continue;

        uint16_t *row = &framebuf[y * scr_w + xa];
        int span = xb - xa;

        if (flat) {
            int n = span + 1;
            while (n-- > 0) *row++ = flat_col;
        } else if (span == 0) {
            *row = PACK565(ra, ga, ba);
        } else {
            for (int x = 0; x <= span; x++) {
                int r = ra + (rb - ra) * x / span;
                int g = ga + (gb - ga) * x / span;
                int b = ba + (bb - ba) * x / span;
                *row++ = PACK565(r, g, b);
            }
        }
    }
}

// ============================================================
// Draw list management
// ============================================================

void render_init(uint16_t *fb, int w, int h) {
    framebuf = fb;
    scr_w = w;
    scr_h = h;
    tri_count = 0;
}

void render_clear(void) {
    memset(framebuf, 0, scr_w * scr_h * sizeof(uint16_t));
    tri_count = 0;
}

void render_fade(uint8_t factor) {
    int npix = scr_w * scr_h;
    for (int i = 0; i < npix; i++) {
        uint16_t c = framebuf[i];
        uint8_t r = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t b = c & 0x1F;
        r = (r * factor) >> 8;
        g = (g * factor) >> 8;
        b = (b * factor) >> 8;
        framebuf[i] = (r << 11) | (g << 5) | b;
    }
}

bool render_push_tri(const Tri &t) {
    if (tri_count >= MAX_DRAW_TRIS) return false;
    draw_list[tri_count++] = t;
    return true;
}

int render_tri_count(void) {
    return tri_count;
}

// Insertion sort by depth (descending — back-to-front)
static void sort_draw_list(void) {
    for (int i = 1; i < tri_count; i++) {
        Tri key = draw_list[i];
        int j = i - 1;
        while (j >= 0 && draw_list[j].depth < key.depth) {
            draw_list[j + 1] = draw_list[j];
            j--;
        }
        draw_list[j + 1] = key;
    }
}

void render_flush(void) {
    sort_draw_list();
    for (int i = 0; i < tri_count; i++) {
        tri_gouraud(draw_list[i]);
    }
    tri_count = 0;
}

// ============================================================
// Bresenham line drawing with Cohen-Sutherland clipping
// ============================================================

// Cohen-Sutherland outcodes
#define CS_INSIDE 0
#define CS_LEFT   1
#define CS_RIGHT  2
#define CS_BOTTOM 4
#define CS_TOP    8

static int cs_outcode(int x, int y) {
    int code = CS_INSIDE;
    if (x < 0) code |= CS_LEFT;
    else if (x >= scr_w) code |= CS_RIGHT;
    if (y < 0) code |= CS_TOP;
    else if (y >= scr_h) code |= CS_BOTTOM;
    return code;
}

void render_line(int x0, int y0, int x1, int y1, uint16_t color) {
    // Cohen-Sutherland clipping
    int oc0 = cs_outcode(x0, y0);
    int oc1 = cs_outcode(x1, y1);

    for (;;) {
        if (!(oc0 | oc1)) break;       // both inside
        if (oc0 & oc1) return;          // both in same outside region
        int oc = oc0 ? oc0 : oc1;
        int x, y;
        int dx = x1 - x0, dy = y1 - y0;
        if (oc & CS_BOTTOM) {
            x = x0 + dx * (scr_h - 1 - y0) / dy;
            y = scr_h - 1;
        } else if (oc & CS_TOP) {
            x = x0 + dx * (0 - y0) / dy;
            y = 0;
        } else if (oc & CS_RIGHT) {
            y = y0 + dy * (scr_w - 1 - x0) / dx;
            x = scr_w - 1;
        } else {
            y = y0 + dy * (0 - x0) / dx;
            x = 0;
        }
        if (oc == oc0) { x0 = x; y0 = y; oc0 = cs_outcode(x0, y0); }
        else           { x1 = x; y1 = y; oc1 = cs_outcode(x1, y1); }
    }

    // Bresenham
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;

    if (dx >= dy) {
        int err = dx / 2;
        int y = y0;
        for (int x = x0; x != x1 + sx; x += sx) {
            framebuf[y * scr_w + x] = color;
            err -= dy;
            if (err < 0) { y += sy; err += dx; }
        }
    } else {
        int err = dy / 2;
        int x = x0;
        for (int y = y0; y != y1 + sy; y += sy) {
            framebuf[y * scr_w + x] = color;
            err -= dx;
            if (err < 0) { x += sx; err += dy; }
        }
    }
}

uint16_t *render_get_fb(void) { return framebuf; }
int render_get_w(void) { return scr_w; }
int render_get_h(void) { return scr_h; }
