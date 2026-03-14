// render.h — Rendering primitives: Gouraud rasterizer + depth-sorted draw list

#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "fix16.h"

// Maximum triangles in the draw list per frame
#define MAX_DRAW_TRIS 600

// Triangle with per-vertex color (for Gouraud shading)
struct Tri {
    int16_t x0, y0, x1, y1, x2, y2;       // screen coords
    uint8_t r0, g0, b0;                     // vertex 0 color (0-255)
    uint8_t r1, g1, b1;                     // vertex 1 color
    uint8_t r2, g2, b2;                     // vertex 2 color
    int32_t depth;                           // depth centroid for sorting
};

// Initialize render engine
void render_init(uint16_t *fb, int w, int h);

// Clear framebuffer to black
void render_clear(void);

// Apply fade: multiply all pixels by factor/256
void render_fade(uint8_t factor);

// Push a triangle into the draw list (returns false if list full)
bool render_push_tri(const Tri &t);

// Sort draw list by depth and render all triangles back-to-front
void render_flush(void);

// Get current draw list count
int render_tri_count(void);

// Perspective projection: world coords → screen coords
// Also returns projected depth for sorting
void project(fix16_t wx, fix16_t wy, fix16_t wz,
             int &sx, int &sy, int32_t &depth);

// Draw a line using Bresenham's algorithm (clipped to screen bounds)
void render_line(int x0, int y0, int x1, int y1, uint16_t color);

// Direct framebuffer access (for per-pixel scenes like plasma/metaballs)
uint16_t *render_get_fb(void);
int render_get_w(void);
int render_get_h(void);

#endif
