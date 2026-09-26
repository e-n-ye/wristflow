/**
 * @file settings_entry_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_entry_gen.h"
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

lv_obj_t * settings_entry_create(lv_obj_t * parent, const char * title, const char * icon, lv_color_t tint)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "settings_entry_#");
        lv_obj_set_width(lv_button_0, 342);
        lv_obj_set_height(lv_button_0, 92);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, BG_SURFACE, 0);
        lv_obj_set_style_border_width(lv_button_0, 0, 0);
        lv_obj_set_style_radius(lv_button_0, 22, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);

        lv_obj_t * lv_obj_0 = lv_obj_create(lv_button_0);
        lv_obj_set_x(lv_obj_0, 18);
        lv_obj_set_align(lv_obj_0, LV_ALIGN_LEFT_MID);
        lv_obj_set_width(lv_obj_0, 48);
        lv_obj_set_height(lv_obj_0, 48);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, tint, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 24, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, icon);
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_button_0);
        lv_obj_set_x(lv_label_1, 82);
        lv_obj_set_align(lv_label_1, LV_ALIGN_LEFT_MID);
        lv_obj_set_width(lv_label_1, 242);
        lv_label_set_text(lv_label_1, title);
        lv_obj_set_style_text_font(lv_label_1, title_24, 0);
        lv_obj_set_style_text_color(lv_label_1, FG_PRIMARY, 0);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

