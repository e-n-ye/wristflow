/**
 * @file screen_settings_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_settings_gen.h"
#include "../wristflow_ui.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * screen_settings_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_settings_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        app_header_create(lv_obj_0, "设置");

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 30);
        lv_obj_set_y(lv_label_0, 100);
        lv_label_set_text(lv_label_0, "亮度");
        lv_obj_set_style_text_font(lv_label_0, title_24, 0);

        lv_obj_t * settings_brightness_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(settings_brightness_value, "settings_brightness_value");
        lv_obj_set_x(settings_brightness_value, 244);
        lv_obj_set_y(settings_brightness_value, 95);
        lv_obj_set_width(settings_brightness_value, 116);
        lv_label_set_text(settings_brightness_value, "60%");
        lv_obj_set_style_text_font(settings_brightness_value, value_36, 0);
        lv_obj_set_style_text_align(settings_brightness_value, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * settings_brightness = lv_slider_create(lv_obj_0);
        lv_obj_set_name(settings_brightness, "settings_brightness");
        lv_obj_set_x(settings_brightness, 42);
        lv_obj_set_y(settings_brightness, 181);
        lv_obj_set_width(settings_brightness, 306);
        lv_obj_set_height(settings_brightness, 20);
        lv_slider_set_value(settings_brightness, 60, false);
        lv_slider_set_min_value(settings_brightness, 10);
        lv_slider_set_max_value(settings_brightness, 100);
        lv_obj_set_style_bg_color(settings_brightness, BG_SURFACE_SUBTLE, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_1, 30);
        lv_obj_set_y(lv_label_1, 270);
        lv_label_set_text(lv_label_1, "电池");
        lv_obj_set_style_text_font(lv_label_1, title_24, 0);

        lv_obj_t * settings_battery = lv_label_create(lv_obj_0);
        lv_obj_set_name(settings_battery, "settings_battery");
        lv_obj_set_x(settings_battery, 244);
        lv_obj_set_y(settings_battery, 264);
        lv_obj_set_width(settings_battery, 116);
        lv_label_set_text(settings_battery, "53%");
        lv_obj_set_style_text_font(settings_battery, value_36, 0);
        lv_obj_set_style_text_align(settings_battery, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * settings_layout = lv_button_create(lv_obj_0);
        lv_obj_set_name(settings_layout, "settings_layout");
        lv_obj_set_x(settings_layout, 24);
        lv_obj_set_y(settings_layout, 348);
        lv_obj_set_width(settings_layout, 342);
        lv_obj_set_height(settings_layout, 70);
        lv_obj_set_style_bg_color(settings_layout, BG_SURFACE, 0);
        lv_obj_set_style_border_width(settings_layout, 0, 0);
        lv_obj_set_style_radius(settings_layout, 20, 0);
        lv_obj_set_style_pad_all(settings_layout, 0, 0);
        lv_obj_set_style_shadow_width(settings_layout, 0, 0);
        lv_obj_t * lv_label_2 = lv_label_create(settings_layout);
        lv_obj_set_x(lv_label_2, 18);
        lv_obj_set_align(lv_label_2, LV_ALIGN_LEFT_MID);
        lv_label_set_text(lv_label_2, "应用布局");
        lv_obj_set_style_text_font(lv_label_2, title_24, 0);
        lv_obj_set_style_text_color(lv_label_2, FG_PRIMARY, 0);

        lv_obj_t * settings_layout_value = lv_label_create(settings_layout);
        lv_obj_set_name(settings_layout_value, "settings_layout_value");
        lv_obj_set_x(settings_layout_value, -18);
        lv_obj_set_align(settings_layout_value, LV_ALIGN_RIGHT_MID);
        lv_label_set_text(settings_layout_value, "列表");
        lv_obj_set_style_text_font(settings_layout_value, body_20, 0);
        lv_obj_set_style_text_color(settings_layout_value, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

