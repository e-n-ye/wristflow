/**
 * @file layout_save_status_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "layout_save_status_gen.h"
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

lv_obj_t * layout_save_status_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_label_0 = lv_label_create(parent);
        lv_obj_set_name_static(lv_label_0, "layout_save_status_#");
        lv_obj_set_x(lv_label_0, 24);
        lv_obj_set_y(lv_label_0, 395);
        lv_obj_set_width(lv_label_0, 342);
        lv_obj_set_flag(lv_label_0, LV_OBJ_FLAG_HIDDEN, true);
        lv_label_set_text(lv_label_0, "保存失败，稍后重试");
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(lv_label_0, body_20, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_PRIMARY, 0);
        lv_obj_set_style_bg_color(lv_label_0, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(lv_label_0, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(lv_label_0, 12, 0);
        lv_obj_set_style_pad_all(lv_label_0, 8, 0);

        the_root = lv_label_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

