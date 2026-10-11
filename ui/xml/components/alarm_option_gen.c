/**
 * @file alarm_option_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "alarm_option_gen.h"
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

lv_obj_t * alarm_option_create(lv_obj_t * parent, const char * label)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "alarm_option_#");
        lv_obj_set_width(lv_button_0, 342);
        lv_obj_set_height(lv_button_0, 78);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, lv_color_hex(0x202124), 0);
        lv_obj_set_style_bg_opa(lv_button_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);
        lv_obj_set_style_radius(lv_button_0, 22, 0);
        lv_obj_set_style_text_color(lv_button_0, FG_PRIMARY, 0);

        lv_obj_t * option_label = lv_label_create(lv_button_0);
        lv_obj_set_name(option_label, "option_label");
        lv_obj_set_x(option_label, 20);
        lv_obj_set_y(option_label, 22);
        lv_obj_set_width(option_label, 264);
        lv_label_set_text(option_label, label);
        lv_obj_set_style_text_font(option_label, title_24, 0);

        lv_obj_t * option_check = lv_label_create(lv_button_0);
        lv_obj_set_name(option_check, "option_check");
        lv_obj_set_x(option_check, 288);
        lv_obj_set_y(option_check, 26);
        lv_label_set_text(option_check, "");
        lv_obj_set_style_text_font(option_check, icons_20, 0);
        lv_obj_set_style_text_color(option_check, lv_color_hex(0x008cff), 0);
        lv_obj_set_flag(option_check, LV_OBJ_FLAG_HIDDEN, true);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

