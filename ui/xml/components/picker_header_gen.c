/**
 * @file picker_header_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "picker_header_gen.h"
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

lv_obj_t * picker_header_create(lv_obj_t * parent, const char * title)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "picker_header_#");
        lv_obj_set_width(lv_obj_0, 342);
        lv_obj_set_height(lv_obj_0, 44);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * back_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(back_icon, "back_icon");
        lv_obj_set_x(back_icon, 0);
        lv_obj_set_y(back_icon, 5);
        lv_label_set_text(back_icon, "");
        lv_obj_set_style_text_font(back_icon, icons_20, 0);
        lv_obj_set_style_text_color(back_icon, FG_PRIMARY, 0);

        lv_obj_t * title_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(title_label, "title_label");
        lv_obj_set_x(title_label, 26);
        lv_obj_set_y(title_label, 0);
        lv_obj_set_width(title_label, 210);
        lv_label_set_long_mode(title_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(title_label, title);
        lv_obj_set_style_text_font(title_label, title_24, 0);
        lv_obj_set_style_text_color(title_label, FG_PRIMARY, 0);

        lv_obj_t * clock_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(clock_label, "clock_label");
        lv_obj_set_x(clock_label, 268);
        lv_obj_set_y(clock_label, 3);
        lv_obj_set_width(clock_label, 74);
        lv_label_set_text(clock_label, "10:48");
        lv_obj_set_style_text_font(clock_label, body_20, 0);
        lv_obj_set_style_text_color(clock_label, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(clock_label, LV_TEXT_ALIGN_RIGHT, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

