/**
 * @file screen_weather_sun_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_weather_sun_gen.h"
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

lv_obj_t * screen_weather_sun_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_weather_sun_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, WEATHER_CLOUDY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * weather_sun_title = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_sun_title, "weather_sun_title");
        lv_obj_set_x(weather_sun_title, 24);
        lv_obj_set_y(weather_sun_title, 24);
        lv_obj_set_width(weather_sun_title, 220);
        lv_obj_set_height(weather_sun_title, 34);
        lv_label_set_text(weather_sun_title, "日升日落");
        lv_obj_set_style_text_font(weather_sun_title, notification_22, 0);
        lv_obj_set_style_text_color(weather_sun_title, FG_PRIMARY, 0);

        lv_obj_t * weather_sun_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_sun_clock, "weather_sun_clock");
        lv_obj_set_x(weather_sun_clock, 296);
        lv_obj_set_y(weather_sun_clock, 26);
        lv_obj_set_width(weather_sun_clock, 70);
        lv_obj_set_height(weather_sun_clock, 30);
        lv_label_set_text(weather_sun_clock, "15:00");
        lv_obj_set_style_text_font(weather_sun_clock, notification_22, 0);
        lv_obj_set_style_text_color(weather_sun_clock, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_sun_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * weather_sun_track_image = lv_image_create(lv_obj_0);
        lv_obj_set_name(weather_sun_track_image, "weather_sun_track_image");
        lv_obj_set_x(weather_sun_track_image, 27);
        lv_obj_set_y(weather_sun_track_image, 157);
        lv_obj_set_width(weather_sun_track_image, 336);
        lv_obj_set_height(weather_sun_track_image, 128);
        lv_image_set_src(weather_sun_track_image, weather_sun_track);
        lv_obj_set_flag(weather_sun_track_image, LV_OBJ_FLAG_CLICKABLE, false);

        lv_obj_t * weather_sun_position = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_sun_position, "weather_sun_position");
        lv_obj_set_x(weather_sun_position, 260);
        lv_obj_set_y(weather_sun_position, 187);
        lv_obj_set_width(weather_sun_position, 24);
        lv_obj_set_height(weather_sun_position, 24);
        lv_obj_set_flag(weather_sun_position, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_sun_position, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(weather_sun_position, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(weather_sun_position, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_sun_position, 0, 0);
        lv_obj_set_style_radius(weather_sun_position, 12, 0);

        lv_obj_t * weather_sunrise_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_sunrise_icon, "weather_sunrise_icon");
        lv_obj_set_x(weather_sunrise_icon, 48);
        lv_obj_set_y(weather_sunrise_icon, 292);
        lv_obj_set_width(weather_sunrise_icon, 40);
        lv_obj_set_height(weather_sunrise_icon, 34);
        lv_label_set_text(weather_sunrise_icon, "");
        lv_obj_set_style_text_font(weather_sunrise_icon, icons_20, 0);
        lv_obj_set_style_text_color(weather_sunrise_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_sunrise_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_sunrise = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_sunrise, "weather_sunrise");
        lv_obj_set_x(weather_sunrise, 100);
        lv_obj_set_y(weather_sunrise, 290);
        lv_obj_set_width(weather_sunrise, 180);
        lv_obj_set_height(weather_sunrise, 36);
        lv_label_set_text(weather_sunrise, "05:48");
        lv_obj_set_style_text_font(weather_sunrise, notification_22, 0);
        lv_obj_set_style_text_color(weather_sunrise, FG_PRIMARY, 0);

        lv_obj_t * weather_sunset_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_sunset_icon, "weather_sunset_icon");
        lv_obj_set_x(weather_sunset_icon, 48);
        lv_obj_set_y(weather_sunset_icon, 350);
        lv_obj_set_width(weather_sunset_icon, 40);
        lv_obj_set_height(weather_sunset_icon, 34);
        lv_label_set_text(weather_sunset_icon, "");
        lv_obj_set_style_text_font(weather_sunset_icon, icons_20, 0);
        lv_obj_set_style_text_color(weather_sunset_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_sunset_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_sunset = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_sunset, "weather_sunset");
        lv_obj_set_x(weather_sunset, 100);
        lv_obj_set_y(weather_sunset, 348);
        lv_obj_set_width(weather_sunset, 180);
        lv_obj_set_height(weather_sunset, 36);
        lv_label_set_text(weather_sunset, "17:44");
        lv_obj_set_style_text_font(weather_sunset, notification_22, 0);
        lv_obj_set_style_text_color(weather_sunset, FG_PRIMARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
