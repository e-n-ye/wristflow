/**
 * @file choice_confirm_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "choice_confirm_gen.h"
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

lv_obj_t * choice_confirm_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "choice_confirm_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * choice_back = lv_button_create(lv_obj_0);
        lv_obj_set_name(choice_back, "choice_back");
        lv_obj_set_x(choice_back, 24);
        lv_obj_set_y(choice_back, 22);
        lv_obj_set_width(choice_back, 48);
        lv_obj_set_height(choice_back, 44);
        lv_obj_set_style_bg_opa(choice_back, (255 * 0 / 100), 0);
        lv_obj_set_style_shadow_width(choice_back, 0, 0);
        lv_obj_set_style_pad_all(choice_back, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(choice_back);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);

        lv_obj_t * choice_text = lv_label_create(lv_obj_0);
        lv_obj_set_name(choice_text, "choice_text");
        lv_obj_set_x(choice_text, 36);
        lv_obj_set_y(choice_text, 100);
        lv_obj_set_width(choice_text, 318);
        lv_label_set_text(choice_text, "请选择抬腕后显示的表盘，选择息屏表盘将关闭抬腕亮屏。");
        lv_obj_set_style_text_align(choice_text, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * choice_first = lv_button_create(lv_obj_0);
        lv_obj_set_name(choice_first, "choice_first");
        lv_obj_set_x(choice_first, 36);
        lv_obj_set_y(choice_first, 274);
        lv_obj_set_width(choice_first, 318);
        lv_obj_set_height(choice_first, 66);
        lv_obj_set_style_bg_color(choice_first, BG_SURFACE, 0);
        lv_obj_set_style_radius(choice_first, 33, 0);
        lv_obj_set_style_shadow_width(choice_first, 0, 0);
        lv_obj_set_style_pad_all(choice_first, 0, 0);
        lv_obj_t * choice_first_text = lv_label_create(choice_first);
        lv_obj_set_name(choice_first_text, "choice_first_text");
        lv_obj_set_align(choice_first_text, LV_ALIGN_CENTER);
        lv_label_set_text(choice_first_text, "息屏表盘");

        lv_obj_t * choice_second = lv_button_create(lv_obj_0);
        lv_obj_set_name(choice_second, "choice_second");
        lv_obj_set_x(choice_second, 36);
        lv_obj_set_y(choice_second, 352);
        lv_obj_set_width(choice_second, 318);
        lv_obj_set_height(choice_second, 66);
        lv_obj_set_style_bg_color(choice_second, BG_SURFACE, 0);
        lv_obj_set_style_radius(choice_second, 33, 0);
        lv_obj_set_style_shadow_width(choice_second, 0, 0);
        lv_obj_set_style_pad_all(choice_second, 0, 0);
        lv_obj_t * choice_second_text = lv_label_create(choice_second);
        lv_obj_set_name(choice_second_text, "choice_second_text");
        lv_obj_set_align(choice_second_text, LV_ALIGN_CENTER);
        lv_label_set_text(choice_second_text, "主表盘");

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

