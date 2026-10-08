/**
 * @file screen_weather_current_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_weather_current_gen.h"
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

lv_obj_t * screen_weather_current_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_weather_current_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, WEATHER_CLOUDY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * weather_city = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_city, "weather_city");
        lv_obj_set_x(weather_city, 0);
        lv_obj_set_y(weather_city, 42);
        lv_obj_set_width(weather_city, 390);
        lv_obj_set_height(weather_city, 34);
        lv_label_set_text(weather_city, "乐清市");
        lv_obj_set_style_text_font(weather_city, notification_22, 0);
        lv_obj_set_style_text_color(weather_city, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_city, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_update = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_update, "weather_update");
        lv_obj_set_x(weather_update, 0);
        lv_obj_set_y(weather_update, 80);
        lv_obj_set_width(weather_update, 390);
        lv_obj_set_height(weather_update, 30);
        lv_label_set_text(weather_update, "刚刚更新");
        lv_obj_set_style_text_font(weather_update, notification_22, 0);
        lv_obj_set_style_text_color(weather_update, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_update, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_temp = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_temp, "weather_temp");
        lv_obj_set_x(weather_temp, 0);
        lv_obj_set_y(weather_temp, 132);
        lv_obj_set_width(weather_temp, 390);
        lv_obj_set_height(weather_temp, 72);
        lv_label_set_text(weather_temp, "30°");
        lv_obj_set_style_text_font(weather_temp, metric_56, 0);
        lv_obj_set_style_text_color(weather_temp, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_temp, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_condition = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_condition, "weather_condition");
        lv_obj_set_x(weather_condition, 0);
        lv_obj_set_y(weather_condition, 232);
        lv_obj_set_width(weather_condition, 390);
        lv_obj_set_height(weather_condition, 34);
        lv_label_set_text(weather_condition, "多云");
        lv_obj_set_style_text_font(weather_condition, notification_22, 0);
        lv_obj_set_style_text_color(weather_condition, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_condition, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_range = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_range, "weather_range");
        lv_obj_set_x(weather_range, 0);
        lv_obj_set_y(weather_range, 270);
        lv_obj_set_width(weather_range, 390);
        lv_obj_set_height(weather_range, 30);
        lv_label_set_text(weather_range, "31°/24°");
        lv_obj_set_style_text_font(weather_range, notification_22, 0);
        lv_obj_set_style_text_color(weather_range, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_range, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_aqi_caption = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_aqi_caption, "weather_aqi_caption");
        lv_obj_set_x(weather_aqi_caption, 0);
        lv_obj_set_y(weather_aqi_caption, 360);
        lv_obj_set_width(weather_aqi_caption, 390);
        lv_obj_set_height(weather_aqi_caption, 28);
        lv_label_set_text(weather_aqi_caption, "空气质量");
        lv_obj_set_style_text_font(weather_aqi_caption, notification_22, 0);
        lv_obj_set_style_text_color(weather_aqi_caption, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_aqi_caption, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_aqi = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_aqi, "weather_aqi");
        lv_obj_set_x(weather_aqi, 0);
        lv_obj_set_y(weather_aqi, 392);
        lv_obj_set_width(weather_aqi, 390);
        lv_obj_set_height(weather_aqi, 34);
        lv_label_set_text(weather_aqi, "优");
        lv_obj_set_style_text_font(weather_aqi, notification_22, 0);
        lv_obj_set_style_text_color(weather_aqi, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_aqi, LV_TEXT_ALIGN_CENTER, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
