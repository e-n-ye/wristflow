/**
 * @file screen_brightness_adjust_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_brightness_adjust_gen.h"
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

lv_obj_t * screen_brightness_adjust_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t brightness_fill;
    static lv_style_t brightness_knob;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&brightness_fill);
        lv_style_init(&brightness_knob);

        lv_style_set_bg_color(&brightness_fill, ACCENT_BLUE);
        lv_style_set_radius(&brightness_fill, 32);
        lv_style_set_bg_opa(&brightness_knob, (255 * 0 / 100));
        lv_style_set_pad_all(&brightness_knob, 0);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_brightness_adjust_#");
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

        app_header_create(lv_obj_0, "自定义亮度");

        lv_obj_t * settings_brightness_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(settings_brightness_value, "settings_brightness_value");
        lv_obj_set_x(settings_brightness_value, 100);
        lv_obj_set_y(settings_brightness_value, 84);
        lv_obj_set_width(settings_brightness_value, 190);
        lv_label_set_text(settings_brightness_value, "60%");
        lv_obj_set_style_text_font(settings_brightness_value, value_36, 0);
        lv_obj_set_style_text_align(settings_brightness_value, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * settings_brightness = lv_slider_create(lv_obj_0);
        lv_obj_set_name(settings_brightness, "settings_brightness");
        lv_obj_set_x(settings_brightness, 163);
        lv_obj_set_y(settings_brightness, 152);
        lv_obj_set_width(settings_brightness, 64);
        lv_obj_set_height(settings_brightness, 248);
        lv_slider_set_min_value(settings_brightness, 10);
        lv_slider_set_max_value(settings_brightness, 100);
        lv_slider_set_value(settings_brightness, 60, false);
        lv_obj_set_style_radius(settings_brightness, 32, 0);
        lv_obj_set_style_bg_color(settings_brightness, BG_SURFACE_SUBTLE, 0);
        lv_obj_set_style_bg_opa(settings_brightness, (255 * 100 / 100), 0);
        lv_obj_add_style(settings_brightness, &brightness_fill, LV_PART_INDICATOR);
        lv_obj_add_style(settings_brightness, &brightness_knob, LV_PART_KNOB);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

