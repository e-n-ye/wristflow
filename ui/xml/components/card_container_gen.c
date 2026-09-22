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

lv_obj_t * card_container_create(lv_obj_t * parent, const char * card_title, const char * metric_val, const char * icon, lv_color_t surface_color, lv_color_t value_color, lv_color_t caption_color, lv_color_t accent_color, lv_coord_t card_width, lv_coord_t card_height)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_card_container;
    static lv_style_t style_title;
    static lv_style_t style_metric;
    static lv_style_t style_status;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_card_container);
        lv_style_init(&style_title);
        lv_style_init(&style_metric);
        lv_style_init(&style_status);

        lv_style_set_bg_color(&style_card_container, BG_SURFACE);
        lv_style_set_radius(&style_card_container, 28);
        lv_style_set_pad_all(&style_card_container, 0);
        lv_style_set_clip_corner(&style_card_container, true);
        lv_style_set_text_color(&style_title, FG_SECONDARY);
        lv_style_set_text_color(&style_metric, FG_PRIMARY);
        lv_style_set_text_color(&style_status, FG_MUTED);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "card_container_#");
        lv_obj_set_width(lv_obj_0, card_width);
        lv_obj_set_height(lv_obj_0, card_height);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, surface_color, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(lv_obj_0, 28, 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);

        lv_obj_t * icon_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(icon_label, "icon_label");
        lv_obj_set_x(icon_label, 20);
        lv_obj_set_y(icon_label, 20);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_font(icon_label, icons_44, 0);
        lv_obj_set_style_text_color(icon_label, accent_color, 0);

        lv_obj_t * value_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 20);
        lv_obj_set_y(value_label, 84);
        lv_obj_set_width(value_label, 145);
        lv_label_set_text(value_label, metric_val);
        lv_obj_set_style_text_font(value_label, value_36, 0);
        lv_obj_set_style_text_color(value_label, value_color, 0);

        lv_obj_t * caption_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 20);
        lv_obj_set_y(caption_label, 136);
        lv_obj_set_width(caption_label, 145);
        lv_label_set_text(caption_label, card_title);
        lv_obj_set_style_text_font(caption_label, body_20, 0);
        lv_obj_set_style_text_color(caption_label, caption_color, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

