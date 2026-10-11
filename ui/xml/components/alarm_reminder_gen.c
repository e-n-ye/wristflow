/**
 * @file alarm_reminder_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "alarm_reminder_gen.h"
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

lv_obj_t * alarm_reminder_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "alarm_reminder_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 151);
        lv_obj_set_y(lv_label_0, 65);
        lv_obj_set_width(lv_label_0, 88);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_44, 0);
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_reminder_time = lv_label_create(lv_obj_0);
        lv_obj_set_name(alarm_reminder_time, "alarm_reminder_time");
        lv_obj_set_x(alarm_reminder_time, 24);
        lv_obj_set_y(alarm_reminder_time, 161);
        lv_obj_set_width(alarm_reminder_time, 342);
        lv_label_set_text(alarm_reminder_time, "07:00");
        lv_obj_set_style_text_font(alarm_reminder_time, metric_56, 0);
        lv_obj_set_style_text_align(alarm_reminder_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_1, 24);
        lv_obj_set_y(lv_label_1, 248);
        lv_obj_set_width(lv_label_1, 342);
        lv_label_set_text(lv_label_1, "闹钟");
        lv_obj_set_style_text_font(lv_label_1, title_24, 0);
        lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_reminder_close = lv_button_create(lv_obj_0);
        lv_obj_set_name(alarm_reminder_close, "alarm_reminder_close");
        lv_obj_set_x(alarm_reminder_close, 24);
        lv_obj_set_y(alarm_reminder_close, 330);
        lv_obj_set_width(alarm_reminder_close, 342);
        lv_obj_set_height(alarm_reminder_close, 66);
        lv_obj_set_style_bg_color(alarm_reminder_close, BG_SURFACE, 0);
        lv_obj_set_style_radius(alarm_reminder_close, 33, 0);
        lv_obj_set_style_shadow_width(alarm_reminder_close, 0, 0);
        lv_obj_set_style_pad_all(alarm_reminder_close, 0, 0);
        lv_obj_t * lv_label_2 = lv_label_create(alarm_reminder_close);
        lv_obj_set_align(lv_label_2, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_2, "关闭");
        lv_obj_set_style_text_font(lv_label_2, title_24, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

