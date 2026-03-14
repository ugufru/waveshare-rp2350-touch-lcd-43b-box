// scene_synthwave.cpp — Neon synthwave highway with sunset, mountains, wireframe car
// Daft Punk / outrun aesthetic. All line-based via render_line() + direct pixel fills.

#include "render.h"
#include "fix16.h"
#include "demo.h"
#include <math.h>

// Layout constants (368x448 display)
#define HORIZON_Y    160      // horizon line on screen
#define SUN_CY       100      // sun center Y
#define SUN_R        70       // sun radius
#define VP_X         184      // vanishing point X (center)

#define NUM_HLINES   14       // horizontal grid lines
#define NUM_VLINES   9        // vertical grid lines (odd = center line)
#define GRID_DEPTH   800.0f   // how far the grid extends

static uint32_t tick = 0;
static float scroll_z = 0.0f;
static float car_x_offset = 0.0f;   // touch drag offset for car
static float speed_boost = 0.0f;    // tap boost decays over time
static int prev_car_cx = 0;         // previous frame car center for motion blur
static int prev_car_by = 0;

// Mountain silhouette heightmap
#define MTN_POINTS 48
static int16_t mtn_height[MTN_POINTS];
static uint32_t rng_state = 0xCAFE;

static uint32_t rng(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state;
}

// Neon colors in RGB565
static uint16_t neon_cyan;
static uint16_t neon_cyan_dim;
static uint16_t neon_magenta;
static uint16_t neon_magenta_dim;
static uint16_t chrome_white;

void scene_synthwave_init(void) {
    tick = 0;
    scroll_z = 0.0f;
    rng_state = 0xCAFE;

    car_x_offset = 0.0f;
    speed_boost = 0.0f;
    prev_car_cx = VP_X;
    prev_car_by = (HORIZON_Y + 448) / 2 + 20;
    neon_cyan      = rgb565(0, 255, 255);
    neon_cyan_dim  = rgb565(0, 80, 100);
    neon_magenta   = rgb565(255, 0, 255);
    neon_magenta_dim = rgb565(80, 0, 80);
    chrome_white   = rgb565(220, 220, 255);

    // Generate mountain silhouette
    for (int i = 0; i < MTN_POINTS; i++) {
        int base = 20 + (rng() % 40);
        // Add some peaks
        if (i % 8 == 3) base += 25 + (rng() % 20);
        if (i % 12 == 7) base += 35 + (rng() % 15);
        mtn_height[i] = (int16_t)base;
    }
}

// Project a ground-plane point (world_x, world_z) to screen coords
static void ground_project(float wx, float wz, int &sx, int &sy, int scr_w) {
    // Perspective: screen_y = horizon + focal/wz, screen_x = vp + wx*focal/wz
    float focal = 200.0f;
    float inv_z = focal / wz;
    sx = VP_X + (int)(wx * inv_z);
    sy = HORIZON_Y + (int)(inv_z * 40.0f);  // 40 = ground plane scale
}

void scene_synthwave_frame(void) {
    tick++;

    // Touch: hold to accelerate, car pulls toward center as it speeds up
    if (g_touch.active) {
        int w = render_get_w();
        float target_x = (float)(g_touch.x - w / 2) * 0.8f;
        // As speed builds, car drifts toward center (pulling away)
        float pull = speed_boost / 25.0f;  // 0..1 as boost ramps
        target_x *= (1.0f - pull * 0.85f); // at full boost, 85% toward center
        car_x_offset += (target_x - car_x_offset) * 0.08f;
        speed_boost += (25.0f - speed_boost) * 0.015f;  // gradual acceleration
    } else {
        car_x_offset *= 0.92f;  // drift back to center
        speed_boost *= 0.95f;   // ease back to cruise
    }

    float speed = 6.0f + speed_boost;
    scroll_z -= speed;
    if (scroll_z < 0.0f) scroll_z += GRID_DEPTH;

    uint16_t *fb = render_get_fb();
    int w = render_get_w();
    int h = render_get_h();

    // === Sky gradient (orange → magenta → purple → dark) ===
    for (int y = 0; y < HORIZON_Y; y++) {
        float t = (float)y / (float)HORIZON_Y;
        uint8_t r, g, b;
        if (t < 0.3f) {
            // Dark purple at top
            float s = t / 0.3f;
            r = (uint8_t)(10 + s * 30);
            g = 0;
            b = (uint8_t)(20 + s * 40);
        } else if (t < 0.6f) {
            // Purple to magenta
            float s = (t - 0.3f) / 0.3f;
            r = (uint8_t)(40 + s * 160);
            g = 0;
            b = (uint8_t)(60 + s * 40);
        } else {
            // Magenta to orange at horizon
            float s = (t - 0.6f) / 0.4f;
            r = (uint8_t)(200 + s * 55);
            g = (uint8_t)(s * 100);
            b = (uint8_t)(100 - s * 80);
        }
        uint16_t col = rgb565(r, g, b);
        uint16_t *row = &fb[y * w];
        for (int x = 0; x < w; x++) *row++ = col;
    }

    // === Ground (black below horizon) ===
    for (int y = HORIZON_Y; y < h; y++) {
        uint16_t *row = &fb[y * w];
        for (int x = 0; x < w; x++) *row++ = 0;
    }

    // === Sun (filled circle with horizontal bands) ===
    for (int y = SUN_CY - SUN_R; y <= SUN_CY + SUN_R; y++) {
        if (y < 0 || y >= HORIZON_Y) continue;
        int dy = y - SUN_CY;
        int half_w_sq = SUN_R * SUN_R - dy * dy;
        if (half_w_sq <= 0) continue;
        // Integer sqrt approximation
        int hw = 0;
        for (int g = SUN_R; g > 0; g >>= 1) {
            if ((hw + g) * (hw + g) <= half_w_sq) hw += g;
        }

        float t = (float)(y - (SUN_CY - SUN_R)) / (float)(SUN_R * 2);
        uint8_t r, g_c, b;
        if (t < 0.5f) {
            r = 255; g_c = (uint8_t)(220 - t * 300); b = 50;
        } else {
            r = (uint8_t)(255 - (t - 0.5f) * 200);
            g_c = (uint8_t)(70 - (t - 0.5f) * 100);
            b = (uint8_t)(50 + (t - 0.5f) * 150);
        }
        // Horizontal scanline gaps for retro effect
        if ((y / 4) % 2 == 0 && y > SUN_CY) continue;

        uint16_t col = rgb565(r, g_c, b);
        int xl = VP_X - hw;
        int xr = VP_X + hw;
        if (xl < 0) xl = 0;
        if (xr >= w) xr = w - 1;
        for (int x = xl; x <= xr; x++) {
            fb[y * w + x] = col;
        }
    }

    // === Mountain silhouette ===
    for (int i = 0; i < MTN_POINTS - 1; i++) {
        int x0 = i * w / (MTN_POINTS - 1);
        int x1 = (i + 1) * w / (MTN_POINTS - 1);
        int h0 = HORIZON_Y - mtn_height[i];
        int h1 = HORIZON_Y - mtn_height[i + 1];

        for (int x = x0; x < x1; x++) {
            // Interpolate height
            int mh = h0 + (h1 - h0) * (x - x0) / (x1 - x0 + 1);
            if (mh < 0) mh = 0;
            uint16_t dark_purple = rgb565(15, 0, 25);
            for (int y = mh; y < HORIZON_Y; y++) {
                fb[y * w + x] = dark_purple;
            }
        }
    }

    // === Perspective grid ===
    // Horizontal lines (receding into distance)
    for (int i = 1; i <= NUM_HLINES; i++) {
        // Exponential spacing for perspective feel
        float z = 20.0f + (float)i * (float)i * 4.0f;
        float fmod_z = fmodf(z + scroll_z, GRID_DEPTH);
        if (fmod_z < 20.0f) continue;

        int sx0, sy0, sx1, sy1;
        ground_project(-500.0f, fmod_z, sx0, sy0, w);
        ground_project(500.0f, fmod_z, sx1, sy1, w);

        if (sy0 >= h || sy0 <= HORIZON_Y) continue;

        // Dim wide glow line
        render_line(sx0, sy0, sx1, sy1, neon_cyan_dim);
        // Bright center line
        render_line(sx0, sy0 + 1, sx1, sy1 + 1, neon_cyan);
    }

    // Vertical lines (converging at vanishing point)
    for (int i = 0; i < NUM_VLINES; i++) {
        float wx = (float)(i - NUM_VLINES / 2) * 80.0f;
        int sx_far, sy_far, sx_near, sy_near;
        ground_project(wx, GRID_DEPTH, sx_far, sy_far, w);
        ground_project(wx, 20.0f, sx_near, sy_near, w);

        uint16_t col = (i == NUM_VLINES / 2) ? neon_magenta : neon_cyan;
        uint16_t dim = (i == NUM_VLINES / 2) ? neon_magenta_dim : neon_cyan_dim;
        render_line(sx_far, sy_far, sx_near, sy_near, dim);
        render_line(sx_far + 1, sy_far, sx_near + 1, sy_near, col);
    }

    // === Dashed center road marking ===
    for (int i = 0; i < 8; i++) {
        float z0 = 30.0f + (float)i * 50.0f;
        float z1 = z0 + 25.0f;
        z0 = fmodf(z0 + scroll_z * 0.5f, 400.0f) + 20.0f;
        z1 = z0 + 25.0f;

        int sx0, sy0, sx1, sy1;
        ground_project(0.0f, z0, sx0, sy0, w);
        ground_project(0.0f, z1, sx1, sy1, w);
        if (sy0 < HORIZON_Y || sy1 >= h) continue;
        render_line(sx0, sy0, sx1, sy1, chrome_white);
    }

    // === Wireframe car — G37S coupe rear view (from photo reference) ===
    float pull = speed_boost / 25.0f;  // 0..1
    int base_y = (HORIZON_Y + h) / 2 + 20;
    int car_by = base_y - (int)(pull * (base_y - HORIZON_Y - 30));
    int car_cx = w / 2 + (int)car_x_offset;
    int bounce = (int)(sinf(tick * 0.08f) * 2.0f * (1.0f - pull * 0.5f));
    car_by += bounce;

    float sc = 1.0f - pull * 0.5f;

    // G37S proportions from photo — fenders are widest, bumper tucks in
    int fw   = (int)(52 * sc);   // half fender width (widest point of body)
    int bmpw = (int)(46 * sc);   // half bumper width (narrower, tucks in)
    int tkw  = (int)(20 * sc);   // half trunk width (narrow between tail lights)
    int rfw  = (int)(30 * sc);   // half roof width (wider than trunk, coupe)
    int ww   = (int)(14 * sc);   // wheel width

    // Vertical dimensions
    int bump_h  = (int)(10 * sc);  // bumper height
    int body_h  = (int)(16 * sc);  // fender/body height (tail light zone)
    int trunk_h = (int)(6 * sc);   // trunk lid height
    int win_h   = (int)(16 * sc);  // rear window height

    // Colors — silver body
    uint16_t silver      = rgb565(180, 185, 195);
    uint16_t silver_dark = rgb565(120, 125, 135);
    uint16_t silver_hi   = rgb565(210, 215, 225);
    uint16_t tail_bright = rgb565(255, 0, 0);
    uint16_t tail_mid    = rgb565(200, 0, 0);
    uint16_t tail_dim    = rgb565(130, 0, 0);
    uint16_t exhaust_col = rgb565(220, 180, 100);
    uint16_t ghost_col   = rgb565(40, 40, 60);
    uint16_t win_col     = rgb565(60, 80, 120);
    uint16_t dark_bumper = rgb565(50, 50, 60);

    // Key Y positions (car_by = bottom of bumper, going UP the screen)
    int y_bot    = car_by;                              // bottom of bumper
    int y_bmp    = car_by - bump_h;                     // top of bumper / bottom of body
    int y_fend   = y_bmp - body_h;                      // top of fenders (shoulder)
    int y_trunk  = y_fend - trunk_h;                    // top of trunk lid
    int y_roof   = y_trunk - win_h;                     // roofline

    // --- Motion blur ghost ---
    int dx_blur = car_cx - prev_car_cx;
    int dy_blur = car_by - prev_car_by;
    int blur_dist = (dx_blur < 0 ? -dx_blur : dx_blur) + (dy_blur < 0 ? -dy_blur : dy_blur);
    if (blur_dist > 2) {
        int gx = prev_car_cx, gb = prev_car_by;
        render_line(gx - bmpw, gb, gx + bmpw, gb, ghost_col);
        render_line(gx - bmpw, gb, gx - fw, gb - bump_h, ghost_col);
        render_line(gx + bmpw, gb, gx + fw, gb - bump_h, ghost_col);
        render_line(gx - fw, gb - bump_h, gx - fw, gb - bump_h - body_h, ghost_col);
        render_line(gx + fw, gb - bump_h, gx + fw, gb - bump_h - body_h, ghost_col);
    }
    prev_car_cx = car_cx;
    prev_car_by = car_by;

    // --- Neon underglow ---
    render_line(car_cx - bmpw + 8, y_bot + 1, car_cx + bmpw - 8, y_bot + 1, neon_magenta);
    render_line(car_cx - bmpw + 12, y_bot + 2, car_cx + bmpw - 12, y_bot + 2, neon_magenta_dim);

    // --- Wheels (below bumper corners) ---
    int wh = (int)(5 * sc); if (wh < 2) wh = 2;
    for (int dy = 0; dy < wh; dy++) {
        render_line(car_cx - bmpw, y_bot + 1 + dy, car_cx - bmpw + ww, y_bot + 1 + dy, silver_dark);
        render_line(car_cx + bmpw - ww, y_bot + 1 + dy, car_cx + bmpw, y_bot + 1 + dy, silver_dark);
    }
    render_line(car_cx - bmpw + 2, y_bot + 2, car_cx - bmpw + ww - 2, y_bot + 2, neon_cyan);
    render_line(car_cx + bmpw - ww + 2, y_bot + 2, car_cx + bmpw - 2, y_bot + 2, neon_cyan);

    // --- Bumper (bottom, narrower than fenders — tucks in) ---
    render_line(car_cx - bmpw, y_bot, car_cx + bmpw, y_bot, silver_dark);
    // Bumper sides flare outward going up to fenders
    render_line(car_cx - bmpw, y_bot, car_cx - fw, y_bmp, silver);
    render_line(car_cx + bmpw, y_bot, car_cx + fw, y_bmp, silver);

    // Dark lower bumper center (recessed area with plate)
    for (int dy = 0; dy < (int)(5 * sc); dy++) {
        int inset = (int)(8 * sc) + dy;
        render_line(car_cx - bmpw + inset, y_bot - dy, car_cx + bmpw - inset, y_bot - dy, dark_bumper);
    }

    // --- Exhaust tips (large round-ish, in bumper cutouts) ---
    int ex_r = (int)(4 * sc); if (ex_r < 2) ex_r = 2;
    int ex_lx = car_cx - bmpw + (int)(8 * sc);
    int ex_rx = car_cx + bmpw - (int)(8 * sc);
    int ex_y  = y_bot - (int)(3 * sc);
    for (int dy = -ex_r; dy <= ex_r; dy++) {
        int half = ex_r - (dy < 0 ? -dy : dy);
        render_line(ex_lx - half, ex_y + dy, ex_lx + half, ex_y + dy, exhaust_col);
        render_line(ex_rx - half, ex_y + dy, ex_rx + half, ex_y + dy, exhaust_col);
    }

    // --- Body/fender sides (widest part, vertical from bumper top to shoulder) ---
    render_line(car_cx - fw, y_bmp, car_cx - fw, y_fend, silver_hi);
    render_line(car_cx + fw, y_bmp, car_cx + fw, y_fend, silver_hi);
    // Bumper-to-body crease
    render_line(car_cx - fw, y_bmp, car_cx + fw, y_bmp, silver);

    // --- Fender to trunk taper (shoulder curves inward to trunk) ---
    render_line(car_cx - fw, y_fend, car_cx - tkw, y_trunk, silver_hi);
    render_line(car_cx + fw, y_fend, car_cx + tkw, y_trunk, silver_hi);

    // --- Trunk lid ---
    render_line(car_cx - tkw, y_trunk, car_cx + tkw, y_trunk, silver_hi);
    // Trunk surface detail
    render_line(car_cx - tkw + 3, (y_fend + y_trunk) / 2, car_cx + tkw - 3, (y_fend + y_trunk) / 2, silver);

    // --- C-pillars (body panels flanking the rear window) ---
    // Left C-pillar: from roof edge down to fender, outside the window
    render_line(car_cx - rfw, y_roof, car_cx - fw, y_fend, silver);
    render_line(car_cx - rfw - 1, y_roof, car_cx - fw, y_fend, silver_dark);
    // Right C-pillar
    render_line(car_cx + rfw, y_roof, car_cx + fw, y_fend, silver);
    render_line(car_cx + rfw + 1, y_roof, car_cx + fw, y_fend, silver_dark);

    // --- Rear window (gentle coupe slope, inside C-pillars) ---
    render_line(car_cx - tkw, y_trunk, car_cx - rfw, y_roof, win_col);
    render_line(car_cx + tkw, y_trunk, car_cx + rfw, y_roof, win_col);
    // Window fill (dark tinted glass)
    for (int dy = 1; dy < win_h - 1; dy++) {
        float t = (float)dy / (float)win_h;
        int xl = car_cx - tkw + (int)((tkw - rfw) * t);
        int xr = car_cx + tkw - (int)((tkw - rfw) * t);
        if (dy % 3 != 0)
            render_line(xl + 1, y_trunk + dy, xr - 1, y_trunk + dy, win_col);
    }
    // Roofline
    render_line(car_cx - rfw, y_roof, car_cx + rfw, y_roof, silver_hi);

    // --- TAIL LIGHTS — G37S LED ring circles (2 per side) ---
    // Each side has an outer (larger) and inner (smaller) LED ring
    int r_outer = (int)(7 * sc);  if (r_outer < 3) r_outer = 3;
    int r_inner = (int)(5 * sc);  if (r_inner < 2) r_inner = 2;
    int ring_w  = (int)(2 * sc);  if (ring_w < 1) ring_w = 1;  // ring thickness

    // Center positions of the 4 rings (2 per side)
    int mid_y = (y_bmp + y_fend) / 2;  // vertically centered in fender area
    // Left side: outer ring near fender edge, inner ring closer to trunk
    int l_out_x = car_cx - fw + r_outer + (int)(3 * sc);
    int l_in_x  = l_out_x + r_outer + r_inner + (int)(3 * sc);
    // Right side: mirrored
    int r_out_x = car_cx + fw - r_outer - (int)(3 * sc);
    int r_in_x  = r_out_x - r_outer - r_inner - (int)(3 * sc);

    // Draw LED rings as circle outlines (Bresenham-ish)
    // Helper: draw a ring at (cx, cy) with radius r and thickness t
    #define DRAW_RING(ring_cx, ring_cy, ring_r) \
        for (int ang = 0; ang < 64; ang++) { \
            float a = ang * 3.14159f * 2.0f / 64.0f; \
            float cs = cosf(a), sn = sinf(a); \
            for (int t = 0; t < ring_w; t++) { \
                float rr = (float)(ring_r - t); \
                int px = (ring_cx) + (int)(cs * rr); \
                int py = (ring_cy) + (int)(sn * rr); \
                if (px >= 0 && px < w && py >= 0 && py < h) \
                    fb[py * w + px] = tail_bright; \
            } \
        }

    DRAW_RING(l_out_x, mid_y, r_outer)
    DRAW_RING(l_in_x,  mid_y, r_inner)
    DRAW_RING(r_out_x, mid_y, r_outer)
    DRAW_RING(r_in_x,  mid_y, r_inner)

    #undef DRAW_RING
}
