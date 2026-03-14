// scene_fractal.cpp — Continuously zooming Julia set with color cycling
// Uses hardware FPU (float) for fractal math. 4x4 coarse grid for frame rate.

#include "render.h"
#include "fix16.h"
#include <math.h>

#define GRID_STEP   4
#define MAX_ITER    28

// Julia set parameters
static float c_re, c_im;           // Julia constant
static float view_cx, view_cy;     // Viewport center
static float view_scale;           // Zoom level
static uint32_t tick = 0;

// Interesting Julia c values to morph between
static const float c_targets[][2] = {
    { -0.7269f,  0.1889f },
    { -0.8f,     0.156f  },
    {  0.285f,   0.01f   },
    { -0.4f,     0.6f    },
    {  0.355f,   0.355f  },
    { -0.54f,    0.54f   },
};
#define NUM_TARGETS (sizeof(c_targets) / sizeof(c_targets[0]))

static int target_idx = 0;
static float morph_t = 0.0f;

void scene_fractal_init(void) {
    tick = 0;
    target_idx = 0;
    morph_t = 0.0f;
    c_re = c_targets[0][0];
    c_im = c_targets[0][1];
    view_cx = 0.0f;
    view_cy = 0.0f;
    view_scale = 3.0f;     // initial zoom (covers -1.5 to 1.5)
}

void scene_fractal_frame(void) {
    tick++;

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();

    // Slowly morph c between targets
    morph_t += 0.003f;
    if (morph_t >= 1.0f) {
        morph_t -= 1.0f;
        target_idx = (target_idx + 1) % NUM_TARGETS;
    }
    int next_idx = (target_idx + 1) % NUM_TARGETS;
    float smooth = morph_t * morph_t * (3.0f - 2.0f * morph_t);  // smoothstep
    c_re = c_targets[target_idx][0] + (c_targets[next_idx][0] - c_targets[target_idx][0]) * smooth;
    c_im = c_targets[target_idx][1] + (c_targets[next_idx][1] - c_targets[target_idx][1]) * smooth;

    // Slow continuous zoom
    view_scale *= 0.997f;
    if (view_scale < 0.001f) {
        view_scale = 3.0f;  // reset zoom
    }

    // Drift viewport center slowly
    float drift_angle = tick * 0.005f;
    view_cx = sinf(drift_angle * 0.7f) * 0.3f;
    view_cy = cosf(drift_angle * 0.5f) * 0.3f;

    // Color time offset for cycling
    uint8_t color_offset = (uint8_t)(tick * 2);

    float aspect = (float)w / (float)h;
    float half_w = view_scale * aspect * 0.5f;
    float half_h = view_scale * 0.5f;
    float left = view_cx - half_w;
    float top  = view_cy - half_h;
    float px_w = view_scale * aspect / (float)w;
    float px_h = view_scale / (float)h;

    for (int gy = 0; gy < h; gy += GRID_STEP) {
        float zy0 = top + (gy + GRID_STEP / 2) * px_h;

        for (int gx = 0; gx < w; gx += GRID_STEP) {
            float zx = left + (gx + GRID_STEP / 2) * px_w;
            float zy = zy0;

            int iter = 0;
            float zx2 = zx * zx;
            float zy2 = zy * zy;

            while (zx2 + zy2 < 4.0f && iter < MAX_ITER) {
                float tmp = zx2 - zy2 + c_re;
                zy = 2.0f * zx * zy + c_im;
                zx = tmp;
                zx2 = zx * zx;
                zy2 = zy * zy;
                iter++;
            }

            uint16_t col;
            if (iter >= MAX_ITER) {
                col = 0;    // inside the set — black
            } else {
                // Smooth coloring with fractional escape
                float frac = (float)iter - log2f(log2f(zx2 + zy2));
                uint8_t hue = (uint8_t)((int)(frac * 12.0f) + color_offset);
                uint8_t val = 200 + (iter * 55 / MAX_ITER);
                if (val < 200) val = 200;
                col = hsv565(hue, 240, (uint8_t)val);
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
