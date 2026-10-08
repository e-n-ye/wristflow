/**
 * @file screen_weather_indices_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_weather_indices_gen.h"
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

lv_obj_t * screen_weather_indices_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_weather_indices_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, WEATHER_CLOUDY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * weather_indices_title = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_indices_title, "weather_indices_title");
        lv_obj_set_x(weather_indices_title, 24);
        lv_obj_set_y(weather_indices_title, 24);
        lv_obj_set_width(weather_indices_title, 220);
        lv_obj_set_height(weather_indices_title, 34);
        lv_label_set_text(weather_indices_title, "天气指数");
        lv_obj_set_style_text_font(weather_indices_title, notification_22, 0);
        lv_obj_set_style_text_color(weather_indices_title, FG_PRIMARY, 0);

        lv_obj_t * weather_indices_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_indices_clock, "weather_indices_clock");
        lv_obj_set_x(weather_indices_clock, 296);
        lv_obj_set_y(weather_indices_clock, 26);
        lv_obj_set_width(weather_indices_clock, 70);
        lv_obj_set_height(weather_indices_clock, 30);
        lv_label_set_text(weather_indices_clock, "15:00");
        lv_obj_set_style_text_font(weather_indices_clock, notification_22, 0);
        lv_obj_set_style_text_color(weather_indices_clock, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_indices_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * weather_index_0_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_0_value, "weather_index_0_value");
        lv_obj_set_x(weather_index_0_value, 30);
        lv_obj_set_y(weather_index_0_value, 78);
        lv_obj_set_width(weather_index_0_value, 88);
        lv_obj_set_height(weather_index_0_value, 58);
        lv_label_set_text(weather_index_0_value, "41");
        lv_obj_set_style_text_font(weather_index_0_value, metric_56, 0);
        lv_obj_set_style_text_color(weather_index_0_value, FG_PRIMARY, 0);

        lv_obj_t * weather_index_0_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_0_icon, "weather_index_0_icon");
        lv_obj_set_x(weather_index_0_icon, 138);
        lv_obj_set_y(weather_index_0_icon, 90);
        lv_obj_set_width(weather_index_0_icon, 36);
        lv_obj_set_height(weather_index_0_icon, 36);
        lv_label_set_text(weather_index_0_icon, "");
        lv_obj_set_style_text_font(weather_index_0_icon, icons_20, 0);
        lv_obj_set_style_text_color(weather_index_0_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_index_0_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_index_0_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_0_label, "weather_index_0_label");
        lv_obj_set_x(weather_index_0_label, 184);
        lv_obj_set_y(weather_index_0_label, 94);
        lv_obj_set_width(weather_index_0_label, 170);
        lv_obj_set_height(weather_index_0_label, 30);
        lv_label_set_text(weather_index_0_label, "空气质量");
        lv_obj_set_style_text_font(weather_index_0_label, notification_22, 0);
        lv_obj_set_style_text_color(weather_index_0_label, FG_PRIMARY, 0);

        lv_obj_t * weather_index_1_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_1_value, "weather_index_1_value");
        lv_obj_set_x(weather_index_1_value, 30);
        lv_obj_set_y(weather_index_1_value, 162);
        lv_obj_set_width(weather_index_1_value, 88);
        lv_obj_set_height(weather_index_1_value, 58);
        lv_label_set_text(weather_index_1_value, "80");
        lv_obj_set_style_text_font(weather_index_1_value, metric_56, 0);
        lv_obj_set_style_text_color(weather_index_1_value, FG_PRIMARY, 0);

        lv_obj_t * weather_index_1_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_1_icon, "weather_index_1_icon");
        lv_obj_set_x(weather_index_1_icon, 138);
        lv_obj_set_y(weather_index_1_icon, 174);
        lv_obj_set_width(weather_index_1_icon, 36);
        lv_obj_set_height(weather_index_1_icon, 36);
        lv_label_set_text(weather_index_1_icon, "");
        lv_obj_set_style_text_font(weather_index_1_icon, icons_20, 0);
        lv_obj_set_style_text_color(weather_index_1_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_index_1_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_index_1_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_1_label, "weather_index_1_label");
        lv_obj_set_x(weather_index_1_label, 184);
        lv_obj_set_y(weather_index_1_label, 178);
        lv_obj_set_width(weather_index_1_label, 170);
        lv_obj_set_height(weather_index_1_label, 30);
        lv_label_set_text(weather_index_1_label, "相对湿度(%)");
        lv_obj_set_style_text_font(weather_index_1_label, notification_22, 0);
        lv_obj_set_style_text_color(weather_index_1_label, FG_PRIMARY, 0);

        lv_obj_t * weather_index_2_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_2_value, "weather_index_2_value");
        lv_obj_set_x(weather_index_2_value, 30);
        lv_obj_set_y(weather_index_2_value, 246);
        lv_obj_set_width(weather_index_2_value, 88);
        lv_obj_set_height(weather_index_2_value, 58);
        lv_label_set_text(weather_index_2_value, "3");
        lv_obj_set_style_text_font(weather_index_2_value, metric_56, 0);
        lv_obj_set_style_text_color(weather_index_2_value, FG_PRIMARY, 0);

        lv_obj_t * weather_index_2_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_2_icon, "weather_index_2_icon");
        lv_obj_set_x(weather_index_2_icon, 138);
        lv_obj_set_y(weather_index_2_icon, 258);
        lv_obj_set_width(weather_index_2_icon, 36);
        lv_obj_set_height(weather_index_2_icon, 36);
        lv_label_set_text(weather_index_2_icon, "");
        lv_obj_set_style_text_font(weather_index_2_icon, icons_20, 0);
        lv_obj_set_style_text_color(weather_index_2_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_index_2_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_index_2_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_2_label, "weather_index_2_label");
        lv_obj_set_x(weather_index_2_label, 184);
        lv_obj_set_y(weather_index_2_label, 262);
        lv_obj_set_width(weather_index_2_label, 170);
        lv_obj_set_height(weather_index_2_label, 30);
        lv_label_set_text(weather_index_2_label, "东南风");
        lv_obj_set_style_text_font(weather_index_2_label, notification_22, 0);
        lv_obj_set_style_text_color(weather_index_2_label, FG_PRIMARY, 0);

        lv_obj_t * weather_index_3_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_3_value, "weather_index_3_value");
        lv_obj_set_x(weather_index_3_value, 30);
        lv_obj_set_y(weather_index_3_value, 330);
        lv_obj_set_width(weather_index_3_value, 88);
        lv_obj_set_height(weather_index_3_value, 58);
        lv_label_set_text(weather_index_3_value, "4");
        lv_obj_set_style_text_font(weather_index_3_value, metric_56, 0);
        lv_obj_set_style_text_color(weather_index_3_value, FG_PRIMARY, 0);

        lv_obj_t * weather_index_3_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_3_icon, "weather_index_3_icon");
        lv_obj_set_x(weather_index_3_icon, 138);
        lv_obj_set_y(weather_index_3_icon, 342);
        lv_obj_set_width(weather_index_3_icon, 36);
        lv_obj_set_height(weather_index_3_icon, 36);
        lv_label_set_text(weather_index_3_icon, "");
        lv_obj_set_style_text_font(weather_index_3_icon, icons_20, 0);
        lv_obj_set_style_text_color(weather_index_3_icon, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(weather_index_3_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * weather_index_3_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(weather_index_3_label, "weather_index_3_label");
        lv_obj_set_x(weather_index_3_label, 184);
        lv_obj_set_y(weather_index_3_label, 346);
        lv_obj_set_width(weather_index_3_label, 170);
        lv_obj_set_height(weather_index_3_label, 30);
        lv_label_set_text(weather_index_3_label, "紫外线指数");
        lv_obj_set_style_text_font(weather_index_3_label, notification_22, 0);
        lv_obj_set_style_text_color(weather_index_3_label, FG_PRIMARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
