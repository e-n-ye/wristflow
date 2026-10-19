/**
 * @file activity_full_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "activity_full_gen.h"
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

lv_obj_t * activity_full_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * tile_full_0 = tile_full_create(parent);
        lv_obj_set_name_static(tile_full_0, "activity_full_#");
        lv_obj_set_style_bg_color(tile_full_0, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(tile_full_0, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(tile_full_0, RADIUS_LG, 0);

        lv_obj_t * caption_label = lv_label_create(tile_full_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 24);
        lv_obj_set_y(caption_label, 18);
        lv_label_set_text(caption_label, "活力指标");
        lv_obj_set_style_text_font(caption_label, title_24, 0);
        lv_obj_set_style_text_color(caption_label, FG_SECONDARY, 0);

        lv_obj_t * activity_rings_0 = activity_rings_create(tile_full_0);
        lv_obj_set_y(activity_rings_0, 62);

        lv_obj_t * value_label = lv_label_create(tile_full_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 18);
        lv_obj_set_y(value_label, 236);
        lv_obj_set_width(value_label, 98);
        lv_label_set_text(value_label, "--");
        lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_font(value_label, value_36, 0);
        lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(value_label, lv_color_hex(0xf97316), 0);

        lv_obj_t * activity_value_1 = lv_label_create(tile_full_0);
        lv_obj_set_name(activity_value_1, "activity_value_1");
        lv_obj_set_x(activity_value_1, 122);
        lv_obj_set_y(activity_value_1, 236);
        lv_obj_set_width(activity_value_1, 98);
        lv_label_set_text(activity_value_1, "--");
        lv_label_set_long_mode(activity_value_1, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_font(activity_value_1, value_36, 0);
        lv_obj_set_style_text_align(activity_value_1, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(activity_value_1, lv_color_hex(0xe2db48), 0);

        lv_obj_t * activity_value_2 = lv_label_create(tile_full_0);
        lv_obj_set_name(activity_value_2, "activity_value_2");
        lv_obj_set_x(activity_value_2, 226);
        lv_obj_set_y(activity_value_2, 236);
        lv_obj_set_width(activity_value_2, 98);
        lv_label_set_text(activity_value_2, "--");
        lv_label_set_long_mode(activity_value_2, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_font(activity_value_2, value_36, 0);
        lv_obj_set_style_text_align(activity_value_2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(activity_value_2, lv_color_hex(0x38bdf8), 0);

        lv_obj_t * lv_label_0 = lv_label_create(tile_full_0);
        lv_obj_set_x(lv_label_0, 18);
        lv_obj_set_y(lv_label_0, 296);
        lv_obj_set_width(lv_label_0, 98);
        lv_label_set_text(lv_label_0, "千卡");
        lv_obj_set_style_text_font(lv_label_0, body_20, 0);
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_SECONDARY, 0);

        lv_obj_t * lv_label_1 = lv_label_create(tile_full_0);
        lv_obj_set_x(lv_label_1, 122);
        lv_obj_set_y(lv_label_1, 296);
        lv_obj_set_width(lv_label_1, 98);
        lv_label_set_text(lv_label_1, "步");
        lv_obj_set_style_text_font(lv_label_1, body_20, 0);
        lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(lv_label_1, FG_SECONDARY, 0);

        lv_obj_t * lv_label_2 = lv_label_create(tile_full_0);
        lv_obj_set_x(lv_label_2, 226);
        lv_obj_set_y(lv_label_2, 296);
        lv_obj_set_width(lv_label_2, 98);
        lv_label_set_text(lv_label_2, "分钟");
        lv_obj_set_style_text_font(lv_label_2, body_20, 0);
        lv_obj_set_style_text_align(lv_label_2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(lv_label_2, FG_SECONDARY, 0);

        the_root = tile_full_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

