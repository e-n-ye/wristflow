/**
 * @file screen_settings_list_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_settings_list_gen.h"
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

lv_obj_t * screen_settings_list_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_settings_list_#");
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

        app_header_create(lv_obj_0, "设置");

        lv_obj_t * settings_scroll = lv_obj_create(lv_obj_0);
        lv_obj_set_name(settings_scroll, "settings_scroll");
        lv_obj_set_x(settings_scroll, 24);
        lv_obj_set_y(settings_scroll, 82);
        lv_obj_set_width(settings_scroll, 342);
        lv_obj_set_height(settings_scroll, 348);
        lv_obj_set_style_layout(settings_scroll, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(settings_scroll, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_scrollbar_mode(settings_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(settings_scroll, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(settings_scroll, 0, 0);
        lv_obj_set_style_radius(settings_scroll, 0, 0);
        lv_obj_set_style_pad_all(settings_scroll, 0, 0);
        lv_obj_set_style_pad_row(settings_scroll, 12, 0);
        lv_obj_set_style_pad_bottom(settings_scroll, 20, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

