/**
 * @file screen_weather_daily_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_weather_daily_gen.h"
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

lv_obj_t * screen_weather_daily_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_weather_daily_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, WEATHER_CLOUDY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * weather_daily_title = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_daily_title, "weather_daily_title");
        lv_obj_set_x(weather_daily_title, 24);
        lv_obj_set_y(weather_daily_title, 24);
        lv_obj_set_width(weather_daily_title, 220);
        lv_obj_set_height(weather_daily_title, 34);
        lv_label_set_text(weather_daily_title, "未来天气");
        lv_obj_set_style_text_font(weather_daily_title, notification_22, 0);
        lv_obj_set_style_text_color(weather_daily_title, FG_PRIMARY, 0);

        lv_obj_t * weather_daily_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_daily_clock, "weather_daily_clock");
        lv_obj_set_x(weather_daily_clock, 296);
        lv_obj_set_y(weather_daily_clock, 26);
        lv_obj_set_width(weather_daily_clock, 70);
        lv_obj_set_height(weather_daily_clock, 30);
        lv_label_set_text(weather_daily_clock, "15:00");
        lv_obj_set_style_text_font(weather_daily_clock, notification_22, 0);
        lv_obj_set_style_text_color(weather_daily_clock, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_daily_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * weather_daily_content = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_daily_content, "weather_daily_content");
        lv_obj_set_x(weather_daily_content, 0);
        lv_obj_set_y(weather_daily_content, 76);
        lv_obj_set_width(weather_daily_content, 390);
        lv_obj_set_height(weather_daily_content, 320);
        lv_obj_set_flag(weather_daily_content, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_daily_content, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_content, 0, 0);
        lv_obj_set_style_pad_all(weather_daily_content, 0, 0);
        lv_obj_set_style_radius(weather_daily_content, 0, 0);
        lv_obj_t * day_1_icon = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_1_icon, "day_1_icon");
        lv_obj_set_x(day_1_icon, 18);
        lv_obj_set_y(day_1_icon, 18);
        lv_obj_set_width(day_1_icon, 52);
        lv_obj_set_height(day_1_icon, 44);
        lv_label_set_text(day_1_icon, "");
        lv_obj_set_style_text_font(day_1_icon, icons_44, 0);
        lv_obj_set_style_text_color(day_1_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_1_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * day_1_name = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_1_name, "day_1_name");
        lv_obj_set_x(day_1_name, 88);
        lv_obj_set_y(day_1_name, 24);
        lv_obj_set_width(day_1_name, 104);
        lv_obj_set_height(day_1_name, 32);
        lv_label_set_text(day_1_name, "今天");
        lv_obj_set_style_text_font(day_1_name, notification_22, 0);
        lv_obj_set_style_text_color(day_1_name, FG_PRIMARY, 0);

        lv_obj_t * day_1_range = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_1_range, "day_1_range");
        lv_obj_set_x(day_1_range, 220);
        lv_obj_set_y(day_1_range, 24);
        lv_obj_set_width(day_1_range, 148);
        lv_obj_set_height(day_1_range, 32);
        lv_label_set_text(day_1_range, "31°/24°");
        lv_obj_set_style_text_font(day_1_range, title_24, 0);
        lv_obj_set_style_text_color(day_1_range, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_1_range, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * day_2_icon = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_2_icon, "day_2_icon");
        lv_obj_set_x(day_2_icon, 18);
        lv_obj_set_y(day_2_icon, 76);
        lv_obj_set_width(day_2_icon, 52);
        lv_obj_set_height(day_2_icon, 44);
        lv_label_set_text(day_2_icon, "");
        lv_obj_set_style_text_font(day_2_icon, icons_44, 0);
        lv_obj_set_style_text_color(day_2_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_2_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * day_2_name = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_2_name, "day_2_name");
        lv_obj_set_x(day_2_name, 88);
        lv_obj_set_y(day_2_name, 82);
        lv_obj_set_width(day_2_name, 104);
        lv_obj_set_height(day_2_name, 32);
        lv_label_set_text(day_2_name, "周二");
        lv_obj_set_style_text_font(day_2_name, notification_22, 0);
        lv_obj_set_style_text_color(day_2_name, FG_PRIMARY, 0);

        lv_obj_t * day_2_range = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_2_range, "day_2_range");
        lv_obj_set_x(day_2_range, 220);
        lv_obj_set_y(day_2_range, 82);
        lv_obj_set_width(day_2_range, 148);
        lv_obj_set_height(day_2_range, 32);
        lv_label_set_text(day_2_range, "30°/23°");
        lv_obj_set_style_text_font(day_2_range, title_24, 0);
        lv_obj_set_style_text_color(day_2_range, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_2_range, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * day_3_icon = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_3_icon, "day_3_icon");
        lv_obj_set_x(day_3_icon, 18);
        lv_obj_set_y(day_3_icon, 134);
        lv_obj_set_width(day_3_icon, 52);
        lv_obj_set_height(day_3_icon, 44);
        lv_label_set_text(day_3_icon, "");
        lv_obj_set_style_text_font(day_3_icon, icons_44, 0);
        lv_obj_set_style_text_color(day_3_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_3_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * day_3_name = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_3_name, "day_3_name");
        lv_obj_set_x(day_3_name, 88);
        lv_obj_set_y(day_3_name, 140);
        lv_obj_set_width(day_3_name, 104);
        lv_obj_set_height(day_3_name, 32);
        lv_label_set_text(day_3_name, "周三");
        lv_obj_set_style_text_font(day_3_name, notification_22, 0);
        lv_obj_set_style_text_color(day_3_name, FG_PRIMARY, 0);

        lv_obj_t * day_3_range = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_3_range, "day_3_range");
        lv_obj_set_x(day_3_range, 220);
        lv_obj_set_y(day_3_range, 140);
        lv_obj_set_width(day_3_range, 148);
        lv_obj_set_height(day_3_range, 32);
        lv_label_set_text(day_3_range, "29°/22°");
        lv_obj_set_style_text_font(day_3_range, title_24, 0);
        lv_obj_set_style_text_color(day_3_range, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_3_range, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * day_4_icon = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_4_icon, "day_4_icon");
        lv_obj_set_x(day_4_icon, 18);
        lv_obj_set_y(day_4_icon, 192);
        lv_obj_set_width(day_4_icon, 52);
        lv_obj_set_height(day_4_icon, 44);
        lv_label_set_text(day_4_icon, "");
        lv_obj_set_style_text_font(day_4_icon, icons_44, 0);
        lv_obj_set_style_text_color(day_4_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_4_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * day_4_name = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_4_name, "day_4_name");
        lv_obj_set_x(day_4_name, 88);
        lv_obj_set_y(day_4_name, 198);
        lv_obj_set_width(day_4_name, 104);
        lv_obj_set_height(day_4_name, 32);
        lv_label_set_text(day_4_name, "周四");
        lv_obj_set_style_text_font(day_4_name, notification_22, 0);
        lv_obj_set_style_text_color(day_4_name, FG_PRIMARY, 0);

        lv_obj_t * day_4_range = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_4_range, "day_4_range");
        lv_obj_set_x(day_4_range, 220);
        lv_obj_set_y(day_4_range, 198);
        lv_obj_set_width(day_4_range, 148);
        lv_obj_set_height(day_4_range, 32);
        lv_label_set_text(day_4_range, "28°/22°");
        lv_obj_set_style_text_font(day_4_range, title_24, 0);
        lv_obj_set_style_text_color(day_4_range, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_4_range, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * day_5_icon = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_5_icon, "day_5_icon");
        lv_obj_set_x(day_5_icon, 18);
        lv_obj_set_y(day_5_icon, 250);
        lv_obj_set_width(day_5_icon, 52);
        lv_obj_set_height(day_5_icon, 44);
        lv_label_set_text(day_5_icon, "");
        lv_obj_set_style_text_font(day_5_icon, icons_44, 0);
        lv_obj_set_style_text_color(day_5_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_5_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * day_5_name = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_5_name, "day_5_name");
        lv_obj_set_x(day_5_name, 88);
        lv_obj_set_y(day_5_name, 256);
        lv_obj_set_width(day_5_name, 104);
        lv_obj_set_height(day_5_name, 32);
        lv_label_set_text(day_5_name, "周五");
        lv_obj_set_style_text_font(day_5_name, notification_22, 0);
        lv_obj_set_style_text_color(day_5_name, FG_PRIMARY, 0);

        lv_obj_t * day_5_range = lv_label_create(weather_daily_content);
        lv_obj_set_name(day_5_range, "day_5_range");
        lv_obj_set_x(day_5_range, 220);
        lv_obj_set_y(day_5_range, 256);
        lv_obj_set_width(day_5_range, 148);
        lv_obj_set_height(day_5_range, 32);
        lv_label_set_text(day_5_range, "27°/21°");
        lv_obj_set_style_text_font(day_5_range, title_24, 0);
        lv_obj_set_style_text_color(day_5_range, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(day_5_range, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * weather_daily_dots = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_daily_dots, "weather_daily_dots");
        lv_obj_set_x(weather_daily_dots, 183);
        lv_obj_set_y(weather_daily_dots, 420);
        lv_obj_set_width(weather_daily_dots, 24);
        lv_obj_set_height(weather_daily_dots, 6);
        lv_obj_set_flag(weather_daily_dots, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_daily_dots, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_dots, 0, 0);
        lv_obj_set_style_pad_all(weather_daily_dots, 0, 0);
        lv_obj_set_style_radius(weather_daily_dots, 0, 0);
        lv_obj_t * weather_daily_dot_0 = lv_obj_create(weather_daily_dots);
        lv_obj_set_name(weather_daily_dot_0, "weather_daily_dot_0");
        lv_obj_set_x(weather_daily_dot_0, 0);
        lv_obj_set_y(weather_daily_dot_0, 0);
        lv_obj_set_width(weather_daily_dot_0, 6);
        lv_obj_set_height(weather_daily_dot_0, 6);
        lv_obj_set_flag(weather_daily_dot_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_daily_dot_0, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(weather_daily_dot_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_dot_0, 0, 0);
        lv_obj_set_style_radius(weather_daily_dot_0, 3, 0);

        lv_obj_t * weather_daily_dot_1 = lv_obj_create(weather_daily_dots);
        lv_obj_set_name(weather_daily_dot_1, "weather_daily_dot_1");
        lv_obj_set_x(weather_daily_dot_1, 18);
        lv_obj_set_y(weather_daily_dot_1, 0);
        lv_obj_set_width(weather_daily_dot_1, 6);
        lv_obj_set_height(weather_daily_dot_1, 6);
        lv_obj_set_flag(weather_daily_dot_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_daily_dot_1, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_daily_dot_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_dot_1, 0, 0);
        lv_obj_set_style_radius(weather_daily_dot_1, 3, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

