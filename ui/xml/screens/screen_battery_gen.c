/**
 * @file screen_battery_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_battery_gen.h"
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

lv_obj_t * screen_battery_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * battery_page = lv_obj_create(NULL);
        lv_obj_set_name_static(battery_page, "screen_battery_#");
        lv_obj_set_name(battery_page, "battery_page");
        lv_obj_set_width(battery_page, 390);
        lv_obj_set_height(battery_page, 450);
        lv_obj_set_flag(battery_page, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(battery_page, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(battery_page, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(battery_page, 0, 0);
        lv_obj_set_style_pad_all(battery_page, 0, 0);
        lv_obj_set_style_radius(battery_page, 0, 0);
        lv_obj_set_style_text_color(battery_page, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(battery_page, notification_22, 0);

        app_header_create(battery_page, "电池");

        lv_obj_t * battery_time = lv_label_create(battery_page);
        lv_obj_set_name(battery_time, "battery_time");
        lv_obj_set_x(battery_time, 260);
        lv_obj_set_y(battery_time, 30);
        lv_obj_set_width(battery_time, 106);
        lv_obj_set_height(battery_time, 28);
        lv_label_set_text(battery_time, "21:12");
        lv_obj_set_style_text_font(battery_time, body_20, 0);
        lv_obj_set_style_text_align(battery_time, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * battery_badge = lv_obj_create(battery_page);
        lv_obj_set_name(battery_badge, "battery_badge");
        lv_obj_set_x(battery_badge, 28);
        lv_obj_set_y(battery_badge, 128);
        lv_obj_set_width(battery_badge, 54);
        lv_obj_set_height(battery_badge, 54);
        lv_obj_set_flag(battery_badge, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(battery_badge, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(battery_badge, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(battery_badge, 3, 0);
        lv_obj_set_style_border_color(battery_badge, lv_color_hex(0x00dfc0), 0);
        lv_obj_set_style_radius(battery_badge, 27, 0);
        lv_obj_set_style_pad_all(battery_badge, 0, 0);
        lv_obj_t * battery_icon = lv_label_create(battery_badge);
        lv_obj_set_name(battery_icon, "battery_icon");
        lv_obj_set_align(battery_icon, LV_ALIGN_CENTER);
        lv_label_set_text(battery_icon, "");
        lv_obj_set_style_text_font(battery_icon, icons_20, 0);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00dfc0), 0);

        lv_obj_t * battery_percent = lv_label_create(battery_page);
        lv_obj_set_name(battery_percent, "battery_percent");
        lv_obj_set_x(battery_percent, 94);
        lv_obj_set_y(battery_percent, 96);
        lv_obj_set_width(battery_percent, 272);
        lv_obj_set_height(battery_percent, 112);
        lv_label_set_text(battery_percent, "~91%");
        lv_obj_set_style_text_font(battery_percent, battery_80, 0);

        lv_obj_t * battery_status = lv_label_create(battery_page);
        lv_obj_set_name(battery_status, "battery_status");
        lv_obj_set_x(battery_status, 24);
        lv_obj_set_y(battery_status, 214);
        lv_obj_set_width(battery_status, 342);
        lv_obj_set_height(battery_status, 30);
        lv_label_set_text(battery_status, "充电中");
        lv_obj_set_style_text_align(battery_status, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(battery_status, lv_color_hex(0x00dfc0), 0);

        lv_obj_t * battery_reason = lv_label_create(battery_page);
        lv_obj_set_name(battery_reason, "battery_reason");
        lv_obj_set_x(battery_reason, 24);
        lv_obj_set_y(battery_reason, 248);
        lv_obj_set_width(battery_reason, 342);
        lv_obj_set_height(battery_reason, 30);
        lv_label_set_text(battery_reason, "");
        lv_obj_set_style_text_align(battery_reason, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(battery_reason, FG_SECONDARY, 0);

        lv_obj_t * battery_details = lv_obj_create(battery_page);
        lv_obj_set_name(battery_details, "battery_details");
        lv_obj_set_x(battery_details, 24);
        lv_obj_set_y(battery_details, 285);
        lv_obj_set_width(battery_details, 342);
        lv_obj_set_height(battery_details, 98);
        lv_obj_set_flag(battery_details, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(battery_details, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(battery_details, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(battery_details, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(battery_details, 0, 0);
        lv_obj_set_style_pad_all(battery_details, 0, 0);
        lv_obj_set_style_radius(battery_details, 20, 0);
        lv_obj_set_style_text_color(battery_details, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(battery_details, notification_22, 0);
        lv_obj_t * lv_label_0 = lv_label_create(battery_details);
        lv_obj_set_x(lv_label_0, 18);
        lv_obj_set_y(lv_label_0, 18);
        lv_label_set_text(lv_label_0, "电压");

        lv_obj_t * battery_voltage = lv_label_create(battery_details);
        lv_obj_set_name(battery_voltage, "battery_voltage");
        lv_obj_set_x(battery_voltage, 140);
        lv_obj_set_y(battery_voltage, 6);
        lv_obj_set_width(battery_voltage, 184);
        lv_obj_set_height(battery_voltage, 48);
        lv_label_set_text(battery_voltage, "4.04V");
        lv_obj_set_style_text_font(battery_voltage, value_36, 0);
        lv_obj_set_style_text_align(battery_voltage, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * battery_input = lv_label_create(battery_details);
        lv_obj_set_name(battery_input, "battery_input");
        lv_obj_set_x(battery_input, 18);
        lv_obj_set_y(battery_input, 61);
        lv_obj_set_width(battery_input, 306);
        lv_obj_set_height(battery_input, 30);
        lv_label_set_text(battery_input, "USB 输入有效");
        lv_obj_set_style_text_color(battery_input, FG_SECONDARY, 0);

        lv_obj_t * battery_source = lv_label_create(battery_page);
        lv_obj_set_name(battery_source, "battery_source");
        lv_obj_set_x(battery_source, 24);
        lv_obj_set_y(battery_source, 402);
        lv_obj_set_width(battery_source, 342);
        lv_obj_set_height(battery_source, 30);
        lv_label_set_text(battery_source, "电压估算，仅供参考");
        lv_obj_set_style_text_align(battery_source, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(battery_source, FG_SECONDARY, 0);

        the_root = battery_page;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

