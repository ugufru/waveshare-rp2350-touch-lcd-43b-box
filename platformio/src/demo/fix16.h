// fix16.h — Fixed-point 16.16 math, sin/cos LUT, HSV color conversion
// Extracted from ribbon.cpp for reuse across scenes

#ifndef FIX16_H
#define FIX16_H

#include <stdint.h>
#include <math.h>

// ============================================================
// Fixed-point 16.16 arithmetic
// ============================================================

typedef int32_t fix16_t;

#define FIX(x)       ((fix16_t)((x) * 65536.0f))
#define INT2FIX(x)   ((fix16_t)(x) << 16)
#define FIX2INT(x)   ((x) >> 16)

static inline fix16_t fix_mul(fix16_t a, fix16_t b) {
    return (fix16_t)(((int64_t)a * b) >> 16);
}

static inline fix16_t fix_div(fix16_t a, fix16_t b) {
    return (fix16_t)(((int64_t)a << 16) / b);
}

// ============================================================
// Sin/Cos lookup table — 256 entries, one full cycle
// Defined in fix16.cpp, shared across all translation units
// ============================================================

extern fix16_t sin_lut[256];
void build_sin_lut(void);

static inline fix16_t fsin(uint8_t angle) { return sin_lut[angle]; }
static inline fix16_t fcos(uint8_t angle) { return sin_lut[(uint8_t)(angle + 64)]; }

// ============================================================
// RGB565 packing — standard byte order for RGB parallel LCD
// ============================================================

// Pack R,G,B (0-255) to standard RGB565
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ============================================================
// HSV to RGB565
// h: 0-255 (hue), s: 0-255 (saturation), v: 0-255 (value)
// ============================================================

static inline uint16_t hsv565(uint8_t h, uint8_t s, uint8_t v) {
    uint8_t region = h / 43;
    uint8_t rem = (h - region * 43) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * rem) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;

    uint8_t r, g, b;
    switch (region) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return rgb565(r, g, b);
}

// HSV to separate R,G,B (0-255 each) for Gouraud shading
static inline void hsv_rgb(uint8_t h, uint8_t s, uint8_t v,
                           uint8_t &r, uint8_t &g, uint8_t &b) {
    uint8_t region = h / 43;
    uint8_t rem = (h - region * 43) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * rem) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;

    switch (region) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

#endif
