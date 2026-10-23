/**
 * @file screen_aod_time_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_aod_time_gen.h"
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

lv_obj_t * screen_aod_time_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_aod_time_#");
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

        app_header_create(lv_obj_0, "开始时间");

        lv_obj_t * aod_time_picker = time_picker_create(lv_obj_0);
        lv_obj_set_name(aod_time_picker, "aod_time_picker");

        lv_obj_t * aod_time_error = lv_label_create(lv_obj_0);
        lv_obj_set_name(aod_time_error, "aod_time_error");
        lv_obj_set_x(aod_time_error, 24);
        lv_obj_set_y(aod_time_error, 306);
        lv_obj_set_width(aod_time_error, 342);
        lv_obj_set_flag(aod_time_error, LV_OBJ_FLAG_HIDDEN, true);
        lv_label_set_text(aod_time_error, "开始与结束时间不能相同");
        lv_obj_set_style_text_align(aod_time_error, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(aod_time_error, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

