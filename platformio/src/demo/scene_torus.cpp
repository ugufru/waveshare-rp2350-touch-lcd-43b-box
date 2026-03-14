// scene_torus.cpp — Animated (2,3) torus knot with Lambert shading
// 24 steps around tube × 8 steps around cross-section = 384 tris

#include "render.h"
#include "demo.h"

#define TUBE_STEPS   24   // steps around the knot path
#define CROSS_STEPS  8    // steps around the tube cross-section
#define TORUS_P      2    // knot parameter p
#define TORUS_Q      3    // knot parameter q

#define KNOT_R       FIX(100.0f)  // major radius
#define TUBE_R       FIX(35.0f)   // tube radius

// Light direction (same as ribbon scene)
#define LIGHT_X  FIX(0.408f)
#define LIGHT_Y  FIX(0.408f)
#define LIGHT_Z  FIX(-0.816f)
#define AMBIENT  76

// Vertex storage for the torus mesh
struct Vec3 {
    fix16_t x, y, z;
};

static Vec3 mesh[TUBE_STEPS][CROSS_STEPS];
static uint32_t tick = 0;
static uint32_t auto_spin = 0;  // only advances when not touching

// Touch drag rotation state
static int drag_rot_x = 0;   // accumulated drag rotation (in uint8 angle units * 256)
static int drag_rot_y = 0;
static bool drag_active = false;
static int16_t drag_last_x = 0, drag_last_y = 0;

// Approximate integer square root (for normalizing)
static fix16_t fix_sqrt_approx(fix16_t val) {
    if (val <= 0) return 0;
    // Newton's method: 3 iterations
    fix16_t x = val;
    // Initial guess: shift right by half the bit position
    if (val > INT2FIX(1)) x = val >> 1;
    else x = INT2FIX(1);

    for (int i = 0; i < 6; i++) {
        if (x == 0) return 0;
        x = (x + fix_div(val, x)) >> 1;
    }
    return x;
}

static uint8_t lambert_torus(fix16_t nx, fix16_t ny, fix16_t nz) {
    // Normalize the normal vector
    fix16_t len_sq = fix_mul(nx, nx) + fix_mul(ny, ny) + fix_mul(nz, nz);
    fix16_t len = fix_sqrt_approx(len_sq);
    if (len < FIX(0.01f)) return AMBIENT;

    nx = fix_div(nx, len);
    ny = fix_div(ny, len);
    nz = fix_div(nz, len);

    int32_t dot = fix_mul(nx, LIGHT_X) + fix_mul(ny, LIGHT_Y) + fix_mul(nz, LIGHT_Z);

    int lit = AMBIENT;
    if (dot > 0) {
        lit += (int)((int64_t)(255 - AMBIENT) * dot / FIX(1.0f));
        if (lit > 255) lit = 255;
    }
    return (uint8_t)lit;
}

void scene_torus_init(void) {
    tick = 0;
    auto_spin = 0;
    drag_rot_x = 0;
    drag_rot_y = 0;
    drag_active = false;
}

void scene_torus_frame(void) {
    tick++;
    if (!g_touch.active) auto_spin++;

    // Touch drag to rotate
    if (g_touch.active) {
        if (drag_active) {
            int dx = g_touch.x - drag_last_x;
            int dy = g_touch.y - drag_last_y;
            drag_rot_y -= dx * 48;  // horizontal drag → Y rotation
            drag_rot_x += dy * 48;  // vertical drag → X rotation (inverted)
        }
        drag_last_x = g_touch.x;
        drag_last_y = g_touch.y;
        drag_active = true;
    } else {
        drag_active = false;
    }

    // Rotation: auto-spin (pauses while touching) + drag offset
    uint8_t rot_y = (uint8_t)(auto_spin * 2 + (drag_rot_y >> 8));
    uint8_t rot_x = (uint8_t)(auto_spin * 1 + (drag_rot_x >> 8));

    fix16_t cos_ry = fcos(rot_y), sin_ry = fsin(rot_y);
    fix16_t cos_rx = fcos(rot_x), sin_rx = fsin(rot_x);

    // Generate torus knot mesh
    for (int i = 0; i < TUBE_STEPS; i++) {
        // Parameter along the knot: 0..255 mapped from 0..TUBE_STEPS
        uint8_t u = (uint8_t)(i * 256 / TUBE_STEPS);

        // Torus knot center point: parametric curve
        // r(u) = (R + r*cos(q*u)) * cos(p*u), etc.
        // For a (p,q) knot on a torus:
        // x = cos(p*u) * (R + r_knot * cos(q*u))
        // y = sin(p*u) * (R + r_knot * cos(q*u))
        // z = r_knot * sin(q*u)
        uint8_t pu = (uint8_t)(u * TORUS_P);
        uint8_t qu = (uint8_t)(u * TORUS_Q);

        fix16_t knot_r = FIX(30.0f);  // small radius of knot winding
        fix16_t cx = fix_mul(fcos(pu), KNOT_R + fix_mul(knot_r, fcos(qu)));
        fix16_t cy = fix_mul(fsin(pu), KNOT_R + fix_mul(knot_r, fcos(qu)));
        fix16_t cz = fix_mul(knot_r, fsin(qu));

        // Tangent vector (approximate via finite difference)
        uint8_t u_next = (uint8_t)(u + 256 / TUBE_STEPS);
        uint8_t pu_n = (uint8_t)(u_next * TORUS_P);
        uint8_t qu_n = (uint8_t)(u_next * TORUS_Q);
        fix16_t nx_t = fix_mul(fcos(pu_n), KNOT_R + fix_mul(knot_r, fcos(qu_n))) - cx;
        fix16_t ny_t = fix_mul(fsin(pu_n), KNOT_R + fix_mul(knot_r, fcos(qu_n))) - cy;
        fix16_t nz_t = fix_mul(knot_r, fsin(qu_n)) - cz;

        // Build a local frame around the tangent
        // Use "up" vector trick to get perpendicular axes
        fix16_t up_x = 0, up_y = 0, up_z = FIX(1.0f);

        // binormal = tangent × up
        fix16_t bx = fix_mul(ny_t, up_z) - fix_mul(nz_t, up_y);
        fix16_t by = fix_mul(nz_t, up_x) - fix_mul(nx_t, up_z);
        fix16_t bz = fix_mul(nx_t, up_y) - fix_mul(ny_t, up_x);

        // Normalize binormal (approximate)
        fix16_t blen = fix_sqrt_approx(fix_mul(bx,bx) + fix_mul(by,by) + fix_mul(bz,bz));
        if (blen < FIX(0.1f)) { bx = FIX(1.0f); by = 0; bz = 0; blen = FIX(1.0f); }
        bx = fix_div(bx, blen);
        by = fix_div(by, blen);
        bz = fix_div(bz, blen);

        // normal = binormal × tangent
        fix16_t nnx = fix_mul(by, nz_t) - fix_mul(bz, ny_t);
        fix16_t nny = fix_mul(bz, nx_t) - fix_mul(bx, nz_t);
        fix16_t nnz = fix_mul(bx, ny_t) - fix_mul(by, nx_t);
        fix16_t nlen = fix_sqrt_approx(fix_mul(nnx,nnx) + fix_mul(nny,nny) + fix_mul(nnz,nnz));
        if (nlen < FIX(0.1f)) { nnx = 0; nny = FIX(1.0f); nnz = 0; nlen = FIX(1.0f); }
        nnx = fix_div(nnx, nlen);
        nny = fix_div(nny, nlen);
        nnz = fix_div(nnz, nlen);

        // Generate cross-section vertices
        for (int j = 0; j < CROSS_STEPS; j++) {
            uint8_t v = (uint8_t)(j * 256 / CROSS_STEPS);
            fix16_t cos_v = fcos(v), sin_v = fsin(v);

            // Point on tube surface = center + TUBE_R * (cos_v * normal + sin_v * binormal)
            fix16_t px = cx + fix_mul(TUBE_R, fix_mul(cos_v, nnx) + fix_mul(sin_v, bx));
            fix16_t py = cy + fix_mul(TUBE_R, fix_mul(cos_v, nny) + fix_mul(sin_v, by));
            fix16_t pz = cz + fix_mul(TUBE_R, fix_mul(cos_v, nnz) + fix_mul(sin_v, bz));

            // Apply rotation (Y then X)
            fix16_t rx = fix_mul(px, cos_ry) + fix_mul(pz, sin_ry);
            fix16_t rz = fix_mul(-px, sin_ry) + fix_mul(pz, cos_ry);
            fix16_t ry = fix_mul(py, cos_rx) - fix_mul(rz, sin_rx);
            fix16_t rz2 = fix_mul(py, sin_rx) + fix_mul(rz, cos_rx);

            mesh[i][j].x = rx;
            mesh[i][j].y = ry;
            mesh[i][j].z = rz2;
        }
    }

    // Generate triangles from mesh quads
    for (int i = 0; i < TUBE_STEPS; i++) {
        int i1 = (i + 1) % TUBE_STEPS;
        uint8_t hue = (uint8_t)(i * 256 / TUBE_STEPS + tick * 3);

        for (int j = 0; j < CROSS_STEPS; j++) {
            int j1 = (j + 1) % CROSS_STEPS;

            Vec3 &v00 = mesh[i][j];
            Vec3 &v10 = mesh[i1][j];
            Vec3 &v01 = mesh[i][j1];
            Vec3 &v11 = mesh[i1][j1];

            // Face normal from cross product
            fix16_t nx, ny, nz;
            fix16_t ux = v10.x - v00.x, uy = v10.y - v00.y, uz = v10.z - v00.z;
            fix16_t vx = v01.x - v00.x, vy = v01.y - v00.y, vz = v01.z - v00.z;
            nx = fix_mul(uy, vz) - fix_mul(uz, vy);
            ny = fix_mul(uz, vx) - fix_mul(ux, vz);
            nz = fix_mul(ux, vy) - fix_mul(uy, vx);

            uint8_t lit = lambert_torus(nx, ny, nz);

            uint8_t cr, cg, cb;
            hsv_rgb(hue, 200, lit, cr, cg, cb);

            // Project all 4 vertices
            int sx00, sy00, sx10, sy10, sx01, sy01, sx11, sy11;
            int32_t d00, d10, d01, d11;
            project(v00.x, v00.y, v00.z, sx00, sy00, d00);
            project(v10.x, v10.y, v10.z, sx10, sy10, d10);
            project(v01.x, v01.y, v01.z, sx01, sy01, d01);
            project(v11.x, v11.y, v11.z, sx11, sy11, d11);

            int32_t depth = (d00 + d10 + d01 + d11) / 4;

            Tri t1;
            t1.x0 = sx00; t1.y0 = sy00;
            t1.x1 = sx10; t1.y1 = sy10;
            t1.x2 = sx01; t1.y2 = sy01;
            t1.r0 = t1.r1 = t1.r2 = cr;
            t1.g0 = t1.g1 = t1.g2 = cg;
            t1.b0 = t1.b1 = t1.b2 = cb;
            t1.depth = depth;
            render_push_tri(t1);

            Tri t2;
            t2.x0 = sx10; t2.y0 = sy10;
            t2.x1 = sx11; t2.y1 = sy11;
            t2.x2 = sx01; t2.y2 = sy01;
            t2.r0 = t2.r1 = t2.r2 = cr;
            t2.g0 = t2.g1 = t2.g2 = cg;
            t2.b0 = t2.b1 = t2.b2 = cb;
            t2.depth = depth;
            render_push_tri(t2);
        }
    }
}
