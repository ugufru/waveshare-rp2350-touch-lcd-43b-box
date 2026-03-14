// demo.h — Scene manager: cycles between 3D demo scenes with fade transitions

#ifndef DEMO_H
#define DEMO_H

#include <stdint.h>

// Touch state shared with scenes
struct TouchState {
    bool active;        // finger currently on screen
    int16_t x, y;       // current touch position
    bool tap;           // single tap detected this frame (one-shot)
};

// Global touch state — scenes read this directly
extern TouchState g_touch;

// Initialize demo system (call once after framebuffer allocation)
void demo_init(uint16_t *fb, int w, int h);

// Render one frame (call in main loop)
void demo_frame(void);

// Switch to next/previous scene immediately (e.g. from touch gesture)
void demo_next_scene(void);
void demo_prev_scene(void);

// Update touch state from main loop (call before demo_frame)
void demo_touch_update(bool active, int16_t x, int16_t y, bool tap);

#endif
