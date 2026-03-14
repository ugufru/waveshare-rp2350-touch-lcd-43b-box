// scene_wormhole.cpp — IMU-steerable starfield tunnel
// Stars rush toward viewer in a cylindrical tunnel. Tilt shifts tunnel center.
// Uses render_fade() for speed-line trails.

#include "render.h"
#include "fix16.h"
#include "demo.h"
#include "QMI8658.h"

#define NUM_STARS 200
#define FAR_Z     FIX(600.0f)
#define NEAR_Z    FIX(5.0f)
#define SPEED     FIX(8.0f)
#define TUNNEL_R  FIX(120.0f)

struct Star {
    fix16_t x, y, z;       // 3D position in tunnel
    uint8_t hue;            // 0 = white, nonzero = colored
};

static Star stars[NUM_STARS];
static uint32_t tick = 0;
static uint32_t rng_state = 0xDEAD;

// Smoothed IMU steering (float for precision — hardware FPU)
static float steer_x = 0.0f;
static float steer_y = 0.0f;
static float cal_x = 0.0f, cal_y = 0.0f;  // calibration offset (resting position)
static bool calibrated = false;

static uint32_t rng(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

static fix16_t rng_range(fix16_t lo, fix16_t hi) {
    uint32_t r = rng() & 0xFFFF;
    return lo + (fix16_t)(((int64_t)(hi - lo) * r) >> 16);
}

// Smoothed tunnel origin and spawn radius
static float origin_x, origin_y;     // current smoothed origin
static float spawn_min_r = 10.0f;    // current minimum spawn radius

static void spawn_star(Star &s, bool far_only) {
    // Random angle around tunnel
    uint8_t angle = (uint8_t)(rng() & 0xFF);
    fix16_t radius = rng_range(FIX(spawn_min_r), TUNNEL_R);
    s.x = fix_mul(fcos(angle), radius);
    s.y = fix_mul(fsin(angle), radius);
    s.z = far_only ? rng_range(FIX(400.0f), FAR_Z) : rng_range(NEAR_Z, FAR_Z);
    // 85% white, 15% colored
    s.hue = (rng() % 100 < 85) ? 0 : (uint8_t)(rng() & 0xFF);
}

static void read_imu(void) {
    float acc[3], gyro[3];
    unsigned int ts;
    QMI8658_read_xyz(acc, gyro, &ts);

    // Calibrate on first few reads (average resting position)
    if (!calibrated) {
        cal_x = acc[1];
        cal_y = acc[0];
        calibrated = true;
    }

    // IMU Y → screen X, IMU X → screen Y (rotated 90°)
    // Subtract calibration offset so resting = center
    float raw_x = (acc[1] - cal_x) * 0.16f;
    float raw_y = (acc[0] - cal_y) * 0.16f;
    // IIR smooth (factor 0.12 = responsive but not jittery)
    steer_x += (raw_x - steer_x) * 0.12f;
    steer_y += (raw_y - steer_y) * 0.12f;
}

void scene_wormhole_init(void) {
    tick = 0;
    rng_state = 0xDEAD;
    steer_x = 0.0f;
    steer_y = 0.0f;
    calibrated = false;
    int w = render_get_w(), h = render_get_h();
    origin_x = (float)(w / 2);
    origin_y = (float)(h / 2);
    spawn_min_r = 10.0f;
    for (int i = 0; i < NUM_STARS; i++) {
        spawn_star(stars[i], false);
    }
}

void scene_wormhole_frame(void) {
    tick++;

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();

    read_imu();

    // Smoothly interpolate origin and spawn radius
    bool hyper = g_touch.active;
    float target_x, target_y, target_r;
    if (hyper) {
        target_x = (float)g_touch.x;
        target_y = (float)g_touch.y;
        target_r = 100.0f;
    } else {
        target_x = (float)(w / 2) + steer_x;
        target_y = (float)(h / 2) + steer_y;
        target_r = 10.0f;
    }
    // Ease toward target
    origin_x += (target_x - origin_x) * 0.06f;
    origin_y += (target_y - origin_y) * 0.06f;
    spawn_min_r += (target_r - spawn_min_r) * 0.06f;

    // Hyperdrive speed/trails ramp with radius
    float hyper_t = (spawn_min_r - 10.0f) / 90.0f;  // 0..1
    fix16_t speed = FIX(8.0f) + (fix16_t)(hyper_t * (float)FIX(20.0f));
    uint8_t fade = (uint8_t)(240.0f + hyper_t * 10.0f);

    render_fade(fade);

    int cx = (int)origin_x;
    int cy = (int)origin_y;

    for (int i = 0; i < NUM_STARS; i++) {
        Star &s = stars[i];

        // Move toward viewer
        s.z -= speed;
        if (s.z < NEAR_Z) {
            spawn_star(s, true);
            // Blend toward rainbow as hyper ramps up
            if (hyper_t > 0.3f) s.hue = (uint8_t)(tick * 3 + i * 7);
            continue;
        }

        // Project
        fix16_t inv_z = fix_div(FIX(300.0f), s.z);
        int sx = cx + FIX2INT(fix_mul(s.x, inv_z));
        int sy = cy + FIX2INT(fix_mul(s.y, inv_z));

        // Off screen? Recycle
        if (sx < -5 || sx >= w + 5 || sy < -5 || sy >= h + 5) {
            spawn_star(s, true);
            continue;
        }

        // Brightness and size based on proximity (z closer = brighter/larger)
        int brightness = 255 - (int)(FIX2INT(s.z) * 255 / FIX2INT(FAR_Z));
        if (brightness < 30) brightness = 30;
        if (brightness > 255) brightness = 255;

        // Dot size scales with hyper_t
        int sz;
        if (hyper_t > 0.5f) {
            sz = (s.z < FIX(150.0f)) ? 4 : (s.z < FIX(350.0f)) ? 3 : 2;
        } else {
            sz = (s.z < FIX(100.0f)) ? 3 : (s.z < FIX(250.0f)) ? 2 : 1;
        }

        uint16_t col;
        if (hyper_t > 0.3f && s.hue != 0) {
            // Rainbow: each star cycles through hue
            uint8_t hue = (uint8_t)(s.hue + tick * 2);
            uint8_t sat = (uint8_t)(180 + hyper_t * 75);
            col = hsv565(hue, sat, (uint8_t)brightness);
        } else if (s.hue == 0) {
            // White/blue star
            uint8_t bv = (uint8_t)brightness;
            col = rgb565(bv, bv, 255);
        } else {
            col = hsv565(s.hue, 180, (uint8_t)brightness);
        }

        // Draw dot
        for (int dy = 0; dy < sz; dy++) {
            int py = sy + dy;
            if (py < 0 || py >= h) continue;
            for (int dx = 0; dx < sz; dx++) {
                int px = sx + dx;
                if (px < 0 || px >= w) continue;
                fb[py * w + px] = col;
            }
        }
    }
}
