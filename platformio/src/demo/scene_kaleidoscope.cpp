// scene_kaleidoscope.cpp — Mirror-symmetric vivid patterns
// 6-fold symmetry, polar coordinates, color cycling. Shows off OLED color quality.
// IMU-reactive: tilt rotates the pattern, shake randomizes blob parameters.

#include "render.h"
#include "fix16.h"
#include "QMI8658.h"

#define GRID_STEP   2
#define SYMMETRY    6         // 6-fold kaleidoscope

static uint32_t tick = 0;
static uint32_t rng_state = 0xCA1E;

// IMU state
static fix16_t tilt_angle = 0;      // rotation offset from tilt (0-255 mapped)
static fix16_t shake_impulse = 0;

// Blob parameters that change on shake
static uint8_t blob1_phase = 40;
static uint8_t blob2_phase = 100;
static uint8_t blob3_phase = 200;
static int blob1_base_r = 80;
static int blob2_base_r = 60;
static int blob3_base_r = 20;
static uint8_t hue_offset = 0;       // extra hue shift from shakes
static int ripple_freq = 3;          // ripple spatial frequency

static uint32_t rng(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

// Fast atan2 approximation returning 0-255 (full circle)
static uint8_t fast_atan2(int y, int x) {
    if (x == 0 && y == 0) return 0;

    int ax = x < 0 ? -x : x;
    int ay = y < 0 ? -y : y;

    int a;
    if (ax >= ay) {
        a = (ay * 32) / (ax + 1);
    } else {
        a = 64 - (ax * 32) / (ay + 1);
    }

    if (x >= 0 && y >= 0) return (uint8_t)a;
    if (x < 0 && y >= 0) return (uint8_t)(128 - a);
    if (x < 0 && y < 0) return (uint8_t)(128 + a);
    return (uint8_t)(256 - a);
}

static int isqrt(int n) {
    if (n <= 0) return 0;
    int x = n;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

static void read_imu(void) {
    float acc[3], gyro[3];
    unsigned int ts;
    QMI8658_read_xyz(acc, gyro, &ts);

    // Tilt → pattern rotation (IMU Y-axis → rotation angle)
    // Map tilt to a slow-moving angle offset
    fix16_t raw_tilt = (fix16_t)(acc[1] * 65536.0f / 300.0f);
    tilt_angle = tilt_angle - (tilt_angle >> 3) + (raw_tilt >> 3);

    // Shake detection from total acceleration magnitude
    fix16_t ax = (fix16_t)(acc[0] * 65536.0f / 500.0f);
    fix16_t ay = (fix16_t)(acc[1] * 65536.0f / 500.0f);
    fix16_t az = (fix16_t)(acc[2] * 65536.0f / 500.0f);
    // Subtract gravity (~1g on one axis) — just use lateral magnitude
    fix16_t mag = (ax < 0 ? -ax : ax) + (ay < 0 ? -ay : ay);

    fix16_t threshold = FIX(1.8f);
    if (mag > threshold) {
        fix16_t impulse = mag - threshold;
        if (impulse > FIX(3.0f)) impulse = FIX(3.0f);

        if (impulse > shake_impulse) {
            shake_impulse = impulse;

            // Randomize pattern on shake!
            blob1_phase = (uint8_t)(rng() & 0xFF);
            blob2_phase = (uint8_t)(rng() & 0xFF);
            blob3_phase = (uint8_t)(rng() & 0xFF);
            blob1_base_r = 50 + (int)(rng() % 60);
            blob2_base_r = 40 + (int)(rng() % 50);
            blob3_base_r = 15 + (int)(rng() % 30);
            hue_offset += 40 + (rng() % 60);  // big color jump
            ripple_freq = 2 + (int)(rng() % 4);
        }
    } else {
        // Decay shake
        shake_impulse = shake_impulse - (shake_impulse >> 3);
        if (shake_impulse < FIX(0.05f)) shake_impulse = 0;
    }

    (void)az;  // unused but read for completeness
}

void scene_kaleidoscope_init(void) {
    tick = 0;
    rng_state = 0xCA1E;
    tilt_angle = 0;
    shake_impulse = 0;
    blob1_phase = 40;
    blob2_phase = 100;
    blob3_phase = 200;
    blob1_base_r = 80;
    blob2_base_r = 60;
    blob3_base_r = 20;
    hue_offset = 0;
    ripple_freq = 3;
}

void scene_kaleidoscope_frame(void) {
    tick++;

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();
    int cx = w / 2;
    int cy = h / 2;

    read_imu();

    // Tilt adds a rotation offset to the whole kaleidoscope
    uint8_t angle_offset = (uint8_t)FIX2INT(tilt_angle);

    // Time-varying source pattern parameters
    uint8_t t1 = (uint8_t)(tick * 2);
    uint8_t t2 = (uint8_t)(tick * 3);
    uint8_t t3 = (uint8_t)(tick);
    uint8_t color_shift = (uint8_t)(tick + hue_offset);

    int wedge_size = 256 / SYMMETRY;

    for (int gy = 0; gy < h; gy += GRID_STEP) {
        int dy = gy - cy;

        for (int gx = 0; gx < w; gx += GRID_STEP) {
            int dx = gx - cx;

            // Polar coordinates
            int r = isqrt(dx * dx + dy * dy);
            uint8_t theta = fast_atan2(dy, dx);

            // Apply tilt rotation
            theta = (uint8_t)(theta + angle_offset);

            // Fold theta into one wedge
            int t = theta % wedge_size;
            int wedge_idx = theta / wedge_size;
            if (wedge_idx & 1) {
                t = wedge_size - 1 - t;
            }

            // Source pattern — blob positions driven by shake-randomized params
            int blob1_r = blob1_base_r + (fsin((uint8_t)(t1 + blob1_phase)) >> 13);
            int blob1_t = wedge_size / 3;
            int d1 = (r - blob1_r) * (r - blob1_r) / 16 + (t - blob1_t) * (t - blob1_t);
            int v1 = 8000 / (d1 + 10);

            int blob2_r = blob2_base_r + (fsin((uint8_t)(t2 + blob2_phase)) >> 13);
            int blob2_t = wedge_size * 2 / 3;
            int d2 = (r - blob2_r) * (r - blob2_r) / 16 + (t - blob2_t) * (t - blob2_t);
            int v2 = 6000 / (d2 + 10);

            int blob3_r = blob3_base_r + (fsin((uint8_t)(t3 + blob3_phase)) >> 14);
            int d3 = (r - blob3_r) * (r - blob3_r) / 20;
            int v3 = 4000 / (d3 + 10);

            // Interference ripple (frequency changes on shake)
            uint8_t ripple_angle = (uint8_t)(r * ripple_freq - (tick * 4));
            int ripple = fsin(ripple_angle) >> 15;

            int field = v1 + v2 + v3 + ripple;

            uint8_t hue = (uint8_t)(field * 2 + r + color_shift);
            int val = 80 + field * 3;
            if (val > 255) val = 255;
            uint8_t sat = (uint8_t)(220 + (fsin((uint8_t)(r + tick)) >> 14));

            uint16_t col = hsv565(hue, sat, (uint8_t)val);

            // Fill 2x2 block
            fb[gy * w + gx] = col;
            if (gx + 1 < w) fb[gy * w + gx + 1] = col;
            if (gy + 1 < h) {
                fb[(gy + 1) * w + gx] = col;
                if (gx + 1 < w) fb[(gy + 1) * w + gx + 1] = col;
            }
        }
    }
}
