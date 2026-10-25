/**
 * @file screen_stopwatch_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_stopwatch_gen.h"
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

lv_obj_t * screen_stopwatch_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_stopwatch_#");
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

        app_header_create(lv_obj_0, "秒表");

        lv_obj_t * stopwatch_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(stopwatch_clock, "stopwatch_clock");
        lv_obj_set_x(stopwatch_clock, 260);
        lv_obj_set_y(stopwatch_clock, 30);
        lv_obj_set_width(stopwatch_clock, 106);
        lv_label_set_text(stopwatch_clock, "13:40");
        lv_obj_set_style_text_align(stopwatch_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * stopwatch_readout = lv_obj_create(lv_obj_0);
        lv_obj_set_name(stopwatch_readout, "stopwatch_readout");
        lv_obj_set_x(stopwatch_readout, 24);
        lv_obj_set_y(stopwatch_readout, 139);
        lv_obj_set_width(stopwatch_readout, 342);
        lv_obj_set_height(stopwatch_readout, 88);
        lv_obj_set_flag(stopwatch_readout, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(stopwatch_readout, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(stopwatch_readout, 0, 0);
        lv_obj_set_style_pad_all(stopwatch_readout, 0, 0);
        lv_obj_set_style_pad_column(stopwatch_readout, 0, 0);
        lv_obj_set_style_layout(stopwatch_readout, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(stopwatch_readout, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_style_flex_main_place(stopwatch_readout, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(stopwatch_readout, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(stopwatch_readout, FG_PRIMARY, 0);
        lv_obj_t * stopwatch_time = lv_label_create(stopwatch_readout);
        lv_obj_set_name(stopwatch_time, "stopwatch_time");
        lv_label_set_text(stopwatch_time, "00:00");
        lv_obj_set_style_text_font(stopwatch_time, metric_56, 0);

        lv_obj_t * stopwatch_fraction = lv_label_create(stopwatch_readout);
        lv_obj_set_name(stopwatch_fraction, "stopwatch_fraction");
        lv_label_set_text(stopwatch_fraction, ".00");
        lv_obj_set_style_text_font(stopwatch_fraction, metric_56, 0);

        lv_obj_t * stopwatch_laps = lv_obj_create(lv_obj_0);
        lv_obj_set_name(stopwatch_laps, "stopwatch_laps");
        lv_obj_set_x(stopwatch_laps, 24);
        lv_obj_set_y(stopwatch_laps, 171);
        lv_obj_set_width(stopwatch_laps, 342);
        lv_obj_set_height(stopwatch_laps, 159);
        lv_obj_set_flag(stopwatch_laps, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_scrollbar_mode(stopwatch_laps, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(stopwatch_laps, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(stopwatch_laps, 0, 0);
        lv_obj_set_style_pad_all(stopwatch_laps, 0, 0);
        lv_obj_set_style_radius(stopwatch_laps, 0, 0);

        lv_obj_t * stopwatch_limit = lv_label_create(lv_obj_0);
        lv_obj_set_name(stopwatch_limit, "stopwatch_limit");
        lv_obj_set_x(stopwatch_limit, 24);
        lv_obj_set_y(stopwatch_limit, 309);
        lv_obj_set_width(stopwatch_limit, 342);
        lv_label_set_text(stopwatch_limit, "已达上限");
        lv_obj_set_flag(stopwatch_limit, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_style_text_color(stopwatch_limit, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(stopwatch_limit, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * stopwatch_reset = lv_button_create(lv_obj_0);
        lv_obj_set_name(stopwatch_reset, "stopwatch_reset");
        lv_obj_set_x(stopwatch_reset, 24);
        lv_obj_set_y(stopwatch_reset, 342);
        lv_obj_set_width(stopwatch_reset, 164);
        lv_obj_set_height(stopwatch_reset, 72);
        lv_obj_set_flag(stopwatch_reset, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_style_radius(stopwatch_reset, 36, 0);
        lv_obj_set_style_bg_color(stopwatch_reset, lv_color_hex(0x252525), 0);
        lv_obj_set_style_shadow_width(stopwatch_reset, 0, 0);
        lv_obj_set_style_pad_all(stopwatch_reset, 0, 0);
        lv_obj_set_style_text_color(stopwatch_reset, FG_PRIMARY, 0);
        lv_obj_t * stopwatch_reset_icon = lv_label_create(stopwatch_reset);
        lv_obj_set_name(stopwatch_reset_icon, "stopwatch_reset_icon");
        lv_obj_set_align(stopwatch_reset_icon, LV_ALIGN_CENTER);
        lv_label_set_text(stopwatch_reset_icon, "");
        lv_obj_set_style_text_font(stopwatch_reset_icon, icons_44, 0);

        lv_obj_t * stopwatch_toggle = lv_button_create(lv_obj_0);
        lv_obj_set_name(stopwatch_toggle, "stopwatch_toggle");
        lv_obj_set_x(stopwatch_toggle, 24);
        lv_obj_set_y(stopwatch_toggle, 342);
        lv_obj_set_width(stopwatch_toggle, 342);
        lv_obj_set_height(stopwatch_toggle, 72);
        lv_obj_set_style_radius(stopwatch_toggle, 36, 0);
        lv_obj_set_style_bg_color(stopwatch_toggle, lv_color_hex(0x252525), 0);
        lv_obj_set_style_shadow_width(stopwatch_toggle, 0, 0);
        lv_obj_set_style_pad_all(stopwatch_toggle, 0, 0);
        lv_obj_set_style_text_color(stopwatch_toggle, FG_PRIMARY, 0);
        lv_obj_t * stopwatch_toggle_icon = lv_label_create(stopwatch_toggle);
        lv_obj_set_name(stopwatch_toggle_icon, "stopwatch_toggle_icon");
        lv_obj_set_align(stopwatch_toggle_icon, LV_ALIGN_CENTER);
        lv_label_set_text(stopwatch_toggle_icon, "开始");
        lv_obj_set_style_text_font(stopwatch_toggle_icon, title_24, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

