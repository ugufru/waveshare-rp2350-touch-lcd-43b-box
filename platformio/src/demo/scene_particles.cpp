// scene_particles.cpp — Fire/heat particle effect + accelerometer
// Particles rise upward (screen -Y). Tilt = wind. Shake = flare-up.
// Color: white-hot core → yellow → orange → red → dark
// Note: physical "up" = screen -Y, but IMU axes are rotated 90° from screen.

#include "render.h"
#include "demo.h"
#include "QMI8658.h"
#include <string.h>

#define MAX_PARTICLES 300
#define RISE_SPEED    FIX(-1.8f)   // upward on screen
#define EMIT_PER_FRAME 6

struct Particle {
    fix16_t x, y;
    fix16_t vx, vy;
    uint8_t life;
    uint8_t max_life;
};

static Particle particles[MAX_PARTICLES];
static uint32_t tick = 0;
static uint32_t rng_state = 12345;

// Smoothed accelerometer for wind
static fix16_t accel_wind = 0;
static fix16_t shake_impulse = 0;

static uint32_t rng(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

static fix16_t rng_spread(fix16_t range) {
    int32_t r = (int32_t)(rng() & 0xFFFF) - 0x8000;
    return (fix16_t)(((int64_t)r * range) >> 15);
}

// Fire color: age 0.0=white-hot → yellow → orange → red → dark ember
static uint16_t fire_color(uint8_t life, uint8_t max_life) {
    uint8_t age = (uint8_t)(255 - (uint32_t)life * 255 / max_life);
    uint8_t r, g, b;

    if (age < 60) {
        r = 255;
        g = 255 - age;
        b = (uint8_t)(220 - age * 3);
    } else if (age < 130) {
        uint8_t t = age - 60;
        r = 255;
        g = (uint8_t)(195 - t * 2);
        b = (uint8_t)(40 > t ? 40 - t : 0);
    } else if (age < 200) {
        uint8_t t = age - 130;
        r = (uint8_t)(255 - t);
        g = (uint8_t)(55 > t ? 55 - t : 0);
        b = 0;
    } else {
        uint8_t t = age - 200;
        r = (uint8_t)(185 - t * 3);
        if (r > 185) r = 0;
        g = 0;
        b = 0;
    }

    return rgb565(r, g, b);
}

static void emit_particle(Particle &p, int w, int h) {
    // Emit from bottom center, spread along X
    p.x = INT2FIX(w / 2) + rng_spread(FIX(50.0f));
    p.y = INT2FIX(h - 10) + rng_spread(FIX(8.0f));

    // Rise upward (screen -Y) with slight X spread
    p.vx = rng_spread(FIX(1.2f));
    p.vy = RISE_SPEED + rng_spread(FIX(0.8f));

    // Shake flares
    if (shake_impulse > FIX(0.5f)) {
        p.vx += rng_spread(fix_mul(shake_impulse, FIX(2.0f)));
        p.vy += rng_spread(FIX(0.5f)) - fix_mul(shake_impulse, FIX(1.0f));
    }

    // Touch Y controls particle lifetime: high = longer trails, low = shorter
    int base_life = 50;
    if (g_touch.active) {
        int h = render_get_h();
        // top of screen (y=0) → long life, bottom (y=h) → short life
        base_life = 20 + (h - g_touch.y) * 80 / h;
    }
    p.max_life = (uint8_t)(base_life + (rng() & 0x3F));
    p.life = p.max_life;
}

static void read_accel(void) {
    float acc[3], gyro[3];
    unsigned int ts;
    QMI8658_read_xyz(acc, gyro, &ts);

    // IMU is rotated 90° from screen:
    //   IMU Y-axis → screen X (wind left/right)
    //   IMU X-axis → screen Y (but we don't use this for fire)
    fix16_t raw_wind = (fix16_t)(acc[1] * 65536.0f / 500.0f);
    accel_wind = accel_wind - (accel_wind >> 2) + (raw_wind >> 2);

    // Shake from total lateral magnitude
    fix16_t raw_x = (fix16_t)(acc[0] * 65536.0f / 500.0f);
    fix16_t mag = (raw_x < 0 ? -raw_x : raw_x) + (raw_wind < 0 ? -raw_wind : raw_wind);
    fix16_t threshold = FIX(1.5f);
    if (mag > threshold) {
        shake_impulse = mag - threshold;
        if (shake_impulse > FIX(3.0f)) shake_impulse = FIX(3.0f);
    } else {
        shake_impulse = shake_impulse - (shake_impulse >> 2);
        if (shake_impulse < FIX(0.05f)) shake_impulse = 0;
    }
}

void scene_particles_init(void) {
    tick = 0;
    rng_state = 12345;
    accel_wind = 0;
    shake_impulse = 0;
    memset(particles, 0, sizeof(particles));
}

void scene_particles_frame(void) {
    tick++;

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();

    read_accel();

    // Wind: IMU Y → screen X (tilting pushes flames left/right)
    fix16_t wind = accel_wind >> 2;

    render_fade(210);

    int emit_count = EMIT_PER_FRAME;
    if (shake_impulse > FIX(0.5f)) emit_count = 16;

    int emitted = 0;
    for (int i = 0; i < MAX_PARTICLES && emitted < emit_count; i++) {
        if (particles[i].life == 0) {
            emit_particle(particles[i], w, h);
            emitted++;
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle &p = particles[i];
        if (p.life == 0) continue;

        // Turbulence jitter on X
        p.vx += rng_spread(FIX(0.15f));

        // Wind from tilt pushes flames on screen-X
        p.vx += wind;

        // Dampen horizontal velocity
        p.vx = p.vx - (p.vx >> 4);

        // Slight upward acceleration (rising heat)
        p.vy -= FIX(0.02f);

        p.x += p.vx;
        p.y += p.vy;
        p.life--;

        int sx = FIX2INT(p.x);
        int sy = FIX2INT(p.y);
        if (sx < -10 || sx >= w + 10 || sy < -20 || sy >= h + 10) {
            p.life = 0;
            continue;
        }

        uint16_t col = fire_color(p.life, p.max_life);

        int sz = (p.life > p.max_life - 10) ? 3 : 2;
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
