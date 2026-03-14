#ifndef __BSP_ST7262_H__
#define __BSP_ST7262_H__

#include "pico/stdlib.h"
#include <stdio.h>

#include "bsp_display.h"

#define BSP_LCD_DE_PIN 20
#define BSP_LCD_VSYNC_PIN 21
#define BSP_LCD_HSYNC_PIN 22
#define BSP_LCD_PLCK_PIN 23
#define BSP_LCD_DATA0_PIN 24

#define BSP_LCD_RST_PIN 19
#define BSP_LCD_BL_PIN 40
#define BSP_LCD_EN_PIN 18

#define BSP_LCD_PCLK_FREQ   (16 * 1000 * 1000)

#define PWM_FREQ 5000
#define PWM_WRAP 1000

#ifdef __cplusplus
extern "C" {
#endif

bool bsp_display_new_st7262(bsp_display_interface_t **interface, bsp_display_info_t *info);

#ifdef __cplusplus
}
#endif

#endif
