/**
 * @file screen_weather_host_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_weather_host_gen.h"
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

lv_obj_t * screen_weather_host_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_weather_host_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, WEATHER_CLOUDY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * weather_pager = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_pager, "weather_pager");
        lv_obj_set_x(weather_pager, 0);
        lv_obj_set_y(weather_pager, 0);
        lv_obj_set_width(weather_pager, 390);
        lv_obj_set_height(weather_pager, 450);
        lv_obj_set_flag(weather_pager, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_pager, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(weather_pager, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_pager, 0, 0);
        lv_obj_set_style_pad_all(weather_pager, 0, 0);
        lv_obj_set_style_radius(weather_pager, 0, 0);
        lv_obj_t * weather_page_current = lv_obj_create(weather_pager);
        lv_obj_set_name(weather_page_current, "weather_page_current");
        lv_obj_set_x(weather_page_current, 0);
        lv_obj_set_y(weather_page_current, 0);
        lv_obj_set_width(weather_page_current, 390);
        lv_obj_set_height(weather_page_current, 450);
        lv_obj_set_flag(weather_page_current, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_page_current, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(weather_page_current, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_page_current, 0, 0);
        lv_obj_set_style_pad_all(weather_page_current, 0, 0);
        lv_obj_set_style_radius(weather_page_current, 0, 0);

        lv_obj_t * weather_page_hourly = lv_obj_create(weather_pager);
        lv_obj_set_name(weather_page_hourly, "weather_page_hourly");
        lv_obj_set_x(weather_page_hourly, 0);
        lv_obj_set_y(weather_page_hourly, 450);
        lv_obj_set_width(weather_page_hourly, 390);
        lv_obj_set_height(weather_page_hourly, 450);
        lv_obj_set_flag(weather_page_hourly, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_page_hourly, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(weather_page_hourly, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_page_hourly, 0, 0);
        lv_obj_set_style_pad_all(weather_page_hourly, 0, 0);
        lv_obj_set_style_radius(weather_page_hourly, 0, 0);

        lv_obj_t * weather_page_daily = lv_obj_create(weather_pager);
        lv_obj_set_name(weather_page_daily, "weather_page_daily");
        lv_obj_set_x(weather_page_daily, 0);
        lv_obj_set_y(weather_page_daily, 900);
        lv_obj_set_width(weather_page_daily, 390);
        lv_obj_set_height(weather_page_daily, 450);
        lv_obj_set_flag(weather_page_daily, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_page_daily, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(weather_page_daily, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_page_daily, 0, 0);
        lv_obj_set_style_pad_all(weather_page_daily, 0, 0);
        lv_obj_set_style_radius(weather_page_daily, 0, 0);

        lv_obj_t * weather_page_indices = lv_obj_create(weather_pager);
        lv_obj_set_name(weather_page_indices, "weather_page_indices");
        lv_obj_set_x(weather_page_indices, 0);
        lv_obj_set_y(weather_page_indices, 1350);
        lv_obj_set_width(weather_page_indices, 390);
        lv_obj_set_height(weather_page_indices, 450);
        lv_obj_set_flag(weather_page_indices, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_page_indices, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(weather_page_indices, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_page_indices, 0, 0);
        lv_obj_set_style_pad_all(weather_page_indices, 0, 0);
        lv_obj_set_style_radius(weather_page_indices, 0, 0);

        lv_obj_t * weather_page_sun = lv_obj_create(weather_pager);
        lv_obj_set_name(weather_page_sun, "weather_page_sun");
        lv_obj_set_x(weather_page_sun, 0);
        lv_obj_set_y(weather_page_sun, 1800);
        lv_obj_set_width(weather_page_sun, 390);
        lv_obj_set_height(weather_page_sun, 450);
        lv_obj_set_flag(weather_page_sun, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_page_sun, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(weather_page_sun, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_page_sun, 0, 0);
        lv_obj_set_style_pad_all(weather_page_sun, 0, 0);
        lv_obj_set_style_radius(weather_page_sun, 0, 0);

        lv_obj_t * weather_state_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(weather_state_panel, "weather_state_panel");
        lv_obj_set_x(weather_state_panel, 0);
        lv_obj_set_y(weather_state_panel, 0);
        lv_obj_set_width(weather_state_panel, 390);
        lv_obj_set_height(weather_state_panel, 450);
        lv_obj_set_flag(weather_state_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(weather_state_panel, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(weather_state_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(weather_state_panel, 0, 0);
        lv_obj_set_style_pad_all(weather_state_panel, 0, 0);
        lv_obj_set_style_radius(weather_state_panel, 0, 0);
        lv_obj_t * weather_state_icon = lv_label_create(weather_state_panel);
        lv_obj_set_name(weather_state_icon, "weather_state_icon");
        lv_obj_set_x(weather_state_icon, 0);
        lv_obj_set_y(weather_state_icon, 148);
        lv_obj_set_width(weather_state_icon, 390);
        lv_obj_set_height(weather_state_icon, 50);
        lv_label_set_text(weather_state_icon, "");
        lv_obj_set_style_text_font(weather_state_icon, icons_44, 0);
        lv_obj_set_style_text_color(weather_state_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_state_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(weather_state_icon, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * weather_state_label = lv_label_create(weather_state_panel);
        lv_obj_set_name(weather_state_label, "weather_state_label");
        lv_obj_set_x(weather_state_label, 24);
        lv_obj_set_y(weather_state_label, 214);
        lv_obj_set_width(weather_state_label, 342);
        lv_obj_set_height(weather_state_label, 30);
        lv_label_set_text(weather_state_label, "正在同步天气");
        lv_obj_set_style_text_font(weather_state_label, notification_22, 0);
        lv_obj_set_style_text_color(weather_state_label, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_state_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(weather_state_label, LV_LABEL_LONG_MODE_CLIP);

        lv_obj_t * weather_retry = lv_button_create(weather_state_panel);
        lv_obj_set_name(weather_retry, "weather_retry");
        lv_obj_set_x(weather_retry, 134);
        lv_obj_set_y(weather_retry, 276);
        lv_obj_set_width(weather_retry, 122);
        lv_obj_set_height(weather_retry, 42);
        lv_obj_set_flag(weather_retry, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(weather_retry, (255 * 0 / 100), 0);
        lv_obj_set_style_border_color(weather_retry, FG_PRIMARY, 0);
        lv_obj_set_style_border_width(weather_retry, 1, 0);
        lv_obj_set_style_radius(weather_retry, 0, 0);
        lv_obj_set_style_shadow_width(weather_retry, 0, 0);
        lv_obj_set_style_pad_all(weather_retry, 0, 0);
        lv_obj_t * weather_retry_label = lv_label_create(weather_retry);
        lv_obj_set_name(weather_retry_label, "weather_retry_label");
        lv_obj_set_x(weather_retry_label, 0);
        lv_obj_set_y(weather_retry_label, 8);
        lv_obj_set_width(weather_retry_label, 122);
        lv_obj_set_height(weather_retry_label, 26);
        lv_label_set_text(weather_retry_label, "重试");
        lv_obj_set_style_text_font(weather_retry_label, notification_22, 0);
        lv_obj_set_style_text_color(weather_retry_label, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_retry_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(weather_retry_label, LV_LABEL_LONG_MODE_CLIP);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

