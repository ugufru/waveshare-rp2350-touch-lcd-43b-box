// scene_swarm.cpp — Glowing orbital swarm with gyroscope-rotated viewpoint
// 120 boids orbit a center point with different radii/speeds/phases.
// Device rotation (gyroscope) rotates the entire swarm around the viewer.

#include "render.h"
#include "fix16.h"
#include "demo.h"
#include "QMI8658.h"

#define NUM_BOIDS 120

struct Boid {
    fix16_t radius;         // orbital radius
    fix16_t speed;          // angular speed (fix16 radians/frame mapped to uint8)
    uint8_t phase;          // initial phase (0-255)
    fix16_t y_offset;       // vertical offset from center plane
    fix16_t y_speed;        // vertical bob speed
    uint8_t y_phase;        // vertical phase
    uint8_t hue;            // color
};

static Boid boids[NUM_BOIDS];
static uint32_t tick = 0;
static uint32_t rng_state = 0xBEEF;
static float speed_mult = 0.3f;   // ramps up on touch
static float anim_phase = 0.0f;   // accumulated phase (advances at variable rate)

// Gyroscope-integrated rotation angles (fix16 in "uint8 angle" units)
static fix16_t rot_x = 0;
static fix16_t rot_y = 0;

static uint32_t rng(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

static fix16_t rng_range(fix16_t lo, fix16_t hi) {
    uint32_t r = rng() & 0xFFFF;
    return lo + (fix16_t)(((int64_t)(hi - lo) * r) >> 16);
}

static void read_gyro(void) {
    float acc[3], gyro[3];
    unsigned int ts;
    QMI8658_read_xyz(acc, gyro, &ts);

    // Integrate gyroscope (dps) → rotation angle
    // Map to uint8 angle units: 360° = 256 units
    // ~10x previous sensitivity for noticeable rotation
    fix16_t gx = (fix16_t)(gyro[0] * 65536.0f * 0.12f / 60.0f);
    fix16_t gy = (fix16_t)(gyro[1] * 65536.0f * 0.12f / 60.0f);

    // IIR smooth
    rot_x += gx;
    rot_y += gy;

    // Gentle drift back to center
    rot_x -= rot_x >> 8;
    rot_y -= rot_y >> 8;
}

void scene_swarm_init(void) {
    tick = 0;
    rng_state = 0xBEEF;
    speed_mult = 0.3f;
    anim_phase = 0.0f;
    rot_x = 0;
    rot_y = 0;

    for (int i = 0; i < NUM_BOIDS; i++) {
        Boid &b = boids[i];
        b.radius = rng_range(FIX(30.0f), FIX(140.0f));
        b.speed = rng_range(FIX(0.5f), FIX(2.5f));
        b.phase = (uint8_t)(rng() & 0xFF);
        b.y_offset = rng_range(FIX(-60.0f), FIX(60.0f));
        b.y_speed = rng_range(FIX(0.3f), FIX(1.2f));
        b.y_phase = (uint8_t)(rng() & 0xFF);
        b.hue = (uint8_t)(rng() & 0xFF);
    }
}

void scene_swarm_frame(void) {
    // Touch to speed up gently, ease back when released
    if (g_touch.active) {
        speed_mult += (1.5f - speed_mult) * 0.04f;
    } else {
        speed_mult += (0.3f - speed_mult) * 0.02f;
    }

    anim_phase += speed_mult;
    tick++;

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();

    read_gyro();

    // Fade for trails
    render_fade(230);

    // Rotation from gyroscope (convert fix16 to uint8 angle)
    uint8_t rx = (uint8_t)FIX2INT(rot_x);
    uint8_t ry = (uint8_t)FIX2INT(rot_y);

    // Pre-compute rotation sin/cos
    fix16_t sin_rx = fsin(rx), cos_rx = fcos(rx);
    fix16_t sin_ry = fsin(ry), cos_ry = fcos(ry);

    for (int i = 0; i < NUM_BOIDS; i++) {
        Boid &b = boids[i];

        // Orbital position (anim_phase advances smoothly at variable rate)
        int phase_int = (int)anim_phase;
        uint8_t angle = (uint8_t)(b.phase + FIX2INT(fix_mul(INT2FIX(phase_int), b.speed)));
        fix16_t bx = fix_mul(fcos(angle), b.radius);
        fix16_t bz = fix_mul(fsin(angle), b.radius);

        // Vertical bob
        uint8_t y_angle = (uint8_t)(b.y_phase + FIX2INT(fix_mul(INT2FIX(phase_int), b.y_speed)));
        fix16_t by = b.y_offset + fix_mul(fsin(y_angle), FIX(25.0f));

        // Apply gyroscope rotation (Y-axis then X-axis)
        // Rotate around Y
        fix16_t rx2 = fix_mul(bx, cos_ry) - fix_mul(bz, sin_ry);
        fix16_t rz2 = fix_mul(bx, sin_ry) + fix_mul(bz, cos_ry);
        // Rotate around X
        fix16_t ry2 = fix_mul(by, cos_rx) - fix_mul(rz2, sin_rx);
        fix16_t rz3 = fix_mul(by, sin_rx) + fix_mul(rz2, cos_rx);

        // Project to screen
        int sx, sy;
        int32_t depth;
        project(rx2, ry2, rz3, sx, sy, depth);

        if (sx < -10 || sx >= w + 10 || sy < -10 || sy >= h + 10) continue;
        if (depth < FIX(40.0f)) continue;

        // Depth-based brightness and size
        int z_int = FIX2INT(rz3 + FIX(240.0f));  // offset matches Z_OFF in render
        int brightness = 255 - z_int / 3;
        if (brightness < 40) brightness = 40;
        if (brightness > 255) brightness = 255;

        int sz = (z_int < 100) ? 4 : (z_int < 200) ? 3 : 2;

        uint16_t col = hsv565(b.hue, 255, (uint8_t)brightness);

        // Draw dot
        for (int dy = 0; dy < sz; dy++) {
            int py = sy + dy - sz / 2;
            if (py < 0 || py >= h) continue;
            for (int dx = 0; dx < sz; dx++) {
                int px = sx + dx - sz / 2;
                if (px < 0 || px >= w) continue;
                fb[py * w + px] = col;
            }
        }
    }
}
