/**
 * @file breathing_home_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "breathing_home_gen.h"
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

lv_obj_t * breathing_home_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "breathing_home_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 374);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * breathing_home_ring = breathing_ring_create(lv_obj_0);
        lv_obj_set_name(breathing_home_ring, "breathing_home_ring");
        lv_obj_set_x(breathing_home_ring, 105);
        lv_obj_set_y(breathing_home_ring, 30);

        lv_obj_t * breathing_duration = lv_button_create(lv_obj_0);
        lv_obj_set_name(breathing_duration, "breathing_duration");
        lv_obj_set_x(breathing_duration, 95);
        lv_obj_set_y(breathing_duration, 231);
        lv_obj_set_width(breathing_duration, 200);
        lv_obj_set_height(breathing_duration, 52);
        lv_obj_set_style_bg_opa(breathing_duration, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(breathing_duration, 0, 0);
        lv_obj_set_style_shadow_width(breathing_duration, 0, 0);
        lv_obj_set_style_pad_all(breathing_duration, 0, 0);
        lv_obj_t * breathing_duration_text = lv_label_create(breathing_duration);
        lv_obj_set_name(breathing_duration_text, "breathing_duration_text");
        lv_obj_set_x(breathing_duration_text, 0);
        lv_obj_set_y(breathing_duration_text, 10);
        lv_obj_set_width(breathing_duration_text, 164);
        lv_label_set_text(breathing_duration_text, "1分钟");
        lv_obj_set_style_text_font(breathing_duration_text, value_36, 0);
        lv_obj_set_style_text_align(breathing_duration_text, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_label_0 = lv_label_create(breathing_duration);
        lv_obj_set_x(lv_label_0, 171);
        lv_obj_set_y(lv_label_0, 20);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);

        lv_obj_t * breathing_start = lv_button_create(lv_obj_0);
        lv_obj_set_name(breathing_start, "breathing_start");
        lv_obj_set_x(breathing_start, 30);
        lv_obj_set_y(breathing_start, 289);
        lv_obj_set_width(breathing_start, 330);
        lv_obj_set_height(breathing_start, 66);
        lv_obj_set_style_bg_color(breathing_start, lv_color_hex(0x252525), 0);
        lv_obj_set_style_radius(breathing_start, 33, 0);
        lv_obj_set_style_shadow_width(breathing_start, 0, 0);
        lv_obj_set_style_pad_all(breathing_start, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(breathing_start);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "开始");
        lv_obj_set_style_text_font(lv_label_1, value_36, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

