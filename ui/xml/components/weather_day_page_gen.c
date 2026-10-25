/**
 * @file weather_day_page_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "weather_day_page_gen.h"
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

lv_obj_t * weather_day_page_create(lv_obj_t * parent, bool short_page)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "weather_day_page_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 320);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * row_0 = weather_day_row_create(lv_obj_0);
        lv_obj_set_name(row_0, "row_0");
        lv_obj_set_x(row_0, 0);
        lv_obj_set_y(row_0, 14);

        lv_obj_t * row_1 = weather_day_row_create(lv_obj_0);
        lv_obj_set_name(row_1, "row_1");
        lv_obj_set_x(row_1, 0);
        lv_obj_set_y(row_1, 72);

        lv_obj_t * row_2 = weather_day_row_create(lv_obj_0);
        lv_obj_set_name(row_2, "row_2");
        lv_obj_set_x(row_2, 0);
        lv_obj_set_y(row_2, 130);
        lv_obj_set_flag(row_2, LV_OBJ_FLAG_HIDDEN, short_page);

        lv_obj_t * row_3 = weather_day_row_create(lv_obj_0);
        lv_obj_set_name(row_3, "row_3");
        lv_obj_set_x(row_3, 0);
        lv_obj_set_y(row_3, 188);
        lv_obj_set_flag(row_3, LV_OBJ_FLAG_HIDDEN, short_page);

        lv_obj_t * row_4 = weather_day_row_create(lv_obj_0);
        lv_obj_set_name(row_4, "row_4");
        lv_obj_set_x(row_4, 0);
        lv_obj_set_y(row_4, 246);
        lv_obj_set_flag(row_4, LV_OBJ_FLAG_HIDDEN, short_page);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

