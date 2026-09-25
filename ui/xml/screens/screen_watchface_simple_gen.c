/**
 * @file screen_watchface_simple_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_watchface_simple_gen.h"
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

lv_obj_t * screen_watchface_simple_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_watchface_simple_#");
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

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 24);
        lv_obj_set_y(lv_label_0, 45);
        lv_obj_set_width(lv_label_0, 342);
        lv_label_set_text(lv_label_0, "WRISTFLOW");
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_SECONDARY, 0);

        lv_obj_t * simple_time = lv_label_create(lv_obj_0);
        lv_obj_set_name(simple_time, "simple_time");
        lv_obj_set_x(simple_time, 24);
        lv_obj_set_y(simple_time, 160);
        lv_obj_set_width(simple_time, 342);
        lv_label_set_text(simple_time, "22:48");
        lv_obj_set_style_text_font(simple_time, clock_80, 0);
        lv_obj_set_style_text_align(simple_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * simple_battery = lv_label_create(lv_obj_0);
        lv_obj_set_name(simple_battery, "simple_battery");
        lv_obj_set_x(simple_battery, 24);
        lv_obj_set_y(simple_battery, 339);
        lv_obj_set_width(simple_battery, 342);
        lv_label_set_text(simple_battery, "53%");
        lv_obj_set_style_text_align(simple_battery, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(simple_battery, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

