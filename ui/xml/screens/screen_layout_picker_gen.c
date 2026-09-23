/**
 * @file screen_layout_picker_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_layout_picker_gen.h"
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

lv_obj_t * screen_layout_picker_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_layout_picker_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * picker_header_0 = picker_header_create(lv_obj_0, "新增页面");
        lv_obj_set_x(picker_header_0, 24);
        lv_obj_set_y(picker_header_0, 20);

        lv_obj_t * quarters = layout_option_create(lv_obj_0);
        lv_obj_set_name(quarters, "quarters");
        lv_obj_set_x(quarters, 24);
        lv_obj_set_y(quarters, 80);
        lv_obj_t * layout_block_0 = layout_block_create(quarters);
        lv_obj_set_x(layout_block_0, 12);
        lv_obj_set_y(layout_block_0, 12);
        lv_obj_set_width(layout_block_0, 66);
        lv_obj_set_height(layout_block_0, 66);

        lv_obj_t * layout_block_1 = layout_block_create(quarters);
        lv_obj_set_x(layout_block_1, 87);
        lv_obj_set_y(layout_block_1, 12);
        lv_obj_set_width(layout_block_1, 66);
        lv_obj_set_height(layout_block_1, 66);

        lv_obj_t * layout_block_2 = layout_block_create(quarters);
        lv_obj_set_x(layout_block_2, 12);
        lv_obj_set_y(layout_block_2, 86);
        lv_obj_set_width(layout_block_2, 66);
        lv_obj_set_height(layout_block_2, 66);

        lv_obj_t * layout_block_3 = layout_block_create(quarters);
        lv_obj_set_x(layout_block_3, 87);
        lv_obj_set_y(layout_block_3, 86);
        lv_obj_set_width(layout_block_3, 66);
        lv_obj_set_height(layout_block_3, 66);

        lv_obj_t * bottom_half = layout_option_create(lv_obj_0);
        lv_obj_set_name(bottom_half, "bottom_half");
        lv_obj_set_x(bottom_half, 201);
        lv_obj_set_y(bottom_half, 80);
        lv_obj_t * layout_block_4 = layout_block_create(bottom_half);
        lv_obj_set_x(layout_block_4, 12);
        lv_obj_set_y(layout_block_4, 12);
        lv_obj_set_width(layout_block_4, 66);
        lv_obj_set_height(layout_block_4, 66);

        lv_obj_t * layout_block_5 = layout_block_create(bottom_half);
        lv_obj_set_x(layout_block_5, 87);
        lv_obj_set_y(layout_block_5, 12);
        lv_obj_set_width(layout_block_5, 66);
        lv_obj_set_height(layout_block_5, 66);

        lv_obj_t * layout_block_6 = layout_block_create(bottom_half);
        lv_obj_set_x(layout_block_6, 12);
        lv_obj_set_y(layout_block_6, 86);
        lv_obj_set_width(layout_block_6, 141);
        lv_obj_set_height(layout_block_6, 66);

        lv_obj_t * top_half = layout_option_create(lv_obj_0);
        lv_obj_set_name(top_half, "top_half");
        lv_obj_set_x(top_half, 24);
        lv_obj_set_y(top_half, 258);
        lv_obj_t * layout_block_7 = layout_block_create(top_half);
        lv_obj_set_x(layout_block_7, 12);
        lv_obj_set_y(layout_block_7, 12);
        lv_obj_set_width(layout_block_7, 141);
        lv_obj_set_height(layout_block_7, 66);

        lv_obj_t * layout_block_8 = layout_block_create(top_half);
        lv_obj_set_x(layout_block_8, 12);
        lv_obj_set_y(layout_block_8, 86);
        lv_obj_set_width(layout_block_8, 66);
        lv_obj_set_height(layout_block_8, 66);

        lv_obj_t * layout_block_9 = layout_block_create(top_half);
        lv_obj_set_x(layout_block_9, 87);
        lv_obj_set_y(layout_block_9, 86);
        lv_obj_set_width(layout_block_9, 66);
        lv_obj_set_height(layout_block_9, 66);

        lv_obj_t * full = layout_option_create(lv_obj_0);
        lv_obj_set_name(full, "full");
        lv_obj_set_x(full, 201);
        lv_obj_set_y(full, 258);
        lv_obj_t * layout_block_10 = layout_block_create(full);
        lv_obj_set_x(layout_block_10, 12);
        lv_obj_set_y(layout_block_10, 12);
        lv_obj_set_width(layout_block_10, 141);
        lv_obj_set_height(layout_block_10, 140);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

