/**
 * @file tile_grid_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "tile_grid_gen.h"
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

lv_obj_t * tile_grid_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "tile_grid_#");
        lv_obj_set_width(lv_obj_0, TILE_AREA_W);
        lv_obj_set_height(lv_obj_0, TILE_AREA_H);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_layout(lv_obj_0, LV_LAYOUT_GRID, 0);
        static const int32_t lv_obj_0_style_grid_column_dsc_array_0[] = {165, 165, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_style_grid_column_dsc_array(lv_obj_0, lv_obj_0_style_grid_column_dsc_array_0, 0);
        static const int32_t lv_obj_0_style_grid_row_dsc_array_0[] = {174, 174, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_style_grid_row_dsc_array(lv_obj_0, lv_obj_0_style_grid_row_dsc_array_0, 0);
        lv_obj_set_style_pad_column(lv_obj_0, 12, 0);
        lv_obj_set_style_pad_row(lv_obj_0, 14, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

