/**
 * @file charging_panel_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "charging_panel_gen.h"
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

lv_obj_t * charging_panel_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * charging_panel = lv_obj_create(parent);
        lv_obj_set_name_static(charging_panel, "charging_panel_#");
        lv_obj_set_name(charging_panel, "charging_panel");
        lv_obj_set_width(charging_panel, 390);
        lv_obj_set_height(charging_panel, 450);
        lv_obj_set_flag(charging_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(charging_panel, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_style_bg_color(charging_panel, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(charging_panel, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(charging_panel, 0, 0);
        lv_obj_set_style_pad_all(charging_panel, 0, 0);
        lv_obj_set_style_radius(charging_panel, 0, 0);
        lv_obj_set_style_text_color(charging_panel, lv_color_hex(0xffffff), 0);

        lv_obj_t * charging_ring = lv_obj_create(charging_panel);
        lv_obj_set_name(charging_ring, "charging_ring");
        lv_obj_set_x(charging_ring, 24);
        lv_obj_set_y(charging_ring, 24);
        lv_obj_set_width(charging_ring, 342);
        lv_obj_set_height(charging_ring, 402);
        lv_obj_set_flag(charging_ring, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(charging_ring, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(charging_ring, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(charging_ring, 12, 0);
        lv_obj_set_style_border_color(charging_ring, lv_color_hex(0x163b38), 0);
        lv_obj_set_style_radius(charging_ring, 70, 0);
        lv_obj_set_style_pad_all(charging_ring, 0, 0);

        lv_obj_t * lv_obj_0 = lv_obj_create(charging_panel);
        lv_obj_set_x(lv_obj_0, 163);
        lv_obj_set_y(lv_obj_0, 8);
        lv_obj_set_width(lv_obj_0, 64);
        lv_obj_set_height(lv_obj_0, 56);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_t * charging_bolt = lv_label_create(lv_obj_0);
        lv_obj_set_name(charging_bolt, "charging_bolt");
        lv_obj_set_align(charging_bolt, LV_ALIGN_CENTER);
        lv_label_set_text(charging_bolt, "");
        lv_obj_set_style_text_font(charging_bolt, icons_44, 0);
        lv_obj_set_style_text_color(charging_bolt, lv_color_hex(0x00dfc0), 0);

        lv_obj_t * charging_clock = lv_label_create(charging_panel);
        lv_obj_set_name(charging_clock, "charging_clock");
        lv_obj_set_x(charging_clock, 40);
        lv_obj_set_y(charging_clock, 172);
        lv_obj_set_width(charging_clock, 310);
        lv_obj_set_height(charging_clock, 100);
        lv_label_set_text(charging_clock, "21:10");
        lv_obj_set_style_text_font(charging_clock, clock_80, 0);
        lv_obj_set_style_text_align(charging_clock, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * charging_percent = lv_label_create(charging_panel);
        lv_obj_set_name(charging_percent, "charging_percent");
        lv_obj_set_x(charging_percent, 60);
        lv_obj_set_y(charging_percent, 278);
        lv_obj_set_width(charging_percent, 270);
        lv_obj_set_height(charging_percent, 48);
        lv_label_set_text(charging_percent, "~59%");
        lv_obj_set_style_text_font(charging_percent, value_36, 0);
        lv_obj_set_style_text_align(charging_percent, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * charging_state = lv_label_create(charging_panel);
        lv_obj_set_name(charging_state, "charging_state");
        lv_obj_set_x(charging_state, 60);
        lv_obj_set_y(charging_state, 333);
        lv_obj_set_width(charging_state, 270);
        lv_label_set_text(charging_state, "充电中");
        lv_obj_set_style_text_font(charging_state, notification_22, 0);
        lv_obj_set_style_text_align(charging_state, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * charging_hint = lv_label_create(charging_panel);
        lv_obj_set_name(charging_hint, "charging_hint");
        lv_obj_set_x(charging_hint, 60);
        lv_obj_set_y(charging_hint, 368);
        lv_obj_set_width(charging_hint, 270);
        lv_label_set_text(charging_hint, "估算电量");
        lv_obj_set_style_text_font(charging_hint, notification_22, 0);
        lv_obj_set_style_text_align(charging_hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(charging_hint, lv_color_hex(0xa7adb5), 0);

        the_root = charging_panel;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

