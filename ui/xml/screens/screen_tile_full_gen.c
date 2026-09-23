/**
 * @file screen_tile_full_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_tile_full_gen.h"
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

lv_obj_t * screen_tile_full_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_tile_full_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * tile_slots = tile_grid_create(lv_obj_0);
        lv_obj_set_name(tile_slots, "tile_slots");
        lv_obj_set_x(tile_slots, TILE_AREA_X);
        lv_obj_set_y(tile_slots, TILE_AREA_Y);
        lv_obj_t * slot_0 = metric_full_create(tile_slots, "今日步数", "6,280", "目标 8,000 步", "", ACCENT_GREEN);
        lv_obj_set_name(slot_0, "slot_0");
        lv_obj_set_style_grid_cell_column_pos(slot_0, 0, 0);
        lv_obj_set_style_grid_cell_row_pos(slot_0, 0, 0);

        lv_obj_t * page_dots_0 = page_dots_create(lv_obj_0, 54);
        lv_obj_set_x(page_dots_0, 165);
        lv_obj_set_y(page_dots_0, PAGER_BOTTOM_Y);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

