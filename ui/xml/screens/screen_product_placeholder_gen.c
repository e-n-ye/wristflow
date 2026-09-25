/**
 * @file screen_product_placeholder_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_product_placeholder_gen.h"
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

lv_obj_t * screen_product_placeholder_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_product_placeholder_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        app_header_create(lv_obj_0, "应用");

        lv_obj_t * placeholder_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(placeholder_icon, "placeholder_icon");
        lv_obj_set_x(placeholder_icon, 24);
        lv_obj_set_y(placeholder_icon, 134);
        lv_obj_set_width(placeholder_icon, 342);
        lv_label_set_text(placeholder_icon, "");
        lv_obj_set_style_text_font(placeholder_icon, icons_44, 0);
        lv_obj_set_style_text_align(placeholder_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(placeholder_icon, FG_SECONDARY, 0);

        lv_obj_t * placeholder_state = lv_label_create(lv_obj_0);
        lv_obj_set_name(placeholder_state, "placeholder_state");
        lv_obj_set_x(placeholder_state, 24);
        lv_obj_set_y(placeholder_state, 216);
        lv_obj_set_width(placeholder_state, 342);
        lv_label_set_text(placeholder_state, "功能待接入");
        lv_obj_set_style_text_font(placeholder_state, title_24, 0);
        lv_obj_set_style_text_align(placeholder_state, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * placeholder_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(placeholder_label, "placeholder_label");
        lv_obj_set_x(placeholder_label, 44);
        lv_obj_set_y(placeholder_label, 272);
        lv_obj_set_width(placeholder_label, 302);
        lv_label_set_text(placeholder_label, "未接入心率传感器");
        lv_obj_set_style_text_font(placeholder_label, body_20, 0);
        lv_obj_set_style_text_align(placeholder_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(placeholder_label, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

