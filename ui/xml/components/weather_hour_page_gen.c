/**
 * @file weather_hour_page_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "weather_hour_page_gen.h"
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

lv_obj_t * weather_hour_page_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "weather_hour_page_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 320);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * column_0 = weather_hour_column_create(lv_obj_0);
        lv_obj_set_name(column_0, "column_0");
        lv_obj_set_x(column_0, 10);
        lv_obj_set_y(column_0, 0);

        lv_obj_t * column_1 = weather_hour_column_create(lv_obj_0);
        lv_obj_set_name(column_1, "column_1");
        lv_obj_set_x(column_1, 106);
        lv_obj_set_y(column_1, 0);

        lv_obj_t * column_2 = weather_hour_column_create(lv_obj_0);
        lv_obj_set_name(column_2, "column_2");
        lv_obj_set_x(column_2, 202);
        lv_obj_set_y(column_2, 0);

        lv_obj_t * column_3 = weather_hour_column_create(lv_obj_0);
        lv_obj_set_name(column_3, "column_3");
        lv_obj_set_x(column_3, 298);
        lv_obj_set_y(column_3, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

