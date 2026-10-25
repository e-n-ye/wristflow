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
        lv_label_set_text(weather_hourly_clock, "--:--");
        lv_obj_set_style_text_font(weather_hourly_clock, body_20, 0);
        lv_obj_set_style_text_color(weather_hourly_clock, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_hourly_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * weather_hourly_pager = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_hourly_pager, "weather_hourly_pager");
        lv_obj_set_x(weather_hourly_pager, 0);
        lv_obj_set_y(weather_hourly_pager, 76);
        lv_obj_set_width(weather_hourly_pager, 390);
        lv_obj_set_height(weather_hourly_pager, 320);
        lv_obj_set_flag(weather_hourly_pager, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_hourly_pager, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_pager, 0, 0);
        lv_obj_set_style_pad_all(weather_hourly_pager, 0, 0);
        lv_obj_set_style_radius(weather_hourly_pager, 0, 0);
        lv_obj_t * forecast_page_0 = weather_hour_page_create(weather_hourly_pager);
        lv_obj_set_name(forecast_page_0, "forecast_page_0");
        lv_obj_set_x(forecast_page_0, 0);
        lv_obj_set_y(forecast_page_0, 0);

        lv_obj_t * forecast_page_1 = weather_hour_page_create(weather_hourly_pager);
        lv_obj_set_name(forecast_page_1, "forecast_page_1");
        lv_obj_set_x(forecast_page_1, 390);
        lv_obj_set_y(forecast_page_1, 0);

        lv_obj_t * forecast_page_2 = weather_hour_page_create(weather_hourly_pager);
        lv_obj_set_name(forecast_page_2, "forecast_page_2");
        lv_obj_set_x(forecast_page_2, 780);
        lv_obj_set_y(forecast_page_2, 0);

        lv_obj_t * forecast_page_3 = weather_hour_page_create(weather_hourly_pager);
        lv_obj_set_name(forecast_page_3, "forecast_page_3");
        lv_obj_set_x(forecast_page_3, 1170);
        lv_obj_set_y(forecast_page_3, 0);

        lv_obj_t * forecast_page_4 = weather_hour_page_create(weather_hourly_pager);
        lv_obj_set_name(forecast_page_4, "forecast_page_4");
        lv_obj_set_x(forecast_page_4, 1560);
        lv_obj_set_y(forecast_page_4, 0);

        lv_obj_t * forecast_page_5 = weather_hour_page_create(weather_hourly_pager);
        lv_obj_set_name(forecast_page_5, "forecast_page_5");
        lv_obj_set_x(forecast_page_5, 1950);
        lv_obj_set_y(forecast_page_5, 0);

        lv_obj_t * weather_hourly_dots = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_hourly_dots, "weather_hourly_dots");
        lv_obj_set_x(weather_hourly_dots, 147);
        lv_obj_set_y(weather_hourly_dots, 422);
        lv_obj_set_width(weather_hourly_dots, 96);
        lv_obj_set_height(weather_hourly_dots, 6);
        lv_obj_set_flag(weather_hourly_dots, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_hourly_dots, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots, 0, 0);
        lv_obj_set_style_pad_all(weather_hourly_dots, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots, 0, 0);
        lv_obj_t * weather_hourly_dots_0 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dots_0, "weather_hourly_dots_0");
        lv_obj_set_x(weather_hourly_dots_0, 0);
        lv_obj_set_y(weather_hourly_dots_0, 0);
        lv_obj_set_width(weather_hourly_dots_0, 6);
        lv_obj_set_height(weather_hourly_dots_0, 6);
        lv_obj_set_flag(weather_hourly_dots_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dots_0, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dots_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots_0, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots_0, 3, 0);

        lv_obj_t * weather_hourly_dots_1 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dots_1, "weather_hourly_dots_1");
        lv_obj_set_x(weather_hourly_dots_1, 18);
        lv_obj_set_y(weather_hourly_dots_1, 0);
        lv_obj_set_width(weather_hourly_dots_1, 6);
        lv_obj_set_height(weather_hourly_dots_1, 6);
        lv_obj_set_flag(weather_hourly_dots_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dots_1, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dots_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots_1, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots_1, 3, 0);

        lv_obj_t * weather_hourly_dots_2 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dots_2, "weather_hourly_dots_2");
        lv_obj_set_x(weather_hourly_dots_2, 36);
        lv_obj_set_y(weather_hourly_dots_2, 0);
        lv_obj_set_width(weather_hourly_dots_2, 6);
        lv_obj_set_height(weather_hourly_dots_2, 6);
        lv_obj_set_flag(weather_hourly_dots_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dots_2, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dots_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots_2, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots_2, 3, 0);

        lv_obj_t * weather_hourly_dots_3 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dots_3, "weather_hourly_dots_3");
        lv_obj_set_x(weather_hourly_dots_3, 54);
        lv_obj_set_y(weather_hourly_dots_3, 0);
        lv_obj_set_width(weather_hourly_dots_3, 6);
        lv_obj_set_height(weather_hourly_dots_3, 6);
        lv_obj_set_flag(weather_hourly_dots_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dots_3, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dots_3, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots_3, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots_3, 3, 0);

        lv_obj_t * weather_hourly_dots_4 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dots_4, "weather_hourly_dots_4");
        lv_obj_set_x(weather_hourly_dots_4, 72);
        lv_obj_set_y(weather_hourly_dots_4, 0);
        lv_obj_set_width(weather_hourly_dots_4, 6);
        lv_obj_set_height(weather_hourly_dots_4, 6);
        lv_obj_set_flag(weather_hourly_dots_4, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dots_4, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dots_4, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots_4, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots_4, 3, 0);

        lv_obj_t * weather_hourly_dots_5 = lv_obj_create(weather_hourly_dots);
        lv_obj_set_name(weather_hourly_dots_5, "weather_hourly_dots_5");
        lv_obj_set_x(weather_hourly_dots_5, 90);
        lv_obj_set_y(weather_hourly_dots_5, 0);
        lv_obj_set_width(weather_hourly_dots_5, 6);
        lv_obj_set_height(weather_hourly_dots_5, 6);
        lv_obj_set_flag(weather_hourly_dots_5, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_hourly_dots_5, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_hourly_dots_5, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_hourly_dots_5, 0, 0);
        lv_obj_set_style_radius(weather_hourly_dots_5, 3, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

