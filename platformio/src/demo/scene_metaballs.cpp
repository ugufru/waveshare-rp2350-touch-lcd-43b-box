// scene_metaballs.cpp — Blobby organic metaball field visualization
// 5 metaballs on Lissajous paths, evaluated on coarse grid, colored by dominant blob

#include "render.h"

#define NUM_BALLS 5
#define GRID_STEP 4     // evaluate every 4th pixel (92×112 grid)
#define THRESHOLD 256   // field threshold for "inside" surface

struct Ball {
    int16_t x, y;       // screen position
    int16_t r;          // radius (in pixels)
    uint8_t hue;        // ball color
    // Lissajous motion parameters
    uint8_t freq_x, freq_y;
    uint8_t phase_x, phase_y;
    int16_t amp_x, amp_y;
};

static Ball balls[NUM_BALLS];
static uint32_t tick = 0;

void scene_metaballs_init(void) {
    tick = 0;
    int w = render_get_w();
    int h = render_get_h();
    int hw = w / 2, hh = h / 2;

    // Set up balls with different Lissajous parameters
    balls[0] = {(int16_t)hw, (int16_t)hh, 60, 0,   2, 3, 0, 0,     (int16_t)(hw-70), (int16_t)(hh-70)};
    balls[1] = {(int16_t)hw, (int16_t)hh, 50, 51,  3, 2, 64, 0,    (int16_t)(hw-60), (int16_t)(hh-80)};
    balls[2] = {(int16_t)hw, (int16_t)hh, 55, 102, 1, 3, 0, 128,   (int16_t)(hw-80), (int16_t)(hh-60)};
    balls[3] = {(int16_t)hw, (int16_t)hh, 45, 153, 3, 4, 32, 64,   (int16_t)(hw-50), (int16_t)(hh-50)};
    balls[4] = {(int16_t)hw, (int16_t)hh, 40, 204, 2, 5, 96, 32,   (int16_t)(hw-60), (int16_t)(hh-70)};
}

void scene_metaballs_frame(void) {
    tick++;

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();
    int hw = w / 2, hh = h / 2;

    // Update ball positions via Lissajous
    for (int i = 0; i < NUM_BALLS; i++) {
        Ball &b = balls[i];
        uint8_t ax = (uint8_t)(tick * b.freq_x + b.phase_x);
        uint8_t ay = (uint8_t)(tick * b.freq_y + b.phase_y);
        b.x = hw + (int16_t)(((int32_t)b.amp_x * fsin(ax)) >> 16);
        b.y = hh + (int16_t)(((int32_t)b.amp_y * fsin(ay)) >> 16);
    }

    // Evaluate field on coarse grid
    for (int gy = 0; gy < h; gy += GRID_STEP) {
        for (int gx = 0; gx < w; gx += GRID_STEP) {
            // Sample at center of block
            int px = gx + GRID_STEP / 2;
            int py = gy + GRID_STEP / 2;

            int field = 0;
            int max_contrib = 0;
            int dominant = 0;

            for (int i = 0; i < NUM_BALLS; i++) {
                int dx = px - balls[i].x;
                int dy = py - balls[i].y;
                int dist_sq = dx * dx + dy * dy;
                if (dist_sq < 1) dist_sq = 1;

                // field contribution = r^2 / dist^2 (scaled by 256)
                int r_sq = (int)balls[i].r * balls[i].r;
                int contrib = (r_sq * 256) / dist_sq;
                field += contrib;

                if (contrib > max_contrib) {
                    max_contrib = contrib;
                    dominant = i;
                }
            }

            uint16_t col;
            if (field > THRESHOLD) {
                // Inside surface — color by dominant ball
                // Brightness varies with field intensity
                int bright = 120 + (field - THRESHOLD) / 4;
                if (bright > 255) bright = 255;
                col = hsv565(balls[dominant].hue, 200, (uint8_t)bright);
            } else {
                // Outside — dark background with subtle glow
                int glow = field * 40 / THRESHOLD;
                if (glow > 40) glow = 40;
                col = hsv565(balls[dominant].hue, 180, (uint8_t)glow);
            }

            // Fill block
            int xe = gx + GRID_STEP;
            int ye = gy + GRID_STEP;
            if (xe > w) xe = w;
            if (ye > h) ye = h;
            for (int by = gy; by < ye; by++) {
                uint16_t *row = &fb[by * w + gx];
                for (int bx = gx; bx < xe; bx++) {
                    *row++ = col;
                }
            }
        }
    }
}
