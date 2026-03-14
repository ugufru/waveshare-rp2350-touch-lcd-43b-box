#include "pio_rgb.h"
#include "pio_rgb.pio.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"

#define RGB_SYNC_PIO pio1
#define RGB_COLOR_DATA_PIO pio2
#define RGB_PIO_BASE_PIN 16

static uint vsync_sm;
static uint hsync_sm;

static uint rgb_de_sm;
static uint rgb_sm;

static int rgb_dma_chan;

static pio_rgb_info_t *g_pio_rgb_info;

uint16_t test_count = 0;
void __no_inline_not_in_flash_func(dma_complete_handler)(void)
{
    test_count = (test_count + 1) % g_pio_rgb_info->transfer_index_max;

    if (g_pio_rgb_info->mode.double_buffer)
    {
        if (g_pio_rgb_info->mode.enabled_transfer)
        {
            g_pio_rgb_info->transfer_index = (g_pio_rgb_info->transfer_index + 1) % g_pio_rgb_info->transfer_index_max;
            uint16_t *transfer_buffer_p = &g_pio_rgb_info->_framebuffer[g_pio_rgb_info->transfer_index * g_pio_rgb_info->transfer_size];

            if (g_pio_rgb_info->mode.enabled_psram)
            {
                uint16_t *dma_buffer = (g_pio_rgb_info->transfer_index % 2)
                                           ? g_pio_rgb_info->transfer_buffer1
                                           : g_pio_rgb_info->transfer_buffer2;
                uint16_t *cp_buffer = (g_pio_rgb_info->transfer_index % 2)
                                          ? g_pio_rgb_info->transfer_buffer2
                                          : g_pio_rgb_info->transfer_buffer1;
                dma_channel_set_read_addr(rgb_dma_chan, dma_buffer, true);
                memcpy(cp_buffer, transfer_buffer_p, g_pio_rgb_info->transfer_size * sizeof(uint16_t));
            }
            else
            {
                dma_channel_set_read_addr(rgb_dma_chan, transfer_buffer_p, true);
            }

            if (g_pio_rgb_info->change_framebuffer_flag && (g_pio_rgb_info->transfer_index == g_pio_rgb_info->transfer_index_max - 1))
            {
                g_pio_rgb_info->change_framebuffer_flag = false;
                g_pio_rgb_info->_framebuffer = (g_pio_rgb_info->_framebuffer == g_pio_rgb_info->framebuffer1)
                                                   ? g_pio_rgb_info->framebuffer2
                                                   : g_pio_rgb_info->framebuffer1;
                if (g_pio_rgb_info->dma_flush_done_cb)
                    g_pio_rgb_info->dma_flush_done_cb();
            }
        }
        else
        {
            if (g_pio_rgb_info->change_framebuffer_flag)
            {
                g_pio_rgb_info->change_framebuffer_flag = false;
                g_pio_rgb_info->_framebuffer = (g_pio_rgb_info->_framebuffer == g_pio_rgb_info->framebuffer1)
                                                   ? g_pio_rgb_info->framebuffer2
                                                   : g_pio_rgb_info->framebuffer1;
                if (g_pio_rgb_info->dma_flush_done_cb)
                    g_pio_rgb_info->dma_flush_done_cb();
            }
            dma_channel_set_read_addr(rgb_dma_chan, g_pio_rgb_info->_framebuffer, true);
        }
    }
    else
    {
        if (g_pio_rgb_info->mode.enabled_transfer)
        {
            if (g_pio_rgb_info->mode.enabled_psram)
            {
                g_pio_rgb_info->transfer_index = (g_pio_rgb_info->transfer_index + 1) % g_pio_rgb_info->transfer_index_max;
                uint16_t *transfer_buffer_p = &g_pio_rgb_info->_framebuffer[g_pio_rgb_info->transfer_index * g_pio_rgb_info->transfer_size];
                uint16_t *dma_buffer = (g_pio_rgb_info->transfer_index % 2)
                                           ? g_pio_rgb_info->transfer_buffer1
                                           : g_pio_rgb_info->transfer_buffer2;
                uint16_t *cp_buffer = (g_pio_rgb_info->transfer_index % 2)
                                          ? g_pio_rgb_info->transfer_buffer2
                                          : g_pio_rgb_info->transfer_buffer1;
                dma_channel_set_read_addr(rgb_dma_chan, dma_buffer, true);
                memcpy(cp_buffer, transfer_buffer_p, g_pio_rgb_info->transfer_size * sizeof(uint16_t));
            }
            else
            {
                uint16_t *transfer_buffer_p = &g_pio_rgb_info->_framebuffer[g_pio_rgb_info->transfer_index * g_pio_rgb_info->transfer_size];
                g_pio_rgb_info->transfer_index = (g_pio_rgb_info->transfer_index + 1) % g_pio_rgb_info->transfer_index_max;
                dma_channel_set_read_addr(rgb_dma_chan, transfer_buffer_p, true);
            }

            if ((g_pio_rgb_info->transfer_index == g_pio_rgb_info->transfer_index_max - 1) && g_pio_rgb_info->dma_flush_done_cb)
                g_pio_rgb_info->dma_flush_done_cb();
        }
        else
        {
            dma_channel_set_read_addr(rgb_dma_chan, g_pio_rgb_info->_framebuffer, true);
            if (g_pio_rgb_info->dma_flush_done_cb)
                g_pio_rgb_info->dma_flush_done_cb();
        }
    }
}

void pio_rgb_change_framebuffer(void)
{
    g_pio_rgb_info->change_framebuffer_flag = true;
}

uint16_t *pio_rgb_get_free_framebuffer(void)
{
    if (g_pio_rgb_info->mode.double_buffer)
        return (g_pio_rgb_info->_framebuffer == g_pio_rgb_info->framebuffer1) ? g_pio_rgb_info->framebuffer2 : g_pio_rgb_info->framebuffer1;
    return g_pio_rgb_info->_framebuffer;
}

void pio_rgb_update_framebuffer(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *color_p)
{
    size_t color_width = (x2 - x1 + 1);
    size_t color_height = (y2 - y1 + 1);
    for (size_t i = 0; i < color_height; i++)
    {
        for (size_t j = 0; j < color_width; j++)
        {
            g_pio_rgb_info->_framebuffer[(i + y1) * g_pio_rgb_info->width + (j + x1)] = color_p[i * color_width + j];
        }
    }
}

static inline void hsync_program_init(PIO pio, uint sm, uint offset, uint pin, float div)
{
    pio_sm_config c = hsync_program_get_default_config(offset);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_clkdiv(&c, div);
    pio_gpio_init(pio, pin);
    pio_gpio_init(pio, pin + 1);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 2, true);
    pio_sm_init(pio, sm, offset, &c);
}

static inline void vsync_program_init(PIO pio, uint sm, uint offset, uint pin, float div)
{
    pio_sm_config c = vsync_program_get_default_config(offset);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_clkdiv(&c, div);
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_init(pio, sm, offset, &c);
}

static inline void rgb_de_program_init(PIO pio, uint sm, uint offset, uint pin, float div)
{
    pio_sm_config c = rgb_de_program_get_default_config(offset);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_clkdiv(&c, div);
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_init(pio, sm, offset, &c);
}

static inline void rgb_program_init(PIO pio, uint sm, uint offset, uint pin, float div)
{
    pio_sm_config c = rgb_program_get_default_config(offset);
    sm_config_set_out_pins(&c, pin, 16);
    sm_config_set_clkdiv(&c, div);
    for (int i = 0; i < 16; i++)
    {
        pio_gpio_init(pio, pin + i);
        gpio_pull_up(pin + i);
    }
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 16, true);
    pio_sm_init(pio, sm, offset, &c);
}

void pio_rgb_dma_init(pio_rgb_info_t *info)
{
    dma_channel_config c0 = dma_channel_get_default_config(rgb_dma_chan);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_16);
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    channel_config_set_dreq(&c0, pio_get_dreq(RGB_COLOR_DATA_PIO, rgb_sm, true));

    uint transfer_count = (info->mode.enabled_transfer) ? info->transfer_size : (info->width * info->height);

    dma_channel_configure(
        rgb_dma_chan,
        &c0,
        &RGB_COLOR_DATA_PIO->txf[rgb_sm],
        NULL,
        transfer_count,
        false);
    bsp_dma_channel_irq_add(1, rgb_dma_chan, dma_complete_handler);
}

void pio_rgb_init(pio_rgb_info_t *info, pio_rgb_pin_t *pin)
{
    static pio_rgb_info_t pio_rgb_info;
    memcpy(&pio_rgb_info, info, sizeof(pio_rgb_info_t));
    g_pio_rgb_info = &pio_rgb_info;
    g_pio_rgb_info->change_framebuffer_flag = false;
    g_pio_rgb_info->_framebuffer = g_pio_rgb_info->framebuffer1;
    g_pio_rgb_info->transfer_index = 0;
    g_pio_rgb_info->transfer_index_max = g_pio_rgb_info->width * g_pio_rgb_info->height / g_pio_rgb_info->transfer_size;

    float sys_clk = clock_get_hz(clk_sys);
    float pio_freq = sys_clk / ((float)(info->pclk_freq * 2));

    pio_set_gpio_base(RGB_SYNC_PIO, RGB_PIO_BASE_PIN);
    pio_set_gpio_base(RGB_COLOR_DATA_PIO, RGB_PIO_BASE_PIN);

    rgb_dma_chan = dma_claim_unused_channel(true);

    hsync_sm = pio_claim_unused_sm(RGB_SYNC_PIO, true);
    vsync_sm = pio_claim_unused_sm(RGB_SYNC_PIO, true);
    rgb_de_sm = pio_claim_unused_sm(RGB_COLOR_DATA_PIO, true);
    rgb_sm = pio_claim_unused_sm(RGB_COLOR_DATA_PIO, true);

    uint hsync_offset = pio_add_program(RGB_SYNC_PIO, &hsync_program);
    uint vsync_offset = pio_add_program(RGB_SYNC_PIO, &vsync_program);
    uint rgb_de_offset = pio_add_program(RGB_COLOR_DATA_PIO, &rgb_de_program);
    uint rgb_offset = pio_add_program(RGB_COLOR_DATA_PIO, &rgb_program);

    hsync_program_init(RGB_SYNC_PIO, hsync_sm, hsync_offset, pin->hsync_pin, pio_freq);
    vsync_program_init(RGB_SYNC_PIO, vsync_sm, vsync_offset, pin->vsync_pin, 1.0f);
    rgb_de_program_init(RGB_COLOR_DATA_PIO, rgb_de_sm, rgb_de_offset, pin->de_pin, 1.0f);
    rgb_program_init(RGB_COLOR_DATA_PIO, rgb_sm, rgb_offset, pin->data0_pin, 1.0f);

    pio_rgb_dma_init(info);
    pio_sm_put_blocking(RGB_SYNC_PIO, hsync_sm, info->width - 1);
    pio_sm_put_blocking(RGB_SYNC_PIO, vsync_sm, info->height - 1);
    pio_sm_put_blocking(RGB_COLOR_DATA_PIO, rgb_de_sm, info->height - 1);
    pio_sm_put_blocking(RGB_COLOR_DATA_PIO, rgb_sm, info->width - 1);

    // Pre-fill first transfer buffer so display DMA has valid data from the start
    if (info->mode.enabled_transfer && info->mode.enabled_psram) {
        memcpy(info->transfer_buffer1, g_pio_rgb_info->_framebuffer,
               g_pio_rgb_info->transfer_size * sizeof(uint16_t));
    }

    // Start all PIO SMs, then kick off DMA. The PIO will stall on pull block
    // until DMA feeds data, keeping pixel stream aligned with the scan.
    pio_enable_sm_mask_in_sync(RGB_COLOR_DATA_PIO, ((1u << rgb_de_sm) | (1u << rgb_sm)));
    pio_enable_sm_mask_in_sync(RGB_SYNC_PIO, ((1u << hsync_sm) | (1u << vsync_sm)));

    dma_complete_handler();
}
