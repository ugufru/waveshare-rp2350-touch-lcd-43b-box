#include "bsp_gt911.h"
#include "bsp_i2c.h"

static bsp_touch_info_t *g_touch_info;
static bsp_touch_interface_t *g_touch_if;
static bool g_gt911_irq_flag = false;

bsp_touch_interface_t *bsp_gt911_get_touch_interface(void)
{
    return g_touch_if;
}

void bsp_gt911_reg_read_byte(uint16_t reg_addr, uint8_t *data, size_t len)
{
    bsp_i2c_read_reg16(GT911_DEVICE_ADDR, reg_addr, data, len);
}

void bsp_gt911_reg_write_byte(uint16_t reg_addr, uint8_t *data, size_t len)
{
    bsp_i2c_write_reg16(GT911_DEVICE_ADDR, reg_addr, data, len);
}

static void bsp_gt911_get_rotation(uint16_t *rotation)
{
    *rotation = g_touch_info->rotation;
}

static void bsp_gt911_set_rotation(uint16_t rotation)
{
    uint16_t swap;

    if (rotation == 1 || rotation == 3)
    {
        if (g_touch_info->width < g_touch_info->height)
        {
            swap = g_touch_info->width;
            g_touch_info->width = g_touch_info->height;
            g_touch_info->height = swap;
        }
    }
    else
    {
        if (g_touch_info->width > g_touch_info->height)
        {
            swap = g_touch_info->width;
            g_touch_info->width = g_touch_info->height;
            g_touch_info->height = swap;
        }
    }
    g_touch_info->rotation = rotation;
}

static bool bsp_gt911_get_touch_data(bsp_touch_data_t *data)
{
    if (g_touch_info->data.points == 0 || data == NULL)
        return false;

    data->points = g_touch_info->data.points;
    for (int i = 0; i < data->points; i++)
    {
        switch (g_touch_info->rotation)
        {
        case 1:
            data->coords[i].x = g_touch_info->data.coords[i].y;
            data->coords[i].y = g_touch_info->height - 1 - g_touch_info->data.coords[i].x;
            break;
        case 2:
            data->coords[i].x = g_touch_info->width - 1 - g_touch_info->data.coords[i].x;
            data->coords[i].y = g_touch_info->height - 1 - g_touch_info->data.coords[i].y;
            break;
        case 3:
            data->coords[i].x = g_touch_info->width - g_touch_info->data.coords[i].y;
            data->coords[i].y = g_touch_info->data.coords[i].x;
            break;
        default:
            data->coords[i].x = g_touch_info->data.coords[i].x;
            data->coords[i].y = g_touch_info->data.coords[i].y;
            break;
        }
        data->coords[i].pressure = g_touch_info->data.coords[i].pressure;
    }
    return true;
}

void bsp_gt911_read(void)
{
    uint8_t buffer[41];
    uint8_t zero = 0x00;

    if (g_gt911_irq_flag == false)
    {
        g_touch_info->data.points = 0;
        return;
    }
    g_gt911_irq_flag = false;

    bsp_gt911_reg_read_byte(GT911_EREG_READ_XY, buffer, 1);
    bsp_gt911_reg_write_byte(GT911_EREG_READ_XY, &zero, 1);
    if ((buffer[0] & 0x80) == 0)
    {
        g_touch_info->data.points = 0;
        return;
    }
    g_touch_info->data.points = buffer[0] & 0x0f;
    if (g_touch_info->data.points > GT911_LCD_TOUCH_MAX_POINTS)
        g_touch_info->data.points = GT911_LCD_TOUCH_MAX_POINTS;

    bsp_gt911_reg_read_byte(GT911_EREG_READ_XY + 1, &buffer[1], 8 * g_touch_info->data.points);

    for (int i = 0; i < g_touch_info->data.points; i++)
    {
        g_touch_info->data.coords[i].x = ((uint16_t)buffer[(i * 8) + 3] << 8) + buffer[(i * 8) + 2];
        g_touch_info->data.coords[i].y = (((uint16_t)buffer[(i * 8) + 5] << 8) + buffer[(i * 8) + 4]);
    }
}

static uint8_t draw_data[41];
static uint16_t pixel_size = 4;
static int draw_x, draw_y;
static const uint16_t color_arr[6] = {0xf800, 0x07e0, 0x001f, 0xf80f, 0xf01f, 0xffff};

void __no_inline_not_in_flash_func(bsp_gt911_draw_point)(uint16_t *buffer)
{
    uint8_t zero = 0x00;

    if (g_gt911_irq_flag == false)
    {
        g_touch_info->data.points = 0;
        return;
    }
    g_gt911_irq_flag = false;

    bsp_gt911_reg_read_byte(GT911_EREG_READ_XY, draw_data, 1);
    bsp_gt911_reg_write_byte(GT911_EREG_READ_XY, &zero, 1);
    if ((draw_data[0] & 0x80) == 0)
    {
        g_touch_info->data.points = 0;
        return;
    }
    g_touch_info->data.points = draw_data[0] & 0x0f;
    if (g_touch_info->data.points > GT911_LCD_TOUCH_MAX_POINTS)
        g_touch_info->data.points = GT911_LCD_TOUCH_MAX_POINTS;

    bsp_gt911_reg_read_byte(GT911_EREG_READ_XY + 1, &draw_data[1], 8 * g_touch_info->data.points);

    if (g_touch_info->data.points > 1)
        pixel_size = 8;
    else
        pixel_size = 4;

    if (g_touch_info->data.points > 0)
    {
        for (size_t i = 0; i < g_touch_info->data.points; i++)
        {
            draw_x = ((uint16_t)draw_data[(i * 8) + 3] << 8) + draw_data[(i * 8) + 2];
            draw_y = (((uint16_t)draw_data[(i * 8) + 5] << 8) + draw_data[(i * 8) + 4]);
            printf("x[%d]: %d, y[%d]: %d\r\n", (int)i, draw_x, (int)i, draw_y);
            if (draw_x > 800 - pixel_size - 1)
                draw_x = 800 - pixel_size - 1;
            if (draw_y > 480 - pixel_size - 1)
                draw_y = 480 - pixel_size - 1;

            for (int w = 0; w < pixel_size; w++)
            {
                for (int h = 0; h < pixel_size; h++)
                {
                    buffer[800 * (draw_y + h) + draw_x + w] = color_arr[i];
                }
            }
        }
    }
}

static void touch_irq_callback(uint gpio, uint32_t event_mask)
{
    if (event_mask == GPIO_IRQ_EDGE_FALL)
    {
        g_gt911_irq_flag = true;
    }
}

void bsp_gt911_reset(void)
{
    gpio_put(BSP_GT911_RST_PIN, 1);
    sleep_ms(50);
    gpio_put(BSP_GT911_RST_PIN, 0);
    sleep_ms(50);
    gpio_put(BSP_GT911_RST_PIN, 1);
    sleep_ms(250);
}

void bsp_gt911_init(void)
{
    uint8_t data[4] = {0};
    gpio_init(BSP_GT911_RST_PIN);
    gpio_set_dir(BSP_GT911_RST_PIN, GPIO_OUT);

    gpio_init(BSP_GT911_INT_PIN);
    gpio_set_dir(BSP_GT911_INT_PIN, GPIO_OUT);
    gpio_put(BSP_GT911_INT_PIN, 0);
    bsp_gt911_reset();

    int retries = 10;
    bool found = false;
    while (retries-- > 0)
    {
        bsp_gt911_reg_read_byte(GT911_REG_PRODUCT_ID, data, 4);

        if (strstr((char *)data, "911"))
        {
            printf("GT911_ID:%s\r\n", data);
            found = true;
            break;
        }
        else
            printf("Not found GT911!! (retries left: %d)\r\n", retries);

        sleep_ms(500);
    }
    if (!found)
    {
        printf("WARNING: GT911 not detected, touch disabled\r\n");
        return;
    }

    gpio_set_dir(BSP_GT911_INT_PIN, GPIO_IN);
    gpio_pull_up(BSP_GT911_INT_PIN);
    gpio_set_irq_enabled_with_callback(BSP_GT911_INT_PIN, GPIO_IRQ_EDGE_FALL, true, touch_irq_callback);
}

bool bsp_touch_new_gt911(bsp_touch_interface_t **interface, bsp_touch_info_t *info)
{
    if (info == NULL)
        return false;

    static bsp_touch_interface_t touch_if;
    static bsp_touch_info_t touch_info;

    memcpy(&touch_info, info, sizeof(bsp_touch_info_t));

    touch_if.init = bsp_gt911_init;
    touch_if.reset = bsp_gt911_reset;
    touch_if.read = bsp_gt911_read;
    touch_if.draw_point = bsp_gt911_draw_point;
    touch_if.get_data = bsp_gt911_get_touch_data;
    touch_if.get_rotation = bsp_gt911_get_rotation;
    touch_if.set_rotation = bsp_gt911_set_rotation;

    g_touch_if = &touch_if;
    *interface = &touch_if;
    g_touch_info = &touch_info;
    return true;
}
