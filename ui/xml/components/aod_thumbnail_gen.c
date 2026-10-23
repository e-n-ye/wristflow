/**
 * @file aod_thumbnail_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "aod_thumbnail_gen.h"
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

lv_obj_t * aod_thumbnail_create(lv_obj_t * parent, bool center_hidden, bool corner_hidden)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "aod_thumbnail_#");
        lv_obj_set_y(lv_obj_0, 6);
        lv_obj_set_width(lv_obj_0, 234);
        lv_obj_set_height(lv_obj_0, 286);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_border_width(lv_obj_0, 1, 0);
        lv_obj_set_style_border_color(lv_obj_0, lv_color_hex(0x454a50), 0);
        lv_obj_set_style_border_post(lv_obj_0, true, 0);
        lv_obj_set_style_clip_corner(lv_obj_0, true, 0);
        lv_obj_set_style_radius(lv_obj_0, 28, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);

        lv_obj_t * aod_thumbnail_content = aod_content_create(lv_obj_0, center_hidden, corner_hidden);
        lv_obj_set_name(aod_thumbnail_content, "aod_thumbnail_content");
        lv_obj_set_x(aod_thumbnail_content, 2);
        lv_obj_set_y(aod_thumbnail_content, 8);
        lv_obj_set_style_transform_scale_x(aod_thumbnail_content, 150, 0);
        lv_obj_set_style_transform_scale_y(aod_thumbnail_content, 150, 0);
        lv_obj_set_style_transform_pivot_x(aod_thumbnail_content, 0, 0);
        lv_obj_set_style_transform_pivot_y(aod_thumbnail_content, 0, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

