// demo.cpp — Scene manager with auto-cycling and fade transitions

#include "demo.h"
#include "render.h"
#include "fix16.h"

// Global touch state
TouchState g_touch = {false, 0, 0, false};

void demo_touch_update(bool active, int16_t x, int16_t y, bool tap) {
    g_touch.active = active;
    g_touch.x = x;
    g_touch.y = y;
    g_touch.tap = tap;
}

// Scene function declarations (defined in scene_*.cpp)
extern void scene_ribbon_init(void);
extern void scene_ribbon_frame(void);
extern void scene_torus_init(void);
extern void scene_torus_frame(void);
extern void scene_plasma_init(void);
extern void scene_plasma_frame(void);
extern void scene_metaballs_init(void);
extern void scene_metaballs_frame(void);
extern void scene_particles_init(void);
extern void scene_particles_frame(void);
extern void scene_wormhole_init(void);
extern void scene_wormhole_frame(void);
extern void scene_fractal_init(void);
extern void scene_fractal_frame(void);
extern void scene_synthwave_init(void);
extern void scene_synthwave_frame(void);
extern void scene_swarm_init(void);
extern void scene_swarm_frame(void);
extern void scene_kaleidoscope_init(void);
extern void scene_kaleidoscope_frame(void);

enum Scene {
    SCENE_RIBBON = 0,
    SCENE_TORUS,
    SCENE_PLASMA,
    SCENE_METABALLS,
    SCENE_PARTICLES,
    SCENE_WORMHOLE,
    SCENE_FRACTAL,
    SCENE_SYNTHWAVE,
    SCENE_SWARM,
    SCENE_KALEIDOSCOPE,
    SCENE_COUNT
};

#define SCENE_DURATION  960   // frames per scene (~16s at 60fps)
#define FADE_FRAMES     4     // frames of fade transition

static Scene current_scene = SCENE_RIBBON;
static uint32_t scene_frame_count = 0;
static int transition_dir = 0;   // 0 = none, +1 = next, -1 = prev
static int fade_step = 0;

static void init_scene(Scene s) {
    switch (s) {
        case SCENE_RIBBON:    scene_ribbon_init();    break;
        case SCENE_TORUS:     scene_torus_init();     break;
        case SCENE_PLASMA:    scene_plasma_init();    break;
        case SCENE_METABALLS: scene_metaballs_init(); break;
        case SCENE_PARTICLES:    scene_particles_init();    break;
        case SCENE_WORMHOLE:     scene_wormhole_init();     break;
        case SCENE_FRACTAL:      scene_fractal_init();      break;
        case SCENE_SYNTHWAVE:    scene_synthwave_init();     break;
        case SCENE_SWARM:        scene_swarm_init();         break;
        case SCENE_KALEIDOSCOPE: scene_kaleidoscope_init();  break;
        default: break;
    }
    scene_frame_count = 0;
}

static void render_scene(Scene s) {
    switch (s) {
        case SCENE_RIBBON:       scene_ribbon_frame();       break;
        case SCENE_TORUS:        scene_torus_frame();        break;
        case SCENE_PLASMA:       scene_plasma_frame();       break;
        case SCENE_METABALLS:    scene_metaballs_frame();    break;
        case SCENE_PARTICLES:    scene_particles_frame();    break;
        case SCENE_WORMHOLE:     scene_wormhole_frame();     break;
        case SCENE_FRACTAL:      scene_fractal_frame();      break;
        case SCENE_SYNTHWAVE:    scene_synthwave_frame();    break;
        case SCENE_SWARM:        scene_swarm_frame();        break;
        case SCENE_KALEIDOSCOPE: scene_kaleidoscope_frame(); break;
        default: break;
    }
}

void demo_init(uint16_t *fb, int w, int h) {
    build_sin_lut();
    render_init(fb, w, h);
    current_scene = SCENE_RIBBON;
    transition_dir = 0;
    fade_step = 0;
    init_scene(current_scene);
}

void demo_next_scene(void) {
    if (!transition_dir) transition_dir = 1;
}

void demo_prev_scene(void) {
    if (!transition_dir) transition_dir = -1;
}

void demo_frame(void) {
    scene_frame_count++;

    // Reset auto-cycle timer while touching (keep current scene)
    if (g_touch.active) {
        scene_frame_count = 0;
    }

    // Auto-cycle check
    if (scene_frame_count >= SCENE_DURATION) {
        if (!transition_dir) transition_dir = 1;
    }

    // Handle fade-out transition
    if (transition_dir) {
        fade_step++;
        if (fade_step <= FADE_FRAMES) {
            // Fade out: darken existing frame
            uint8_t factor = (uint8_t)(256 * (FADE_FRAMES - fade_step) / FADE_FRAMES);
            render_fade(factor);
            return;
        }
        // Transition complete — switch scene in requested direction
        int next = (int)current_scene + transition_dir;
        if (next >= SCENE_COUNT) next = 0;
        if (next < 0) next = SCENE_COUNT - 1;
        current_scene = (Scene)next;
        init_scene(current_scene);
        transition_dir = 0;
        fade_step = 0;
    }

    // Render current scene
    // Particles/wormhole/swarm manage their own fade/clear for trail effects
    if (current_scene != SCENE_PARTICLES &&
        current_scene != SCENE_WORMHOLE &&
        current_scene != SCENE_SWARM) {
        render_clear();
    }
    render_scene(current_scene);
    render_flush();
}
