#include <Arduino.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"

extern "C" {
#include "bsp_i2c.h"
#include "bsp_st7262.h"
#include "bsp_gt911.h"
#include "pio_rgb.h"
#include "rp_pico_alloc.h"
}

#include "demo/demo.h"
#include "demo/render.h"

#define LCD_WIDTH 800
#define LCD_HEIGHT 480
#define RENDER_W 400
#define RENDER_H 240

static bsp_touch_interface_t *g_touch_if = NULL;
static bool g_touch_ok = false;
static uint16_t *g_framebuffer = NULL;
static uint16_t *g_render_buf = NULL;  // Half-res SRAM render buffer

// Blit half-res buffer to full-res PSRAM framebuffer with 2x2 pixel doubling
// Uses 32-bit writes for speed (two pixels per write)
static void blit_2x(uint16_t *dst, const uint16_t *src, int sw, int sh, int dw) {
    for (int y = 0; y < sh; y++) {
        const uint16_t *srcRow = &src[y * sw];
        uint32_t *dstRow0 = (uint32_t *)&dst[(y * 2) * dw];
        uint32_t *dstRow1 = (uint32_t *)&dst[(y * 2 + 1) * dw];
        for (int x = 0; x < sw; x++) {
            uint32_t c = srcRow[x];
            uint32_t pair = c | (c << 16);  // two identical pixels
            dstRow0[x] = pair;
            dstRow1[x] = pair;
        }
    }
}

// Touch tracking for swipe gestures
static bool was_touching = false;
static int16_t touch_start_x = 0, touch_start_y = 0;
static uint32_t touch_start_ms = 0;
static bool swipe_handled = false;

#define SWIPE_THRESHOLD  60
#define TAP_MAX_MOVE     30
#define TAP_MAX_MS       300

static void set_cpu_clock(uint32_t freq_mhz)
{
    set_sys_clock_khz(freq_mhz * 1000, true);
    clock_configure(
        clk_peri,
        0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
        freq_mhz * 1000 * 1000,
        freq_mhz * 1000 * 1000);
}

void setup()
{
    set_cpu_clock(232);
    Serial.begin(115200);
    while (!Serial && millis() < 3000) delay(10);
    Serial.println("RP2350-Touch-LCD-4.3B Screensaver Demo");

    bsp_i2c_init();

    // Configure display with PSRAM framebuffer
    pio_rgb_info_t rgb_info;
    memset(&rgb_info, 0, sizeof(rgb_info));
    rgb_info.width = LCD_WIDTH;
    rgb_info.height = LCD_HEIGHT;
    rgb_info.transfer_size = LCD_WIDTH * 40;
    rgb_info.pclk_freq = BSP_LCD_PCLK_FREQ;
    rgb_info.mode.double_buffer = false;
    rgb_info.mode.enabled_transfer = true;
    rgb_info.mode.enabled_psram = true;
    rgb_info.framebuffer1 = (uint16_t *)rp_mem_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    rgb_info.framebuffer2 = NULL;
    rgb_info.transfer_buffer1 = (uint16_t *)malloc(rgb_info.transfer_size * sizeof(uint16_t));
    rgb_info.transfer_buffer2 = (uint16_t *)malloc(rgb_info.transfer_size * sizeof(uint16_t));
    rgb_info.dma_flush_done_cb = NULL;

    if (!rgb_info.framebuffer1) {
        Serial.println("ERROR: PSRAM framebuffer allocation failed!");
        while (1) delay(1000);
    }
    g_framebuffer = rgb_info.framebuffer1;

    // Pre-fill transfer buffers to black
    for (size_t i = 0; i < rgb_info.transfer_size; i++) {
        rgb_info.transfer_buffer1[i] = 0x0000;
        rgb_info.transfer_buffer2[i] = 0x0000;
    }

    // Init display
    bsp_display_interface_t *display_if;
    bsp_display_info_t display_info;
    memset(&display_info, 0, sizeof(display_info));
    display_info.width = LCD_WIDTH;
    display_info.height = LCD_HEIGHT;
    display_info.brightness = 100;
    display_info.dma_flush_done_cb = NULL;
    display_info.user_data = &rgb_info;

    bsp_display_new_st7262(&display_if, &display_info);
    display_if->init();
    Serial.println("Display OK");

    // Allocate half-res SRAM render buffer
    g_render_buf = (uint16_t *)malloc(RENDER_W * RENDER_H * sizeof(uint16_t));
    if (!g_render_buf) {
        Serial.println("ERROR: SRAM render buffer allocation failed!");
        while (1) delay(1000);
    }
    memset(g_render_buf, 0, RENDER_W * RENDER_H * sizeof(uint16_t));

    // Init demo system at half resolution
    demo_init(g_render_buf, RENDER_W, RENDER_H);
    Serial.println("Demo initialized");

    // Init touch
    bsp_touch_info_t touch_info;
    memset(&touch_info, 0, sizeof(touch_info));
    touch_info.width = LCD_WIDTH;
    touch_info.height = LCD_HEIGHT;

    bsp_touch_new_gt911(&g_touch_if, &touch_info);
    g_touch_if->init();
    g_touch_ok = true;
    Serial.println("Touch OK — swipe left/right to switch scenes");
}

void loop()
{
    bool touching = false;
    int16_t tx = 0, ty = 0;
    bool tap_event = false;

    // Read touch
    if (g_touch_ok && g_touch_if) {
        g_touch_if->read();
        bsp_touch_data_t data;
        if (g_touch_if->get_data(&data)) {
            touching = true;
            tx = (int16_t)data.coords[0].x;
            ty = (int16_t)data.coords[0].y;
        }
    }

    // Track touch start
    if (touching && !was_touching) {
        touch_start_x = tx;
        touch_start_y = ty;
        touch_start_ms = millis();
        swipe_handled = false;
    }

    // Detect swipe while touching
    if (touching && !swipe_handled) {
        int16_t dx = tx - touch_start_x;
        if (dx > SWIPE_THRESHOLD) {
            demo_next_scene();
            swipe_handled = true;
        } else if (dx < -SWIPE_THRESHOLD) {
            demo_prev_scene();
            swipe_handled = true;
        }
    }

    // Detect tap on release
    if (!touching && was_touching && !swipe_handled) {
        uint32_t dt = millis() - touch_start_ms;
        int16_t dx = tx - touch_start_x;
        int16_t dy = ty - touch_start_y;
        int move = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (dt < TAP_MAX_MS && move < TAP_MAX_MOVE) {
            tap_event = true;
        }
    }

    was_touching = touching;

    // Scale touch coords to render resolution
    int16_t rtx = tx / 2;
    int16_t rty = ty / 2;

    demo_touch_update(touching && !swipe_handled, rtx, rty, tap_event);
    demo_frame();

    // Blit half-res SRAM buffer to full-res PSRAM framebuffer
    blit_2x(g_framebuffer, g_render_buf, RENDER_W, RENDER_H, LCD_WIDTH);
}
