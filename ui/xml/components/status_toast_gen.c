/**
 * @file status_toast_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "status_toast_gen.h"
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

lv_obj_t * status_toast_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "status_toast_#");
        lv_obj_set_x(lv_obj_0, 40);
        lv_obj_set_y(lv_obj_0, 59);
        lv_obj_set_width(lv_obj_0, 310);
        lv_obj_set_height(lv_obj_0, 60);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x303030), 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_color(lv_obj_0, lv_color_hex(0x666666), 0);
        lv_obj_set_style_border_width(lv_obj_0, 1, 0);
        lv_obj_set_style_radius(lv_obj_0, 30, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_obj_0, 0, 0);

        lv_obj_t * toast_text = lv_label_create(lv_obj_0);
        lv_obj_set_name(toast_text, "toast_text");
        lv_obj_set_align(toast_text, LV_ALIGN_CENTER);
        lv_obj_set_width(toast_text, 286);
        lv_label_set_text(toast_text, "勿扰模式已开启");
        lv_obj_set_style_text_font(toast_text, title_24, 0);
        lv_obj_set_style_text_color(toast_text, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(toast_text, LV_TEXT_ALIGN_CENTER, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

