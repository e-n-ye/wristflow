/**
 * @file screen_breathing_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_breathing_gen.h"
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

lv_obj_t * screen_breathing_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_breathing_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * app_back = lv_button_create(lv_obj_0);
        lv_obj_set_name(app_back, "app_back");
        lv_obj_set_x(app_back, 18);
        lv_obj_set_y(app_back, 14);
        lv_obj_set_width(app_back, 226);
        lv_obj_set_height(app_back, 52);
        lv_obj_set_style_bg_opa(app_back, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(app_back, 0, 0);
        lv_obj_set_style_shadow_width(app_back, 0, 0);
        lv_obj_set_style_pad_all(app_back, 0, 0);
        lv_obj_t * breathing_title = lv_label_create(app_back);
        lv_obj_set_name(breathing_title, "breathing_title");
        lv_obj_set_x(breathing_title, 6);
        lv_obj_set_y(breathing_title, 12);
        lv_label_set_text(breathing_title, "呼吸");
        lv_obj_set_style_text_font(breathing_title, title_24, 0);

        lv_obj_t * breathing_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(breathing_clock, "breathing_clock");
        lv_obj_set_x(breathing_clock, 270);
        lv_obj_set_y(breathing_clock, 26);
        lv_obj_set_width(breathing_clock, 96);
        lv_label_set_text(breathing_clock, "13:40");
        lv_obj_set_style_text_font(breathing_clock, title_24, 0);
        lv_obj_set_style_text_align(breathing_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * breathing_home_panel = breathing_home_create(lv_obj_0);
        lv_obj_set_name(breathing_home_panel, "breathing_home_panel");
        lv_obj_set_y(breathing_home_panel, 76);

        lv_obj_t * breathing_duration_panel = breathing_duration_create(lv_obj_0);
        lv_obj_set_name(breathing_duration_panel, "breathing_duration_panel");
        lv_obj_set_y(breathing_duration_panel, 76);
        lv_obj_set_flag(breathing_duration_panel, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * breathing_training_panel = breathing_training_create(lv_obj_0);
        lv_obj_set_name(breathing_training_panel, "breathing_training_panel");
        lv_obj_set_y(breathing_training_panel, 76);
        lv_obj_set_flag(breathing_training_panel, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * breathing_controls_panel = breathing_controls_create(lv_obj_0);
        lv_obj_set_name(breathing_controls_panel, "breathing_controls_panel");
        lv_obj_set_y(breathing_controls_panel, 76);
        lv_obj_set_flag(breathing_controls_panel, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * breathing_result_panel = breathing_result_create(lv_obj_0);
        lv_obj_set_name(breathing_result_panel, "breathing_result_panel");
        lv_obj_set_y(breathing_result_panel, 76);
        lv_obj_set_flag(breathing_result_panel, LV_OBJ_FLAG_HIDDEN, true);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

