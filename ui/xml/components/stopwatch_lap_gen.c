/**
 * @file stopwatch_lap_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "stopwatch_lap_gen.h"
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

lv_obj_t * stopwatch_lap_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "stopwatch_lap_#");
        lv_obj_set_width(lv_obj_0, 342);
        lv_obj_set_height(lv_obj_0, 56);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 28, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 18);
        lv_obj_set_y(lv_label_0, 16);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);

        lv_obj_t * lap_number = lv_label_create(lv_obj_0);
        lv_obj_set_name(lap_number, "lap_number");
        lv_obj_set_x(lap_number, 48);
        lv_obj_set_y(lap_number, 11);
        lv_obj_set_width(lap_number, 56);
        lv_label_set_text(lap_number, "01");
        lv_obj_set_style_text_font(lap_number, title_24, 0);

        lv_obj_t * lap_elapsed = lv_label_create(lv_obj_0);
        lv_obj_set_name(lap_elapsed, "lap_elapsed");
        lv_obj_set_x(lap_elapsed, 112);
        lv_obj_set_y(lap_elapsed, 11);
        lv_obj_set_width(lap_elapsed, 212);
        lv_label_set_text(lap_elapsed, "00:05.06");
        lv_obj_set_style_text_font(lap_elapsed, title_24, 0);
        lv_obj_set_style_text_align(lap_elapsed, LV_TEXT_ALIGN_RIGHT, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

