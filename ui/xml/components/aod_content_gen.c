/**
 * @file aod_content_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "aod_content_gen.h"
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

lv_obj_t * aod_content_create(lv_obj_t * parent, bool center_hidden, bool corner_hidden)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "aod_content_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);

        lv_obj_t * aod_center = lv_obj_create(lv_obj_0);
        lv_obj_set_name(aod_center, "aod_center");
        lv_obj_set_width(aod_center, 390);
        lv_obj_set_height(aod_center, 450);
        lv_obj_set_flag(aod_center, LV_OBJ_FLAG_HIDDEN, center_hidden);
        lv_obj_set_flag(aod_center, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(aod_center, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(aod_center, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(aod_center, 0, 0);
        lv_obj_set_style_pad_all(aod_center, 0, 0);
        lv_obj_set_style_radius(aod_center, 0, 0);
        lv_obj_set_style_text_color(aod_center, FG_PRIMARY, 0);
        lv_obj_t * aod_center_time = lv_label_create(aod_center);
        lv_obj_set_name(aod_center_time, "aod_center_time");
        lv_obj_set_x(aod_center_time, 20);
        lv_obj_set_y(aod_center_time, 156);
        lv_obj_set_width(aod_center_time, 350);
        lv_label_set_text(aod_center_time, "09:28");
        lv_obj_set_style_text_font(aod_center_time, clock_80, 0);
        lv_obj_set_style_text_align(aod_center_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * aod_center_date = lv_label_create(aod_center);
        lv_obj_set_name(aod_center_date, "aod_center_date");
        lv_obj_set_x(aod_center_date, 24);
        lv_obj_set_y(aod_center_date, 250);
        lv_obj_set_width(aod_center_date, 342);
        lv_label_set_text(aod_center_date, "5月18日 周一");
        lv_obj_set_style_text_align(aod_center_date, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * aod_corner = lv_obj_create(lv_obj_0);
        lv_obj_set_name(aod_corner, "aod_corner");
        lv_obj_set_width(aod_corner, 390);
        lv_obj_set_height(aod_corner, 450);
        lv_obj_set_flag(aod_corner, LV_OBJ_FLAG_HIDDEN, corner_hidden);
        lv_obj_set_flag(aod_corner, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(aod_corner, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(aod_corner, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(aod_corner, 0, 0);
        lv_obj_set_style_pad_all(aod_corner, 0, 0);
        lv_obj_set_style_radius(aod_corner, 0, 0);
        lv_obj_set_style_text_color(aod_corner, FG_PRIMARY, 0);
        lv_obj_t * aod_corner_time = lv_label_create(aod_corner);
        lv_obj_set_name(aod_corner_time, "aod_corner_time");
        lv_obj_set_x(aod_corner_time, 42);
        lv_obj_set_y(aod_corner_time, 66);
        lv_obj_set_width(aod_corner_time, 310);
        lv_label_set_text(aod_corner_time, "09:28");
        lv_obj_set_style_text_font(aod_corner_time, metric_56, 0);

        lv_obj_t * aod_corner_date = lv_label_create(aod_corner);
        lv_obj_set_name(aod_corner_date, "aod_corner_date");
        lv_obj_set_x(aod_corner_date, 44);
        lv_obj_set_y(aod_corner_date, 136);
        lv_obj_set_width(aod_corner_date, 190);
        lv_label_set_text(aod_corner_date, "5/18");
        lv_obj_set_style_text_font(aod_corner_date, metric_56, 0);

        lv_obj_t * aod_corner_weekday = lv_label_create(aod_corner);
        lv_obj_set_name(aod_corner_weekday, "aod_corner_weekday");
        lv_obj_set_x(aod_corner_weekday, 240);
        lv_obj_set_y(aod_corner_weekday, 160);
        lv_obj_set_width(aod_corner_weekday, 100);
        lv_label_set_text(aod_corner_weekday, "周一");

        lv_obj_t * aod_corner_battery = lv_label_create(aod_corner);
        lv_obj_set_name(aod_corner_battery, "aod_corner_battery");
        lv_obj_set_x(aod_corner_battery, 44);
        lv_obj_set_y(aod_corner_battery, 212);
        lv_obj_set_width(aod_corner_battery, 300);
        lv_label_set_text(aod_corner_battery, "~83%");
        lv_obj_set_style_text_font(aod_corner_battery, metric_56, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

