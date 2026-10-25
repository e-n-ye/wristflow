/**
 * @file weather_hour_column_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "weather_hour_column_gen.h"
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

lv_obj_t * weather_hour_column_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "weather_hour_column_#");
        lv_obj_set_width(lv_obj_0, 92);
        lv_obj_set_height(lv_obj_0, 320);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * hour_time = lv_label_create(lv_obj_0);
        lv_obj_set_name(hour_time, "hour_time");
        lv_obj_set_x(hour_time, 0);
        lv_obj_set_y(hour_time, 18);
        lv_obj_set_width(hour_time, 92);
        lv_obj_set_height(hour_time, 30);
        lv_label_set_text(hour_time, "--:--");
        lv_obj_set_style_text_font(hour_time, body_20, 0);
        lv_obj_set_style_text_color(hour_time, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_time, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(hour_time, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * hour_temp = lv_label_create(lv_obj_0);
        lv_obj_set_name(hour_temp, "hour_temp");
        lv_obj_set_x(hour_temp, 0);
        lv_obj_set_y(hour_temp, 68);
        lv_obj_set_width(hour_temp, 92);
        lv_obj_set_height(hour_temp, 42);
        lv_label_set_text(hour_temp, "--");
        lv_obj_set_style_text_font(hour_temp, title_24, 0);
        lv_obj_set_style_text_color(hour_temp, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_temp, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(hour_temp, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * hour_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(hour_icon, "hour_icon");
        lv_obj_set_x(hour_icon, 0);
        lv_obj_set_y(hour_icon, 120);
        lv_obj_set_width(hour_icon, 92);
        lv_obj_set_height(hour_icon, 48);
        lv_label_set_text(hour_icon, "");
        lv_obj_set_style_text_font(hour_icon, icons_44, 0);
        lv_obj_set_style_text_color(hour_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(hour_icon, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * hour_wind_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(hour_wind_icon, "hour_wind_icon");
        lv_obj_set_x(hour_wind_icon, 32);
        lv_obj_set_y(hour_wind_icon, 180);
        lv_obj_set_width(hour_wind_icon, 28);
        lv_obj_set_height(hour_wind_icon, 34);
        lv_label_set_text(hour_wind_icon, "");
        lv_obj_set_style_text_font(hour_wind_icon, icons_20, 0);
        lv_obj_set_style_text_color(hour_wind_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_wind_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(hour_wind_icon, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * hour_wind = lv_label_create(lv_obj_0);
        lv_obj_set_name(hour_wind, "hour_wind");
        lv_obj_set_x(hour_wind, 0);
        lv_obj_set_y(hour_wind, 215);
        lv_obj_set_width(hour_wind, 92);
        lv_obj_set_height(hour_wind, 30);
        lv_label_set_text(hour_wind, "--");
        lv_obj_set_style_text_font(hour_wind, body_20, 0);
        lv_obj_set_style_text_color(hour_wind, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_wind, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(hour_wind, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * hour_air = lv_label_create(lv_obj_0);
        lv_obj_set_name(hour_air, "hour_air");
        lv_obj_set_x(hour_air, 0);
        lv_obj_set_y(hour_air, 252);
        lv_obj_set_width(hour_air, 92);
        lv_obj_set_height(hour_air, 30);
        lv_label_set_text(hour_air, "--");
        lv_obj_set_style_text_font(hour_air, notification_22, 0);
        lv_obj_set_style_text_color(hour_air, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_air, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(hour_air, LV_LABEL_LONG_MODE_CLIP);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

