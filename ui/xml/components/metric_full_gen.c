/**
 * @file metric_full_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "metric_full_gen.h"
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

lv_obj_t * metric_full_create(lv_obj_t * parent, const char * title, const char * value, const char * detail, const char * icon, lv_color_t accent_color)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * tile_full_0 = tile_full_create(parent);
        lv_obj_set_name_static(tile_full_0, "metric_full_#");

        lv_obj_t * caption_label = lv_label_create(tile_full_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 0);
        lv_obj_set_y(caption_label, 20);
        lv_obj_set_width(caption_label, 342);
        lv_label_set_long_mode(caption_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(caption_label, title);
        lv_obj_set_style_text_align(caption_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(caption_label, title_24, 0);
        lv_obj_set_style_text_color(caption_label, FG_PRIMARY, 0);

        lv_obj_t * icon_label = lv_label_create(tile_full_0);
        lv_obj_set_name(icon_label, "icon_label");
        lv_obj_set_x(icon_label, 0);
        lv_obj_set_y(icon_label, 96);
        lv_obj_set_width(icon_label, 342);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_align(icon_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(icon_label, icons_44, 0);
        lv_obj_set_style_text_color(icon_label, accent_color, 0);

        lv_obj_t * value_label = lv_label_create(tile_full_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 0);
        lv_obj_set_y(value_label, 166);
        lv_obj_set_width(value_label, 342);
        lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(value_label, value);
        lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(value_label, metric_56, 0);
        lv_obj_set_style_text_color(value_label, FG_PRIMARY, 0);

        lv_obj_t * detail_label = lv_label_create(tile_full_0);
        lv_obj_set_name(detail_label, "detail_label");
        lv_obj_set_x(detail_label, 16);
        lv_obj_set_y(detail_label, 274);
        lv_obj_set_width(detail_label, 310);
        lv_label_set_long_mode(detail_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(detail_label, detail);
        lv_obj_set_style_text_align(detail_label, LV_TEXT_ALIGN_CENTER, 0);
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

