/**
 * @file metric_half_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "metric_half_gen.h"
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

lv_obj_t * metric_half_create(lv_obj_t * parent, const char * title, const char * value, const char * detail, const char * icon, lv_color_t surface_color, lv_color_t text_color, lv_color_t accent_color)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * tile_half_0 = tile_half_create(parent, surface_color);
        lv_obj_set_name_static(tile_half_0, "metric_half_#");

        lv_obj_t * caption_label = lv_label_create(tile_half_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 20);
        lv_obj_set_y(caption_label, 18);
        lv_obj_set_width(caption_label, 240);
        lv_label_set_long_mode(caption_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(caption_label, title);
        lv_obj_set_style_text_font(caption_label, body_20, 0);
        lv_obj_set_style_text_color(caption_label, text_color, 0);

        lv_obj_t * value_label = lv_label_create(tile_half_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 20);
        lv_obj_set_y(value_label, 56);
        lv_obj_set_width(value_label, 230);
        lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(value_label, value);
        lv_obj_set_style_text_font(value_label, metric_56, 0);
        lv_obj_set_style_text_color(value_label, text_color, 0);

        lv_obj_t * detail_label = lv_label_create(tile_half_0);
        lv_obj_set_name(detail_label, "detail_label");
        lv_obj_set_x(detail_label, 20);
        lv_obj_set_y(detail_label, 133);
        lv_obj_set_width(detail_label, 280);
        lv_label_set_long_mode(detail_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(detail_label, detail);
        lv_obj_set_style_text_font(detail_label, body_20, 0);
        lv_obj_set_style_text_color(detail_label, text_color, 0);

        lv_obj_t * icon_label = lv_label_create(tile_half_0);
        lv_obj_set_name(icon_label, "icon_label");
        lv_obj_set_x(icon_label, 270);
        lv_obj_set_y(icon_label, 64);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_font(icon_label, icons_44, 0);
        lv_obj_set_style_text_color(icon_label, accent_color, 0);

        the_root = tile_half_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

