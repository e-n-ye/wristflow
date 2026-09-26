/**
 * @file product_metric_full_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "product_metric_full_gen.h"
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

lv_obj_t * product_metric_full_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * tile_full_0 = tile_full_create(parent);
        lv_obj_set_name_static(tile_full_0, "product_metric_full_#");
        lv_obj_set_style_bg_color(tile_full_0, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(tile_full_0, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(tile_full_0, RADIUS_LG, 0);

        lv_obj_t * icon_label = lv_label_create(tile_full_0);
        lv_obj_set_name(icon_label, "icon_label");
        lv_obj_set_x(icon_label, 24);
        lv_obj_set_y(icon_label, 24);
        lv_label_set_text(icon_label, "");
        lv_obj_set_style_text_font(icon_label, icons_44, 0);
        lv_obj_set_style_text_color(icon_label, ACCENT_BLUE, 0);

        lv_obj_t * caption_label = lv_label_create(tile_full_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 24);
        lv_obj_set_y(caption_label, 92);
        lv_obj_set_width(caption_label, 294);
        lv_label_set_text(caption_label, "天气");
        lv_obj_set_style_text_font(caption_label, title_24, 0);
        lv_obj_set_style_text_color(caption_label, FG_SECONDARY, 0);

        lv_obj_t * value_label = lv_label_create(tile_full_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 24);
        lv_obj_set_y(value_label, 145);
        lv_obj_set_width(value_label, 294);
        lv_label_set_text(value_label, "--");
        lv_obj_set_style_text_font(value_label, metric_56, 0);
        lv_obj_set_style_text_color(value_label, FG_PRIMARY, 0);

        lv_obj_t * detail_label = lv_label_create(tile_full_0);
        lv_obj_set_name(detail_label, "detail_label");
        lv_obj_set_x(detail_label, 24);
        lv_obj_set_y(detail_label, 286);
        lv_obj_set_width(detail_label, 294);
        lv_label_set_text(detail_label, "等待手机同步天气");
        lv_obj_set_style_text_font(detail_label, body_20, 0);
        lv_obj_set_style_text_color(detail_label, FG_SECONDARY, 0);

        the_root = tile_full_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

