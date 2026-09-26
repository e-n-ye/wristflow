/**
 * @file settings_choice_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_choice_gen.h"
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

lv_obj_t * settings_choice_create(lv_obj_t * parent, const char * title)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "settings_choice_#");
        lv_obj_set_width(lv_button_0, 342);
        lv_obj_set_height(lv_button_0, 96);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, BG_SURFACE, 0);
        lv_obj_set_style_border_width(lv_button_0, 0, 0);
        lv_obj_set_style_radius(lv_button_0, 22, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);
        lv_obj_set_style_text_color(lv_button_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_0 = lv_label_create(lv_button_0);
        lv_obj_set_x(lv_label_0, 20);
        lv_obj_set_align(lv_label_0, LV_ALIGN_LEFT_MID);
        lv_obj_set_width(lv_label_0, 234);
        lv_label_set_text(lv_label_0, title);
        lv_obj_set_style_text_font(lv_label_0, title_24, 0);

        lv_obj_t * row_check = lv_obj_create(lv_button_0);
        lv_obj_set_name(row_check, "row_check");
        lv_obj_set_x(row_check, -20);
        lv_obj_set_align(row_check, LV_ALIGN_RIGHT_MID);
        lv_obj_set_width(row_check, 42);
        lv_obj_set_height(row_check, 42);
        lv_obj_set_flag(row_check, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(row_check, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(row_check, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(row_check, ACCENT_BLUE, 0);
        lv_obj_set_style_bg_opa(row_check, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(row_check, 0, 0);
        lv_obj_set_style_radius(row_check, 21, 0);
        lv_obj_set_style_pad_all(row_check, 0, 0);
        lv_obj_t * lv_obj_0 = lv_obj_create(row_check);
        lv_obj_set_align(lv_obj_0, LV_ALIGN_CENTER);
        lv_obj_set_width(lv_obj_0, 18);
        lv_obj_set_height(lv_obj_0, 18);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 9, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

