/**
 * @file weather_day_row_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "weather_day_row_gen.h"
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

lv_obj_t * weather_day_row_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "weather_day_row_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 58);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * day_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(day_icon, "day_icon");
        lv_obj_set_x(day_icon, 18);
        lv_obj_set_y(day_icon, 4);
        lv_obj_set_width(day_icon, 52);
        lv_obj_set_height(day_icon, 44);
        lv_label_set_text(day_icon, "");
        lv_obj_set_style_text_font(day_icon, icons_44, 0);
        lv_obj_set_style_text_color(day_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(day_icon, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * day_name = lv_label_create(lv_obj_0);
        lv_obj_set_name(day_name, "day_name");
        lv_obj_set_x(day_name, 88);
        lv_obj_set_y(day_name, 10);
        lv_obj_set_width(day_name, 104);
        lv_obj_set_height(day_name, 32);
        lv_label_set_text(day_name, "--");
        lv_obj_set_style_text_font(day_name, notification_22, 0);
        lv_obj_set_style_text_color(day_name, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_name, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(day_name, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * day_range = lv_label_create(lv_obj_0);
        lv_obj_set_name(day_range, "day_range");
        lv_obj_set_x(day_range, 220);
        lv_obj_set_y(day_range, 10);
        lv_obj_set_width(day_range, 148);
        lv_obj_set_height(day_range, 32);
        lv_label_set_text(day_range, "--/--");
        lv_obj_set_style_text_font(day_range, title_24, 0);
        lv_obj_set_style_text_color(day_range, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_range, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(day_range, LV_LABEL_LONG_MODE_CLIP);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

