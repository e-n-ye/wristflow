/**
 * @file breathing_controls_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "breathing_controls_gen.h"
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

lv_obj_t * breathing_controls_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "breathing_controls_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 374);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * breathing_end = lv_button_create(lv_obj_0);
        lv_obj_set_name(breathing_end, "breathing_end");
        lv_obj_set_x(breathing_end, 44);
        lv_obj_set_y(breathing_end, 99);
        lv_obj_set_width(breathing_end, 118);
        lv_obj_set_height(breathing_end, 118);
        lv_obj_set_style_bg_color(breathing_end, lv_color_hex(0xff5025), 0);
        lv_obj_set_style_radius(breathing_end, 59, 0);
        lv_obj_set_style_shadow_width(breathing_end, 0, 0);
        lv_obj_set_style_pad_all(breathing_end, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(breathing_end);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_44, 0);

        lv_obj_t * breathing_restart = lv_button_create(lv_obj_0);
        lv_obj_set_name(breathing_restart, "breathing_restart");
        lv_obj_set_x(breathing_restart, 228);
        lv_obj_set_y(breathing_restart, 99);
        lv_obj_set_width(breathing_restart, 118);
        lv_obj_set_height(breathing_restart, 118);
        lv_obj_set_style_bg_color(breathing_restart, lv_color_hex(0x00cabf), 0);
        lv_obj_set_style_radius(breathing_restart, 59, 0);
        lv_obj_set_style_shadow_width(breathing_restart, 0, 0);
        lv_obj_set_style_pad_all(breathing_restart, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(breathing_restart);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_44, 0);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_2, 36);
        lv_obj_set_y(lv_label_2, 241);
        lv_obj_set_width(lv_label_2, 134);
        lv_label_set_text(lv_label_2, "结束");
        lv_obj_set_style_text_font(lv_label_2, title_24, 0);
        lv_obj_set_style_text_align(lv_label_2, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_3, 213);
        lv_obj_set_y(lv_label_3, 241);
        lv_obj_set_width(lv_label_3, 148);
        lv_label_set_text(lv_label_3, "重新开始");
        lv_obj_set_style_text_font(lv_label_3, title_24, 0);
        lv_obj_set_style_text_align(lv_label_3, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 185);
        lv_obj_set_y(lv_obj_1, 346);
        lv_obj_set_width(lv_obj_1, 8);
        lv_obj_set_height(lv_obj_1, 8);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_1, lv_color_hex(0x164e80), 0);
        lv_obj_set_style_bg_opa(lv_obj_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_radius(lv_obj_1, 4, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 201);
        lv_obj_set_y(lv_obj_2, 346);
        lv_obj_set_width(lv_obj_2, 8);
        lv_obj_set_height(lv_obj_2, 8);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_2, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(lv_obj_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_2, 0, 0);
        lv_obj_set_style_radius(lv_obj_2, 4, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

