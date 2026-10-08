/**
 * @file screen_weather_hourly_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_weather_hourly_gen.h"
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

lv_obj_t * screen_weather_hourly_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_weather_hourly_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, WEATHER_CLOUDY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * weather_hourly_title = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_hourly_title, "weather_hourly_title");
        lv_obj_set_x(weather_hourly_title, 24);
        lv_obj_set_y(weather_hourly_title, 24);
        lv_obj_set_width(weather_hourly_title, 220);
        lv_obj_set_height(weather_hourly_title, 34);
        lv_label_set_text(weather_hourly_title, "天气预测");
        lv_obj_set_style_text_font(weather_hourly_title, notification_22, 0);
        lv_obj_set_style_text_color(weather_hourly_title, FG_PRIMARY, 0);

        lv_obj_t * weather_hourly_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_hourly_clock, "weather_hourly_clock");
        lv_obj_set_x(weather_hourly_clock, 296);
        lv_obj_set_y(weather_hourly_clock, 26);
        lv_obj_set_width(weather_hourly_clock, 70);
        lv_obj_set_height(weather_hourly_clock, 30);
        lv_label_set_text(weather_hourly_clock, "15:00");
        lv_obj_set_style_text_font(weather_hourly_clock, notification_22, 0);
        lv_obj_set_style_text_color(weather_hourly_clock, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_hourly_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * weather_hourly_content = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_hourly_content, "weather_hourly_content");
        lv_obj_set_x(weather_hourly_content, 0);
        lv_obj_set_y(weather_hourly_content, 76);
        lv_obj_set_width(weather_hourly_content, 390);
        lv_obj_set_height(weather_hourly_content, 320);
        lv_obj_set_flag(weather_hourly_content, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_hourly_content, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_content, 0, 0);
        lv_obj_set_style_pad_all(weather_hourly_content, 0, 0);
        lv_obj_set_style_radius(weather_hourly_content, 0, 0);
        lv_obj_t * hour_1_time = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_1_time, "hour_1_time");
        lv_obj_set_x(hour_1_time, 10);
        lv_obj_set_y(hour_1_time, 18);
        lv_obj_set_width(hour_1_time, 92);
        lv_obj_set_height(hour_1_time, 30);
        lv_label_set_text(hour_1_time, "15:00");
        lv_obj_set_style_text_font(hour_1_time, notification_22, 0);
        lv_obj_set_style_text_color(hour_1_time, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_1_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_1_temp = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_1_temp, "hour_1_temp");
        lv_obj_set_x(hour_1_temp, 10);
        lv_obj_set_y(hour_1_temp, 68);
        lv_obj_set_width(hour_1_temp, 92);
        lv_obj_set_height(hour_1_temp, 42);
        lv_label_set_text(hour_1_temp, "30°");
        lv_obj_set_style_text_font(hour_1_temp, title_24, 0);
        lv_obj_set_style_text_color(hour_1_temp, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_1_temp, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_1_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_1_icon, "hour_1_icon");
        lv_obj_set_x(hour_1_icon, 10);
        lv_obj_set_y(hour_1_icon, 120);
        lv_obj_set_width(hour_1_icon, 92);
        lv_obj_set_height(hour_1_icon, 48);
        lv_label_set_text(hour_1_icon, "");
        lv_obj_set_style_text_font(hour_1_icon, icons_44, 0);
        lv_obj_set_style_text_color(hour_1_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_1_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_1_wind_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_1_wind_icon, "hour_1_wind_icon");
        lv_obj_set_x(hour_1_wind_icon, 10);
        lv_obj_set_y(hour_1_wind_icon, 176);
        lv_obj_set_width(hour_1_wind_icon, 92);
        lv_obj_set_height(hour_1_wind_icon, 26);
        lv_label_set_text(hour_1_wind_icon, "");
        lv_obj_set_style_text_font(hour_1_wind_icon, icons_20, 0);
        lv_obj_set_style_text_color(hour_1_wind_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_1_wind_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_1_wind = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_1_wind, "hour_1_wind");
        lv_obj_set_x(hour_1_wind, 10);
        lv_obj_set_y(hour_1_wind, 204);
        lv_obj_set_width(hour_1_wind, 92);
        lv_obj_set_height(hour_1_wind, 30);
        lv_label_set_text(hour_1_wind, "东南风");
        lv_obj_set_style_text_font(hour_1_wind, notification_22, 0);
        lv_obj_set_style_text_color(hour_1_wind, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_1_wind, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_1_air = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_1_air, "hour_1_air");
        lv_obj_set_x(hour_1_air, 10);
        lv_obj_set_y(hour_1_air, 258);
        lv_obj_set_width(hour_1_air, 92);
        lv_obj_set_height(hour_1_air, 30);
        lv_label_set_text(hour_1_air, "优");
        lv_obj_set_style_text_font(hour_1_air, notification_22, 0);
        lv_obj_set_style_text_color(hour_1_air, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_1_air, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_2_time = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_2_time, "hour_2_time");
        lv_obj_set_x(hour_2_time, 106);
        lv_obj_set_y(hour_2_time, 18);
        lv_obj_set_width(hour_2_time, 92);
        lv_obj_set_height(hour_2_time, 30);
        lv_label_set_text(hour_2_time, "16:00");
        lv_obj_set_style_text_font(hour_2_time, notification_22, 0);
        lv_obj_set_style_text_color(hour_2_time, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_2_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_2_temp = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_2_temp, "hour_2_temp");
        lv_obj_set_x(hour_2_temp, 106);
        lv_obj_set_y(hour_2_temp, 68);
        lv_obj_set_width(hour_2_temp, 92);
        lv_obj_set_height(hour_2_temp, 42);
        lv_label_set_text(hour_2_temp, "29°");
        lv_obj_set_style_text_font(hour_2_temp, title_24, 0);
        lv_obj_set_style_text_color(hour_2_temp, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_2_temp, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_2_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_2_icon, "hour_2_icon");
        lv_obj_set_x(hour_2_icon, 106);
        lv_obj_set_y(hour_2_icon, 120);
        lv_obj_set_width(hour_2_icon, 92);
        lv_obj_set_height(hour_2_icon, 48);
        lv_label_set_text(hour_2_icon, "");
        lv_obj_set_style_text_font(hour_2_icon, icons_44, 0);
        lv_obj_set_style_text_color(hour_2_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_2_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_2_wind_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_2_wind_icon, "hour_2_wind_icon");
        lv_obj_set_x(hour_2_wind_icon, 106);
        lv_obj_set_y(hour_2_wind_icon, 176);
        lv_obj_set_width(hour_2_wind_icon, 92);
        lv_obj_set_height(hour_2_wind_icon, 26);
        lv_label_set_text(hour_2_wind_icon, "");
        lv_obj_set_style_text_font(hour_2_wind_icon, icons_20, 0);
        lv_obj_set_style_text_color(hour_2_wind_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_2_wind_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_2_wind = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_2_wind, "hour_2_wind");
        lv_obj_set_x(hour_2_wind, 106);
        lv_obj_set_y(hour_2_wind, 204);
        lv_obj_set_width(hour_2_wind, 92);
        lv_obj_set_height(hour_2_wind, 30);
        lv_label_set_text(hour_2_wind, "东南风");
        lv_obj_set_style_text_font(hour_2_wind, notification_22, 0);
        lv_obj_set_style_text_color(hour_2_wind, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_2_wind, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_2_air = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_2_air, "hour_2_air");
        lv_obj_set_x(hour_2_air, 106);
        lv_obj_set_y(hour_2_air, 258);
        lv_obj_set_width(hour_2_air, 92);
        lv_obj_set_height(hour_2_air, 30);
        lv_label_set_text(hour_2_air, "优");
        lv_obj_set_style_text_font(hour_2_air, notification_22, 0);
        lv_obj_set_style_text_color(hour_2_air, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_2_air, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_3_time = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_3_time, "hour_3_time");
        lv_obj_set_x(hour_3_time, 202);
        lv_obj_set_y(hour_3_time, 18);
        lv_obj_set_width(hour_3_time, 92);
        lv_obj_set_height(hour_3_time, 30);
        lv_label_set_text(hour_3_time, "17:00");
        lv_obj_set_style_text_font(hour_3_time, notification_22, 0);
        lv_obj_set_style_text_color(hour_3_time, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_3_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_3_temp = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_3_temp, "hour_3_temp");
        lv_obj_set_x(hour_3_temp, 202);
        lv_obj_set_y(hour_3_temp, 68);
        lv_obj_set_width(hour_3_temp, 92);
        lv_obj_set_height(hour_3_temp, 42);
        lv_label_set_text(hour_3_temp, "28°");
        lv_obj_set_style_text_font(hour_3_temp, title_24, 0);
        lv_obj_set_style_text_color(hour_3_temp, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_3_temp, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_3_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_3_icon, "hour_3_icon");
        lv_obj_set_x(hour_3_icon, 202);
        lv_obj_set_y(hour_3_icon, 120);
        lv_obj_set_width(hour_3_icon, 92);
        lv_obj_set_height(hour_3_icon, 48);
        lv_label_set_text(hour_3_icon, "");
        lv_obj_set_style_text_font(hour_3_icon, icons_44, 0);
        lv_obj_set_style_text_color(hour_3_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_3_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_3_wind_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_3_wind_icon, "hour_3_wind_icon");
        lv_obj_set_x(hour_3_wind_icon, 202);
        lv_obj_set_y(hour_3_wind_icon, 176);
        lv_obj_set_width(hour_3_wind_icon, 92);
        lv_obj_set_height(hour_3_wind_icon, 26);
        lv_label_set_text(hour_3_wind_icon, "");
        lv_obj_set_style_text_font(hour_3_wind_icon, icons_20, 0);
        lv_obj_set_style_text_color(hour_3_wind_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_3_wind_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_3_wind = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_3_wind, "hour_3_wind");
        lv_obj_set_x(hour_3_wind, 202);
        lv_obj_set_y(hour_3_wind, 204);
        lv_obj_set_width(hour_3_wind, 92);
        lv_obj_set_height(hour_3_wind, 30);
        lv_label_set_text(hour_3_wind, "东南风");
        lv_obj_set_style_text_font(hour_3_wind, notification_22, 0);
        lv_obj_set_style_text_color(hour_3_wind, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_3_wind, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_3_air = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_3_air, "hour_3_air");
        lv_obj_set_x(hour_3_air, 202);
        lv_obj_set_y(hour_3_air, 258);
        lv_obj_set_width(hour_3_air, 92);
        lv_obj_set_height(hour_3_air, 30);
        lv_label_set_text(hour_3_air, "优");
        lv_obj_set_style_text_font(hour_3_air, notification_22, 0);
        lv_obj_set_style_text_color(hour_3_air, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_3_air, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_4_time = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_4_time, "hour_4_time");
        lv_obj_set_x(hour_4_time, 298);
        lv_obj_set_y(hour_4_time, 18);
        lv_obj_set_width(hour_4_time, 82);
        lv_obj_set_height(hour_4_time, 30);
        lv_label_set_text(hour_4_time, "18:00");
        lv_obj_set_style_text_font(hour_4_time, notification_22, 0);
        lv_obj_set_style_text_color(hour_4_time, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_4_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_4_temp = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_4_temp, "hour_4_temp");
        lv_obj_set_x(hour_4_temp, 298);
        lv_obj_set_y(hour_4_temp, 68);
        lv_obj_set_width(hour_4_temp, 82);
        lv_obj_set_height(hour_4_temp, 42);
        lv_label_set_text(hour_4_temp, "27°");
        lv_obj_set_style_text_font(hour_4_temp, title_24, 0);
        lv_obj_set_style_text_color(hour_4_temp, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_4_temp, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_4_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_4_icon, "hour_4_icon");
        lv_obj_set_x(hour_4_icon, 298);
        lv_obj_set_y(hour_4_icon, 120);
        lv_obj_set_width(hour_4_icon, 82);
        lv_obj_set_height(hour_4_icon, 48);
        lv_label_set_text(hour_4_icon, "");
        lv_obj_set_style_text_font(hour_4_icon, icons_44, 0);
        lv_obj_set_style_text_color(hour_4_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_4_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_4_wind_icon = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_4_wind_icon, "hour_4_wind_icon");
        lv_obj_set_x(hour_4_wind_icon, 298);
        lv_obj_set_y(hour_4_wind_icon, 176);
        lv_obj_set_width(hour_4_wind_icon, 82);
        lv_obj_set_height(hour_4_wind_icon, 26);
        lv_label_set_text(hour_4_wind_icon, "");
        lv_obj_set_style_text_font(hour_4_wind_icon, icons_20, 0);
        lv_obj_set_style_text_color(hour_4_wind_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_4_wind_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_4_wind = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_4_wind, "hour_4_wind");
        lv_obj_set_x(hour_4_wind, 298);
        lv_obj_set_y(hour_4_wind, 204);
        lv_obj_set_width(hour_4_wind, 82);
        lv_obj_set_height(hour_4_wind, 30);
        lv_label_set_text(hour_4_wind, "东南风");
        lv_obj_set_style_text_font(hour_4_wind, notification_22, 0);
        lv_obj_set_style_text_color(hour_4_wind, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_4_wind, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * hour_4_air = lv_label_create(weather_hourly_content);
        lv_obj_set_name(hour_4_air, "hour_4_air");
        lv_obj_set_x(hour_4_air, 298);
        lv_obj_set_y(hour_4_air, 258);
        lv_obj_set_width(hour_4_air, 82);
        lv_obj_set_height(hour_4_air, 30);
        lv_label_set_text(hour_4_air, "优");
        lv_obj_set_style_text_font(hour_4_air, notification_22, 0);
        lv_obj_set_style_text_color(hour_4_air, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(hour_4_air, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_hourly_dots = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_hourly_dots, "weather_hourly_dots");
        lv_obj_set_x(weather_hourly_dots, 147);
        lv_obj_set_y(weather_hourly_dots, 420);
        lv_obj_set_width(weather_hourly_dots, 96);
        lv_obj_set_height(weather_hourly_dots, 6);
        lv_obj_set_flag(weather_hourly_dots, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_hourly_dots, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots, 0, 0);
        lv_obj_set_style_pad_all(weather_hourly_dots, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots, 0, 0);
        lv_obj_t * weather_hourly_dot_0 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dot_0, "weather_hourly_dot_0");
        lv_obj_set_x(weather_hourly_dot_0, 0);
        lv_obj_set_y(weather_hourly_dot_0, 0);
        lv_obj_set_width(weather_hourly_dot_0, 6);
        lv_obj_set_height(weather_hourly_dot_0, 6);
        lv_obj_set_flag(weather_hourly_dot_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dot_0, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dot_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dot_0, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dot_0, 3, 0);

        lv_obj_t * weather_hourly_dot_1 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dot_1, "weather_hourly_dot_1");
        lv_obj_set_x(weather_hourly_dot_1, 18);
        lv_obj_set_y(weather_hourly_dot_1, 0);
        lv_obj_set_width(weather_hourly_dot_1, 6);
        lv_obj_set_height(weather_hourly_dot_1, 6);
        lv_obj_set_flag(weather_hourly_dot_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dot_1, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dot_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dot_1, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dot_1, 3, 0);

        lv_obj_t * weather_hourly_dot_2 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dot_2, "weather_hourly_dot_2");
        lv_obj_set_x(weather_hourly_dot_2, 36);
        lv_obj_set_y(weather_hourly_dot_2, 0);
        lv_obj_set_width(weather_hourly_dot_2, 6);
        lv_obj_set_height(weather_hourly_dot_2, 6);
        lv_obj_set_flag(weather_hourly_dot_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dot_2, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dot_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dot_2, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dot_2, 3, 0);

        lv_obj_t * weather_hourly_dot_3 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dot_3, "weather_hourly_dot_3");
        lv_obj_set_x(weather_hourly_dot_3, 54);
        lv_obj_set_y(weather_hourly_dot_3, 0);
        lv_obj_set_width(weather_hourly_dot_3, 6);
        lv_obj_set_height(weather_hourly_dot_3, 6);
        lv_obj_set_flag(weather_hourly_dot_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dot_3, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dot_3, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dot_3, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dot_3, 3, 0);

        lv_obj_t * weather_hourly_dot_4 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dot_4, "weather_hourly_dot_4");
        lv_obj_set_x(weather_hourly_dot_4, 72);
        lv_obj_set_y(weather_hourly_dot_4, 0);
        lv_obj_set_width(weather_hourly_dot_4, 6);
        lv_obj_set_height(weather_hourly_dot_4, 6);
        lv_obj_set_flag(weather_hourly_dot_4, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dot_4, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dot_4, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dot_4, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dot_4, 3, 0);

        lv_obj_t * weather_hourly_dot_5 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dot_5, "weather_hourly_dot_5");
        lv_obj_set_x(weather_hourly_dot_5, 90);
        lv_obj_set_y(weather_hourly_dot_5, 0);
        lv_obj_set_width(weather_hourly_dot_5, 6);
        lv_obj_set_height(weather_hourly_dot_5, 6);
        lv_obj_set_flag(weather_hourly_dot_5, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dot_5, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dot_5, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dot_5, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dot_5, 3, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
