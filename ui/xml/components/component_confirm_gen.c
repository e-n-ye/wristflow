/**
 * @file component_confirm_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "component_confirm_gen.h"
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

lv_obj_t * component_confirm_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "component_confirm_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * confirm_text = lv_label_create(lv_obj_0);
        lv_obj_set_name(confirm_text, "confirm_text");
        lv_obj_set_x(confirm_text, 40);
        lv_obj_set_y(confirm_text, 110);
        lv_obj_set_width(confirm_text, 310);
        lv_label_set_text(confirm_text, "是否删除当前小组件？");
        lv_obj_set_style_text_align(confirm_text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(confirm_text, title_24, 0);
        lv_obj_set_style_text_color(confirm_text, FG_PRIMARY, 0);

        lv_obj_t * confirm_cancel = lv_button_create(lv_obj_0);
        lv_obj_set_name(confirm_cancel, "confirm_cancel");
        lv_obj_set_x(confirm_cancel, 54);
        lv_obj_set_y(confirm_cancel, 308);
        lv_obj_set_width(confirm_cancel, 126);
        lv_obj_set_height(confirm_cancel, 68);
        lv_obj_set_style_bg_color(confirm_cancel, BG_SURFACE, 0);
        lv_obj_set_style_radius(confirm_cancel, 34, 0);
        lv_obj_set_style_shadow_width(confirm_cancel, 0, 0);
        lv_obj_set_style_pad_all(confirm_cancel, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(confirm_cancel);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);

        lv_obj_t * confirm_accept = lv_button_create(lv_obj_0);
        lv_obj_set_name(confirm_accept, "confirm_accept");
        lv_obj_set_x(confirm_accept, 210);
        lv_obj_set_y(confirm_accept, 308);
        lv_obj_set_width(confirm_accept, 126);
        lv_obj_set_height(confirm_accept, 68);
        lv_obj_set_style_bg_color(confirm_accept, ACCENT_BLUE, 0);
        lv_obj_set_style_radius(confirm_accept, 34, 0);
        lv_obj_set_style_shadow_width(confirm_accept, 0, 0);
        lv_obj_set_style_pad_all(confirm_accept, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(confirm_accept);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_20, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

