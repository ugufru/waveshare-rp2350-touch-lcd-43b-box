// scene_ribbon.cpp — Multi-ribbon scene with directional lighting
// 3 ribbons on independent Lissajous paths, depth-sorted via shared draw list

#include "render.h"
#include <string.h>

#define RIBBON_LEN   80
#define HALF_WIDTH   FIX(32.0f)
#define NUM_RIBBONS  3

// Directional light (normalized approx: (1, 1, -2) / sqrt(6))
// Pre-computed as fix16: 1/sqrt(6) ≈ 0.408
#define LIGHT_X  FIX(0.408f)
#define LIGHT_Y  FIX(0.408f)
#define LIGHT_Z  FIX(-0.816f)
#define AMBIENT  76   // ~30% of 255

struct Seg {
    fix16_t x, y, z;
    uint8_t twist;
    uint8_t hue;
};

struct Ribbon {
    Seg segs[RIBBON_LEN];
    int head;
    int count;
    // Lissajous parameters
    uint8_t freq_a, freq_b, freq_c, freq_d, freq_e;
    fix16_t amp_x, amp_y, amp_z, amp_x2, amp_z2;
    uint8_t phase_c;
    uint8_t twist_rate;
    uint8_t hue_rate;
    uint8_t hue_offset;
};

static Ribbon ribbons[NUM_RIBBONS];
static uint32_t tick = 0;

static void edge_points(const Seg &s,
                        fix16_t &lx, fix16_t &ly, fix16_t &lz,
                        fix16_t &rx, fix16_t &ry, fix16_t &rz) {
    fix16_t dx = fix_mul(HALF_WIDTH, fcos(s.twist));
    fix16_t dy = fix_mul(HALF_WIDTH, fsin(s.twist));
    fix16_t dz = fix_mul(HALF_WIDTH >> 1, fsin((uint8_t)(s.twist + 64)));

    lx = s.x + dx;  ly = s.y + dy;  lz = s.z + dz;
    rx = s.x - dx;  ry = s.y - dy;  rz = s.z - dz;
}

// Cross product of (b-a) x (c-a), returns normal components
static void face_normal(fix16_t ax, fix16_t ay, fix16_t az,
                        fix16_t bx, fix16_t by, fix16_t bz,
                        fix16_t cx, fix16_t cy, fix16_t cz,
                        fix16_t &nx, fix16_t &ny, fix16_t &nz) {
    fix16_t ux = bx - ax, uy = by - ay, uz = bz - az;
    fix16_t vx = cx - ax, vy = cy - ay, vz = cz - az;
    nx = fix_mul(uy, vz) - fix_mul(uz, vy);
    ny = fix_mul(uz, vx) - fix_mul(ux, vz);
    nz = fix_mul(ux, vy) - fix_mul(uy, vx);
}

// Lambert intensity: max(0, dot(normal, light)) + ambient
// Returns 0-255
static uint8_t lambert(fix16_t nx, fix16_t ny, fix16_t nz) {
    // Approximate normalize: find magnitude, scale
    // For speed, skip full normalize — just use sign of dot product
    int32_t dot = fix_mul(nx, LIGHT_X) + fix_mul(ny, LIGHT_Y) + fix_mul(nz, LIGHT_Z);

    // Normalize by |normal| (approximate with max component for speed)
    fix16_t anx = nx < 0 ? -nx : nx;
    fix16_t any = ny < 0 ? -ny : ny;
    fix16_t anz = nz < 0 ? -nz : nz;
    fix16_t mag = anx;
    if (any > mag) mag = any;
    if (anz > mag) mag = anz;
    if (mag < FIX(0.01f)) return AMBIENT;

    // Rough normalization: divide dot by magnitude
    int32_t intensity = (int32_t)(((int64_t)dot << 16) / mag);

    // Map to 0-255 range with ambient
    int lit = AMBIENT;
    if (intensity > 0) {
        lit += (int)((int64_t)(255 - AMBIENT) * intensity / FIX(1.5f));
        if (lit > 255) lit = 255;
    }
    return (uint8_t)lit;
}

void scene_ribbon_init(void) {
    tick = 0;

    // Ribbon 0: original path
    Ribbon &r0 = ribbons[0];
    memset(&r0, 0, sizeof(Ribbon));
    r0.freq_a = 2; r0.freq_b = 5; r0.freq_c = 2; r0.freq_d = 3; r0.freq_e = 7;
    r0.amp_x = FIX(150); r0.amp_y = FIX(180); r0.amp_z = FIX(80);
    r0.amp_x2 = FIX(60); r0.amp_z2 = FIX(40);
    r0.phase_c = 100; r0.twist_rate = 7; r0.hue_rate = 3; r0.hue_offset = 0;

    // Ribbon 1: slower, wider orbit
    Ribbon &r1 = ribbons[1];
    memset(&r1, 0, sizeof(Ribbon));
    r1.freq_a = 1; r1.freq_b = 3; r1.freq_c = 1; r1.freq_d = 2; r1.freq_e = 5;
    r1.amp_x = FIX(170); r1.amp_y = FIX(150); r1.amp_z = FIX(90);
    r1.amp_x2 = FIX(70); r1.amp_z2 = FIX(50);
    r1.phase_c = 50; r1.twist_rate = 5; r1.hue_rate = 2; r1.hue_offset = 85;

    // Ribbon 2: fast tight spiral
    Ribbon &r2 = ribbons[2];
    memset(&r2, 0, sizeof(Ribbon));
    r2.freq_a = 3; r2.freq_b = 7; r2.freq_c = 3; r2.freq_d = 5; r2.freq_e = 11;
    r2.amp_x = FIX(100); r2.amp_y = FIX(120); r2.amp_z = FIX(60);
    r2.amp_x2 = FIX(40); r2.amp_z2 = FIX(30);
    r2.phase_c = 180; r2.twist_rate = 9; r2.hue_rate = 4; r2.hue_offset = 170;
}

void scene_ribbon_frame(void) {
    tick++;

    for (int ri = 0; ri < NUM_RIBBONS; ri++) {
        Ribbon &rb = ribbons[ri];

        // Compute new head position
        uint8_t a = (uint8_t)(tick * rb.freq_a);
        uint8_t b = (uint8_t)(tick * rb.freq_b);
        uint8_t c = (uint8_t)(tick * rb.freq_c + rb.phase_c);
        uint8_t d = (uint8_t)(tick * rb.freq_d);
        uint8_t e = (uint8_t)(tick * rb.freq_e);

        Seg ns;
        ns.x = fix_mul(rb.amp_x, fsin(a)) + fix_mul(rb.amp_x2, fcos(b));
        ns.y = fix_mul(rb.amp_y, fsin(c)) + fix_mul(FIX(50), fsin(d));
        ns.z = fix_mul(rb.amp_z, fcos(a)) + fix_mul(rb.amp_z2, fsin(e));
        ns.twist = (uint8_t)(tick * rb.twist_rate);
        ns.hue   = (uint8_t)(tick * rb.hue_rate + rb.hue_offset);

        rb.head = (rb.head + 1) % RIBBON_LEN;
        rb.segs[rb.head] = ns;
        if (rb.count < RIBBON_LEN) rb.count++;

        if (rb.count < 2) continue;

        // Generate triangles
        for (int i = 0; i < rb.count - 1; i++) {
            int idx0 = (rb.head - rb.count + 1 + i + RIBBON_LEN) % RIBBON_LEN;
            int idx1 = (idx0 + 1) % RIBBON_LEN;

            fix16_t l0x, l0y, l0z, r0x, r0y, r0z;
            fix16_t l1x, l1y, l1z, r1x, r1y, r1z;
            edge_points(rb.segs[idx0], l0x, l0y, l0z, r0x, r0y, r0z);
            edge_points(rb.segs[idx1], l1x, l1y, l1z, r1x, r1y, r1z);

            // Face normal for lighting (use first triangle of quad)
            fix16_t nx, ny, nz;
            face_normal(l0x, l0y, l0z, r0x, r0y, r0z, l1x, l1y, l1z, nx, ny, nz);
            uint8_t lit = lambert(nx, ny, nz);

            // Base color from hue + lighting
            uint8_t brightness = (uint8_t)((uint32_t)(50 + (uint32_t)i * 205 / rb.count) * lit / 255);
            uint8_t cr, cg, cb;
            hsv_rgb(rb.segs[idx1].hue, 220, brightness, cr, cg, cb);

            // Project
            int sl0x, sl0y, sr0x, sr0y, sl1x, sl1y, sr1x, sr1y;
            int32_t d0, d1, d2, d3;
            project(l0x, l0y, l0z, sl0x, sl0y, d0);
            project(r0x, r0y, r0z, sr0x, sr0y, d1);
            project(l1x, l1y, l1z, sl1x, sl1y, d2);
            project(r1x, r1y, r1z, sr1x, sr1y, d3);

            int32_t depth = (d0 + d1 + d2 + d3) / 4;

            // Flat-shaded per face (lit color is uniform per quad)
            Tri t1;
            t1.x0 = sl0x; t1.y0 = sl0y;
            t1.x1 = sr0x; t1.y1 = sr0y;
            t1.x2 = sl1x; t1.y2 = sl1y;
            t1.r0 = t1.r1 = t1.r2 = cr;
            t1.g0 = t1.g1 = t1.g2 = cg;
            t1.b0 = t1.b1 = t1.b2 = cb;
            t1.depth = depth;
            render_push_tri(t1);

            Tri t2;
            t2.x0 = sr0x; t2.y0 = sr0y;
            t2.x1 = sl1x; t2.y1 = sl1y;
            t2.x2 = sr1x; t2.y2 = sr1y;
            t2.r0 = t2.r1 = t2.r2 = cr;
            t2.g0 = t2.g1 = t2.g2 = cg;
            t2.b0 = t2.b1 = t2.b2 = cb;
            t2.depth = depth;
            render_push_tri(t2);
        }
    }
}
