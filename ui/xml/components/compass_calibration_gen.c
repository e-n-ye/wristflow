/**
 * @file compass_calibration_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "compass_calibration_gen.h"
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

lv_obj_t * compass_calibration_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "compass_calibration_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, lv_color_hex(0xb8b8b8), 0);
        lv_obj_set_style_text_font(lv_obj_0, title_24, 0);

        lv_obj_t * compass_gesture = lv_image_create(lv_obj_0);
        lv_obj_set_name(compass_gesture, "compass_gesture");
        lv_obj_set_x(compass_gesture, 55);
        lv_obj_set_y(compass_gesture, 124);
        lv_image_set_src(compass_gesture, compass_gesture_path);

        lv_obj_t * compass_calibration_text = lv_label_create(lv_obj_0);
        lv_obj_set_name(compass_calibration_text, "compass_calibration_text");
        lv_obj_set_x(compass_calibration_text, 35);
        lv_obj_set_y(compass_calibration_text, 300);
        lv_obj_set_width(compass_calibration_text, 320);
        lv_label_set_text(compass_calibration_text, "请把手表按图示方向\n移动进行校准");
        lv_obj_set_style_text_align(compass_calibration_text, LV_TEXT_ALIGN_CENTER, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

