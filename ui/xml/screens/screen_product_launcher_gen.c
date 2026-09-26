/**
 * @file screen_product_launcher_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_product_launcher_gen.h"
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

lv_obj_t * screen_product_launcher_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_product_launcher_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * launcher_list = lv_obj_create(lv_obj_0);
        lv_obj_set_name(launcher_list, "launcher_list");
        lv_obj_set_width(launcher_list, 390);
        lv_obj_set_height(launcher_list, 450);
        lv_obj_set_scrollbar_mode(launcher_list, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(launcher_list, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(launcher_list, 0, 0);
        lv_obj_set_style_pad_left(launcher_list, 24, 0);
        lv_obj_set_style_pad_right(launcher_list, 24, 0);
        lv_obj_set_style_pad_top(launcher_list, 24, 0);
        lv_obj_set_style_pad_bottom(launcher_list, 24, 0);
        lv_obj_set_style_radius(launcher_list, 0, 0);
        lv_obj_set_style_layout(launcher_list, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(launcher_list, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_style_pad_row(launcher_list, 12, 0);

        lv_obj_t * launcher_grid = lv_obj_create(lv_obj_0);
        lv_obj_set_name(launcher_grid, "launcher_grid");
        lv_obj_set_width(launcher_grid, 390);
        lv_obj_set_height(launcher_grid, 450);
        lv_obj_set_flag(launcher_grid, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_scrollbar_mode(launcher_grid, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(launcher_grid, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(launcher_grid, 0, 0);
        lv_obj_set_style_pad_left(launcher_grid, 24, 0);
        lv_obj_set_style_pad_right(launcher_grid, 24, 0);
        lv_obj_set_style_pad_top(launcher_grid, 24, 0);
        lv_obj_set_style_pad_bottom(launcher_grid, 24, 0);
        lv_obj_set_style_radius(launcher_grid, 0, 0);
        lv_obj_set_style_layout(launcher_grid, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(launcher_grid, LV_FLEX_FLOW_ROW_WRAP, 0);
        lv_obj_set_style_pad_column(launcher_grid, 21, 0);
        lv_obj_set_style_pad_row(launcher_grid, 20, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

