// scene_plasma.cpp — Classic color-cycling plasma interference pattern
// 4 overlapping sine waves mapped to HSV hue, direct pixel writes
// Tap to shift color palette

#include "render.h"
#include "demo.h"

static uint32_t tick = 0;
static uint8_t palette_offset = 0;   // shifted by taps

void scene_plasma_init(void) {
    tick = 0;
    palette_offset = 0;
}

void scene_plasma_frame(void) {
    tick++;

    // Tap shifts the palette
    if (g_touch.tap) {
        palette_offset += 42;  // ~1/6 of hue wheel per tap
    }

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();

    uint8_t t1 = (uint8_t)(tick * 3);
    uint8_t t2 = (uint8_t)(tick * 2);
    uint8_t t3 = (uint8_t)(tick * 1);

    // Compute on 2x2 blocks for performance (quarter the pixel evals)
    for (int y = 0; y < h; y += 2) {
        // Pre-compute y-dependent terms
        uint8_t ysc = (uint8_t)(y * 256 / h);
        uint8_t wave2 = (uint8_t)(ysc * 3 + t2);
        int sin2 = fsin(wave2) >> 14;  // ~±4 range

        for (int x = 0; x < w; x += 2) {
            uint8_t xsc = (uint8_t)(x * 256 / w);

            // Wave 1: horizontal
            uint8_t wave1 = (uint8_t)(xsc * 3 + t1);
            int sin1 = fsin(wave1) >> 14;

            // Wave 3: diagonal
            uint8_t wave3 = (uint8_t)((xsc + ysc) * 2 + t3);
            int sin3 = fsin(wave3) >> 14;

            // Wave 4: radial from drifting center
            uint8_t cx = (uint8_t)(128 + (fsin((uint8_t)(tick)) >> 10));
            uint8_t cy = (uint8_t)(128 + (fcos((uint8_t)(tick + 80)) >> 10));
            int dx = xsc - cx;
            int dy = ysc - cy;
            // Approximate distance: |dx| + |dy| is cheap
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            uint8_t wave4 = (uint8_t)(dist * 2 + t1);
            int sin4 = fsin(wave4) >> 14;

            // Combine waves → hue (0-255)
            int combined = sin1 + sin2 + sin3 + sin4;  // range ~±16
            uint8_t hue = (uint8_t)(combined * 8 + tick * 2 + palette_offset);

            uint16_t col = hsv565(hue, 230, 220);

            // Fill 2x2 block
            fb[y * w + x] = col;
            if (x + 1 < w) fb[y * w + x + 1] = col;
            if (y + 1 < h) {
                fb[(y + 1) * w + x] = col;
                if (x + 1 < w) fb[(y + 1) * w + x + 1] = col;
            }
        }
    }
}
