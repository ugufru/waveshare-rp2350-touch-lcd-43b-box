#include "bsp_st7262.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"

#include "pio_rgb.h"

static bsp_display_interface_t *g_display_if;
static bsp_display_info_t *g_display_info;

static uint slice_num;
static uint pwm_channel;

static void bsp_lcd_brightness_init(void)
{
    float sys_clk = clock_get_hz(clk_sys);
    gpio_set_function(BSP_LCD_BL_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(BSP_LCD_BL_PIN);
    pwm_channel = pwm_gpio_to_channel(BSP_LCD_BL_PIN);
    pwm_set_clkdiv(slice_num, sys_clk / (PWM_FREQ * PWM_WRAP));
    pwm_set_wrap(slice_num, PWM_WRAP);
    pwm_set_chan_level(slice_num, pwm_channel, 0);
    pwm_set_enabled(slice_num, true);
}

static void bsp_lcd_set_brightness(uint8_t percent)
{
    if (percent > 100)
        percent = 100;

    if (percent == 0)
    {
        gpio_put(BSP_LCD_EN_PIN, 0);
        gpio_put(BSP_LCD_RST_PIN, 0);
    }
    else
    {
        gpio_put(BSP_LCD_EN_PIN, 1);
        gpio_put(BSP_LCD_RST_PIN, 1);
        pwm_set_chan_level(slice_num, pwm_channel, PWM_WRAP / 100 * (100 - percent));
    }

    g_display_info->brightness = percent;
}

static void bsp_st7262_reset(void)
{
    gpio_put(BSP_LCD_RST_PIN, 0);
    sleep_ms(20);
    gpio_put(BSP_LCD_RST_PIN, 1);
    sleep_ms(200);
}

static void bsp_st7262_init(void)
{
    gpio_init(BSP_LCD_RST_PIN);
    gpio_init(BSP_LCD_EN_PIN);
    gpio_set_dir(BSP_LCD_RST_PIN, GPIO_OUT);
    gpio_set_dir(BSP_LCD_EN_PIN, GPIO_OUT);
    gpio_put(BSP_LCD_EN_PIN, 1);
    bsp_st7262_reset();

    bsp_lcd_brightness_init();
    bsp_lcd_set_brightness(g_display_info->brightness);

    pio_rgb_pin_t pin;
    pio_rgb_info_t *rgb_info = (pio_rgb_info_t *)g_display_info->user_data;

    if (rgb_info->framebuffer1 == NULL)
    {
        printf("Error: Framebuffer1 is NULL\r\n");
        return;
    }

    for (size_t i = 0; i < rgb_info->width * rgb_info->height; i++)
        rgb_info->framebuffer1[i] = 0xffff;

    if (rgb_info->mode.double_buffer)
    {
        if (rgb_info->framebuffer2 == NULL)
        {
            printf("Error: Framebuffer2 is NULL\r\n");
            return;
        }
    }

    if (rgb_info->mode.enabled_psram)
    {
        if (rgb_info->transfer_buffer1 == NULL && rgb_info->transfer_buffer2 == NULL)
        {
            printf("Error: Transfer buffer1 or buffer2 is NULL\r\n");
            return;
        }
    }

    pin.data0_pin = BSP_LCD_DATA0_PIN;
    pin.de_pin = BSP_LCD_DE_PIN;
    pin.hsync_pin = BSP_LCD_HSYNC_PIN;
    pin.plck_pin = BSP_LCD_PLCK_PIN;
    pin.vsync_pin = BSP_LCD_VSYNC_PIN;

    pio_rgb_init(rgb_info, &pin);
}

void bsp_st7262_flush_dma(bsp_display_area_t *area, uint16_t *color_p)
{
    if (area == NULL && color_p == NULL)
    {
        pio_rgb_change_framebuffer();
    }
    else
    {
        pio_rgb_update_framebuffer(area->x1, area->y1, area->x2, area->y2, color_p);
    }
}

bool bsp_display_new_st7262(bsp_display_interface_t **interface, bsp_display_info_t *info)
{
    if (info == NULL)
        return false;

    static bsp_display_interface_t display_if;
    static bsp_display_info_t display_info;

    memcpy(&display_info, info, sizeof(bsp_display_info_t));

    display_if.init = bsp_st7262_init;
    display_if.reset = bsp_st7262_reset;
    display_if.set_brightness = bsp_lcd_set_brightness;
    display_if.flush_dma = bsp_st7262_flush_dma;

    *interface = &display_if;
    g_display_if = &display_if;
    g_display_info = &display_info;
    return true;
}
