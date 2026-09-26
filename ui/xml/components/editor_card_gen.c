/**
 * @file editor_card_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "editor_card_gen.h"
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

lv_obj_t * editor_card_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "editor_card_#");
        lv_obj_set_width(lv_button_0, 126);
        lv_obj_set_height(lv_button_0, 126);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(lv_button_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_button_0, 2, 0);
        lv_obj_set_style_border_color(lv_button_0, FG_MUTED, 0);
        lv_obj_set_style_radius(lv_button_0, 20, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);

        lv_obj_t * icon_label = lv_label_create(lv_button_0);
        lv_obj_set_name(icon_label, "icon_label");
        lv_obj_set_x(icon_label, 14);
        lv_obj_set_y(icon_label, 12);
        lv_label_set_text(icon_label, "+");
        lv_obj_set_style_text_font(icon_label, icons_20, 0);
        lv_obj_set_style_text_color(icon_label, ACCENT_BLUE, 0);

        lv_obj_t * value_label = lv_label_create(lv_button_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 14);
        lv_obj_set_y(value_label, 44);
        lv_label_set_text(value_label, "--");
        lv_obj_set_style_text_font(value_label, value_36, 0);
        lv_obj_set_style_text_color(value_label, FG_PRIMARY, 0);

        lv_obj_t * caption_label = lv_label_create(lv_button_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 14);
        lv_obj_set_y(caption_label, -12);
        lv_obj_set_align(caption_label, LV_ALIGN_BOTTOM_LEFT);
        lv_label_set_text(caption_label, "选择组件");
        lv_obj_set_style_text_font(caption_label, body_20, 0);
        lv_obj_set_style_text_color(caption_label, FG_PRIMARY, 0);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

