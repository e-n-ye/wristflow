/**
 * @file screen_stopwatch_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_stopwatch_gen.h"
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

lv_obj_t * screen_stopwatch_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_stopwatch_#");
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

        app_header_create(lv_obj_0, "秒表");

        lv_obj_t * stopwatch_time = lv_label_create(lv_obj_0);
        lv_obj_set_name(stopwatch_time, "stopwatch_time");
        lv_obj_set_x(stopwatch_time, 24);
        lv_obj_set_y(stopwatch_time, 131);
        lv_obj_set_width(stopwatch_time, 342);
        lv_label_set_text(stopwatch_time, "00:00");
        lv_obj_set_style_text_font(stopwatch_time, metric_56, 0);
        lv_obj_set_style_text_align(stopwatch_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * stopwatch_fraction = lv_label_create(lv_obj_0);
        lv_obj_set_name(stopwatch_fraction, "stopwatch_fraction");
        lv_obj_set_x(stopwatch_fraction, 24);
        lv_obj_set_y(stopwatch_fraction, 211);
        lv_obj_set_width(stopwatch_fraction, 342);
        lv_label_set_text(stopwatch_fraction, ".00");
        lv_obj_set_style_text_font(stopwatch_fraction, value_36, 0);
        lv_obj_set_style_text_align(stopwatch_fraction, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(stopwatch_fraction, FG_SECONDARY, 0);

        lv_obj_t * stopwatch_reset = lv_button_create(lv_obj_0);
        lv_obj_set_name(stopwatch_reset, "stopwatch_reset");
        lv_obj_set_x(stopwatch_reset, 69);
        lv_obj_set_y(stopwatch_reset, 313);
        lv_obj_set_width(stopwatch_reset, 112);
        lv_obj_set_height(stopwatch_reset, 96);
        lv_obj_set_style_radius(stopwatch_reset, 48, 0);
        lv_obj_set_style_bg_color(stopwatch_reset, BG_SURFACE, 0);
        lv_obj_set_style_shadow_width(stopwatch_reset, 0, 0);
        lv_obj_set_style_pad_all(stopwatch_reset, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(stopwatch_reset);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_44, 0);

        lv_obj_t * stopwatch_toggle = lv_button_create(lv_obj_0);
        lv_obj_set_name(stopwatch_toggle, "stopwatch_toggle");
        lv_obj_set_x(stopwatch_toggle, 209);
        lv_obj_set_y(stopwatch_toggle, 313);
        lv_obj_set_width(stopwatch_toggle, 112);
        lv_obj_set_height(stopwatch_toggle, 96);
        lv_obj_set_style_radius(stopwatch_toggle, 48, 0);
        lv_obj_set_style_bg_color(stopwatch_toggle, lv_color_hex(0x31d49a), 0);
        lv_obj_set_style_shadow_width(stopwatch_toggle, 0, 0);
        lv_obj_set_style_pad_all(stopwatch_toggle, 0, 0);
        lv_obj_set_style_text_color(stopwatch_toggle, lv_color_hex(0x05271c), 0);
        lv_obj_t * stopwatch_toggle_icon = lv_label_create(stopwatch_toggle);
        lv_obj_set_name(stopwatch_toggle_icon, "stopwatch_toggle_icon");
        lv_obj_set_align(stopwatch_toggle_icon, LV_ALIGN_CENTER);
        lv_label_set_text(stopwatch_toggle_icon, "");
        lv_obj_set_style_text_font(stopwatch_toggle_icon, icons_44, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

