/**
 * @file breathing_result_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "breathing_result_gen.h"
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

lv_obj_t * breathing_result_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "breathing_result_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 374);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 30);
        lv_obj_set_y(lv_label_0, 14);
        lv_label_set_text(lv_label_0, "训练前心率");
        lv_obj_set_style_text_font(lv_label_0, title_24, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_SECONDARY, 0);

        lv_obj_t * breathing_before = lv_label_create(lv_obj_0);
        lv_obj_set_name(breathing_before, "breathing_before");
        lv_obj_set_x(breathing_before, 171);
        lv_obj_set_y(breathing_before, 14);
        lv_label_set_text(breathing_before, "--次/分");
        lv_obj_set_style_text_font(breathing_before, title_24, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_1, 84);
        lv_obj_set_y(lv_label_1, 59);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_20, 0);
        lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0xf43f8d), 0);

        lv_obj_t * breathing_after = lv_label_create(lv_obj_0);
        lv_obj_set_name(breathing_after, "breathing_after");
        lv_obj_set_x(breathing_after, 30);
        lv_obj_set_y(breathing_after, 57);
        lv_label_set_text(breathing_after, "--");
        lv_obj_set_style_text_font(breathing_after, metric_56, 0);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_2, 91);
        lv_obj_set_y(lv_label_2, 88);
        lv_label_set_text(lv_label_2, "次/分");
        lv_obj_set_style_text_font(lv_label_2, title_24, 0);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_3, 30);
        lv_obj_set_y(lv_label_3, 155);
        lv_label_set_text(lv_label_3, "训练时长");
        lv_obj_set_style_text_font(lv_label_3, title_24, 0);
        lv_obj_set_style_text_color(lv_label_3, FG_SECONDARY, 0);

        lv_obj_t * breathing_result_minutes = lv_label_create(lv_obj_0);
        lv_obj_set_name(breathing_result_minutes, "breathing_result_minutes");
        lv_obj_set_x(breathing_result_minutes, 30);
        lv_obj_set_y(breathing_result_minutes, 192);
        lv_label_set_text(breathing_result_minutes, "1");
        lv_obj_set_style_text_font(breathing_result_minutes, metric_56, 0);

        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_4, 74);
        lv_obj_set_y(lv_label_4, 222);
        lv_label_set_text(lv_label_4, "分钟");
        lv_obj_set_style_text_font(lv_label_4, title_24, 0);

        lv_obj_t * breathing_done = lv_button_create(lv_obj_0);
        lv_obj_set_name(breathing_done, "breathing_done");
        lv_obj_set_x(breathing_done, 30);
        lv_obj_set_y(breathing_done, 289);
        lv_obj_set_width(breathing_done, 330);
        lv_obj_set_height(breathing_done, 66);
        lv_obj_set_style_bg_color(breathing_done, lv_color_hex(0x252525), 0);
        lv_obj_set_style_radius(breathing_done, 33, 0);
        lv_obj_set_style_shadow_width(breathing_done, 0, 0);
        lv_obj_set_style_pad_all(breathing_done, 0, 0);
        lv_obj_t * lv_label_5 = lv_label_create(breathing_done);
        lv_obj_set_align(lv_label_5, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_5, "完成");
        lv_obj_set_style_text_font(lv_label_5, value_36, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

