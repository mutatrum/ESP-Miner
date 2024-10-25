#include <stdio.h>
#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "nvs_config.h"
#include "i2c_bitaxe.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_lcd_panel_ssd1306.h"

#define SSD1306_I2C_ADDRESS    0x3C

#define LCD_H_RES              128
#define LCD_V_RES              32
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

static bool is_display_active = false;

static const char * TAG = "display";

static lv_style_t style;
static lv_style_t chart_style;
extern const lv_font_t lv_font_portfolio_6x8;
extern const lv_img_dsc_t logo;

esp_err_t display_init(void)
{
    uint8_t flip_screen = nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, 1);
    uint8_t invert_screen = nvs_config_get_u16(NVS_CONFIG_INVERT_SCREEN, 0);

    i2c_master_bus_handle_t i2c_master_bus_handle;
    ESP_ERROR_CHECK(i2c_bitaxe_get_master_bus_handle(&i2c_master_bus_handle));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .scl_speed_hz = I2C_BUS_SPEED_HZ,
        .dev_addr = SSD1306_I2C_ADDRESS,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .dc_bit_offset = 6                     
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_master_bus_handle, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
        .color_space = ESP_LCD_COLOR_SPACE_MONOCHROME,
    };

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, invert_screen));
    
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = !flip_screen, // The screen is not flipped, this is for backwards compatibility
            .mirror_y = !flip_screen,
        },
    };

    lvgl_port_add_disp(&disp_cfg);

    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_portfolio_6x8);

    lv_style_init(&chart_style);
    lv_style_set_line_width(&chart_style, 1);

    is_display_active = true;

    return ESP_OK;
}

bool display_active(void)
{
    return is_display_active;
}

void display_clear()
{
    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_clean(scr);
        lvgl_port_unlock();
    }
}

void display_show_status(const char *messages[], size_t message_count)
{
    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_clean(scr);

        lv_obj_t *container = lv_obj_create(scr);
        lv_obj_set_size(container, LCD_H_RES, LCD_V_RES);
        lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);

        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        for (int i = 0; i < message_count; i++)
        {
            lv_obj_t *label = lv_label_create(container);
            lv_label_set_text(label, messages[i]);
            lv_obj_add_style(label, &style, LV_PART_MAIN);
            lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_width(label, LCD_H_RES);
        }

        lvgl_port_unlock();
    }
}

void display_show_logo()
{
    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_clean(scr);

        lv_obj_t *img = lv_img_create(scr);

        lv_img_set_src(img, &logo);

        lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

        lvgl_port_unlock();
    }
}

void display_show_graph(const double hashrate[], int source_length, int size, int index)
{
    if (size == 0) {
        display_show_status((const char *[]){"No hashrate data yet"}, 1);
        return;
    }

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_clean(scr);

        lv_obj_t *chart = lv_chart_create(scr);
        lv_obj_add_style(chart, &chart_style, LV_PART_ITEMS);

        lv_obj_set_size(chart, 100, 32);
        lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
        lv_chart_set_div_line_count(chart, 0, 0);
        lv_chart_set_point_count(chart, size);

        lv_chart_series_t *ser = lv_chart_add_series(chart, lv_color_black(), LV_CHART_AXIS_PRIMARY_Y);

        int min = INT_MAX;
        int max = INT_MIN;
        int sum = 0;
        for (int i = 0; i < size; i++) {

            int value = (int) hashrate[(source_length + index - size + i) % source_length];

            lv_chart_set_next_value(chart, ser, value);

            if (value < min) min = value;
            if (value > max) max = value;
            sum += value;
        }
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
        lv_chart_refresh(chart);

        lv_obj_align(chart, LV_ALIGN_LEFT_MID, 0, 0);

        char max_str[20];
        snprintf(max_str, sizeof(max_str), "%d", max);
        lv_obj_t *max_label = lv_label_create(scr);
        lv_label_set_text(max_label, max_str);
        lv_obj_set_style_text_align(max_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_add_style(max_label, &style, LV_PART_MAIN);
        lv_obj_set_width(max_label, LCD_H_RES);
        lv_obj_align(max_label, LV_ALIGN_TOP_RIGHT, 0, 0);

        char avg_str[20];
        snprintf(avg_str, sizeof(avg_str), "%d", sum / size);
        lv_obj_t *avg_label = lv_label_create(scr);
        lv_label_set_text(avg_label, avg_str);
        lv_obj_set_style_text_align(avg_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_add_style(avg_label, &style, LV_PART_MAIN);
        lv_obj_set_width(avg_label, LCD_H_RES);
        lv_obj_align(avg_label, LV_ALIGN_RIGHT_MID, 0, 0);

        char min_str[20];
        snprintf(min_str, sizeof(min_str), "%d", min);
        lv_obj_t *min_label = lv_label_create(scr);
        lv_label_set_text(min_label, min_str);
        lv_obj_set_style_text_align(min_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_add_style(min_label, &style, LV_PART_MAIN);
        lv_obj_set_width(min_label, LCD_H_RES);
        lv_obj_align(min_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

        lvgl_port_unlock();
    }
}
