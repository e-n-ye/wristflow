/**
 * @file card_container_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "card_container_gen.h"
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

lv_obj_t * card_container_create(lv_obj_t * parent, const char * card_title, const char * metric_val, const char * icon, lv_color_t surface_color, lv_color_t value_color, lv_color_t caption_color, lv_color_t accent_color)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * tile_quarter_0 = tile_quarter_create(parent, surface_color);
        lv_obj_set_name_static(tile_quarter_0, "card_container_#");

        lv_obj_t * icon_label = lv_label_create(tile_quarter_0);
        lv_obj_set_name(icon_label, "icon_label");
        lv_obj_set_x(icon_label, 20);
        lv_obj_set_y(icon_label, 20);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_font(icon_label, icons_44, 0);
        lv_obj_set_style_text_color(icon_label, accent_color, 0);

        lv_obj_t * value_label = lv_label_create(tile_quarter_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 20);
        lv_obj_set_y(value_label, 84);
        lv_obj_set_width(value_label, 125);
        lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(value_label, metric_val);
        lv_obj_set_style_text_font(value_label, value_36, 0);
        lv_obj_set_style_text_color(value_label, value_color, 0);

        lv_obj_t * caption_label = lv_label_create(tile_quarter_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 20);
        lv_obj_set_y(caption_label, 136);
        lv_obj_set_width(caption_label, 125);
        lv_label_set_long_mode(caption_label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(caption_label, card_title);
        lv_obj_set_style_text_font(caption_label, body_20, 0);
        lv_obj_set_style_text_color(caption_label, caption_color, 0);

        the_root = tile_quarter_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

