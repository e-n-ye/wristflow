/**
 * @file screen_watchface_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_watchface_gen.h"
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

lv_obj_t * screen_watchface_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_watchface_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, lv_color_hex(0xc0f4ff), 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);

        lv_obj_t * battery_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(battery_icon, "battery_icon");
        lv_obj_set_x(battery_icon, 151);
        lv_obj_set_y(battery_icon, 25);
        lv_label_set_text(battery_icon, "");
        lv_obj_set_style_text_font(battery_icon, icons_20, 0);

        lv_obj_t * battery_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(battery_label, "battery_label");
        lv_obj_set_x(battery_label, 181);
        lv_obj_set_y(battery_label, 20);
        lv_obj_set_width(battery_label, 65);
        lv_label_set_text(battery_label, "53%");

        lv_obj_t * hour_artwork = lv_image_create(lv_obj_0);
        lv_obj_set_name(hour_artwork, "hour_artwork");
        lv_obj_set_x(hour_artwork, 24);
        lv_obj_set_y(hour_artwork, 66);
        lv_image_set_src(hour_artwork, hour_seven);

        lv_obj_t * minute_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(minute_label, "minute_label");
        lv_obj_set_x(minute_label, 0);
        lv_obj_set_y(minute_label, 365);
        lv_obj_set_width(minute_label, 390);
        lv_label_set_text(minute_label, "42");
        lv_obj_set_style_text_align(minute_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(minute_label, metric_56, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

