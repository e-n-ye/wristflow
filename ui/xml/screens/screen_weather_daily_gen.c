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
        lv_label_set_text(weather_daily_clock, "--:--");
        lv_obj_set_style_text_font(weather_daily_clock, body_20, 0);
        lv_obj_set_style_text_color(weather_daily_clock, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_daily_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * weather_daily_pager = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_daily_pager, "weather_daily_pager");
        lv_obj_set_x(weather_daily_pager, 0);
        lv_obj_set_y(weather_daily_pager, 76);
        lv_obj_set_width(weather_daily_pager, 390);
        lv_obj_set_height(weather_daily_pager, 320);
        lv_obj_set_flag(weather_daily_pager, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_daily_pager, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_pager, 0, 0);
        lv_obj_set_style_pad_all(weather_daily_pager, 0, 0);
        lv_obj_set_style_radius(weather_daily_pager, 0, 0);
        lv_obj_t * forecast_page_0 = weather_day_page_create(weather_daily_pager, false);
        lv_obj_set_name(forecast_page_0, "forecast_page_0");
        lv_obj_set_x(forecast_page_0, 0);
        lv_obj_set_y(forecast_page_0, 0);

        lv_obj_t * forecast_page_1 = weather_day_page_create(weather_daily_pager, true);
        lv_obj_set_name(forecast_page_1, "forecast_page_1");
        lv_obj_set_x(forecast_page_1, 390);
        lv_obj_set_y(forecast_page_1, 0);

        lv_obj_t * weather_daily_dots = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_daily_dots, "weather_daily_dots");
        lv_obj_set_x(weather_daily_dots, 183);
        lv_obj_set_y(weather_daily_dots, 422);
        lv_obj_set_width(weather_daily_dots, 24);
        lv_obj_set_height(weather_daily_dots, 6);
        lv_obj_set_flag(weather_daily_dots, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_daily_dots, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_dots, 0, 0);
        lv_obj_set_style_pad_all(weather_daily_dots, 0, 0);
        lv_obj_set_style_radius(weather_daily_dots, 0, 0);
        lv_obj_t * weather_daily_dots_0 = lv_obj_create(weather_daily_dots);
        lv_obj_set_name(weather_daily_dots_0, "weather_daily_dots_0");
        lv_obj_set_x(weather_daily_dots_0, 0);
        lv_obj_set_y(weather_daily_dots_0, 0);
        lv_obj_set_width(weather_daily_dots_0, 6);
        lv_obj_set_height(weather_daily_dots_0, 6);
        lv_obj_set_flag(weather_daily_dots_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_daily_dots_0, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(weather_daily_dots_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_dots_0, 0, 0);
        lv_obj_set_style_radius(weather_daily_dots_0, 3, 0);

        lv_obj_t * weather_daily_dots_1 = lv_obj_create(weather_daily_dots);
        lv_obj_set_name(weather_daily_dots_1, "weather_daily_dots_1");
        lv_obj_set_x(weather_daily_dots_1, 18);
        lv_obj_set_y(weather_daily_dots_1, 0);
        lv_obj_set_width(weather_daily_dots_1, 6);
        lv_obj_set_height(weather_daily_dots_1, 6);
        lv_obj_set_flag(weather_daily_dots_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(weather_daily_dots_1, PAGER_INACTIVE, 0);
        lv_obj_set_style_bg_opa(weather_daily_dots_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(weather_daily_dots_1, 0, 0);
        lv_obj_set_style_radius(weather_daily_dots_1, 3, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

