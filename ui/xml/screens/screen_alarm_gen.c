/**
 * @file screen_alarm_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_alarm_gen.h"
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

lv_obj_t * screen_alarm_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t alarm_wheel_selected;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&alarm_wheel_selected);

        lv_style_set_bg_opa(&alarm_wheel_selected, (255 * 0 / 100));
        lv_style_set_text_color(&alarm_wheel_selected, FG_PRIMARY);
        lv_style_set_text_font(&alarm_wheel_selected, metric_56);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_alarm_#");
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

        lv_obj_t * app_back = lv_button_create(lv_obj_0);
        lv_obj_set_name(app_back, "app_back");
        lv_obj_set_x(app_back, 18);
        lv_obj_set_y(app_back, 14);
        lv_obj_set_width(app_back, 222);
        lv_obj_set_height(app_back, 52);
        lv_obj_set_style_bg_opa(app_back, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(app_back, 0, 0);
        lv_obj_set_style_shadow_width(app_back, 0, 0);
        lv_obj_set_style_pad_all(app_back, 0, 0);
        lv_obj_t * alarm_back_icon = lv_label_create(app_back);
        lv_obj_set_name(alarm_back_icon, "alarm_back_icon");
        lv_obj_set_x(alarm_back_icon, 0);
        lv_obj_set_y(alarm_back_icon, 15);
        lv_obj_set_flag(alarm_back_icon, LV_OBJ_FLAG_HIDDEN, true);
        lv_label_set_text(alarm_back_icon, "");
        lv_obj_set_style_text_font(alarm_back_icon, icons_20, 0);

        lv_obj_t * alarm_title = lv_label_create(app_back);
        lv_obj_set_name(alarm_title, "alarm_title");
        lv_obj_set_x(alarm_title, 6);
        lv_obj_set_y(alarm_title, 12);
        lv_label_set_text(alarm_title, "闹钟");
        lv_obj_set_style_text_font(alarm_title, title_24, 0);

        lv_obj_t * alarm_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(alarm_clock, "alarm_clock");
        lv_obj_set_x(alarm_clock, 270);
        lv_obj_set_y(alarm_clock, 26);
        lv_obj_set_width(alarm_clock, 96);
        lv_label_set_text(alarm_clock, "--:--");
        lv_obj_set_style_text_font(alarm_clock, title_24, 0);
        lv_obj_set_style_text_align(alarm_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * alarm_list_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(alarm_list_panel, "alarm_list_panel");
        lv_obj_set_x(alarm_list_panel, 0);
        lv_obj_set_y(alarm_list_panel, 78);
        lv_obj_set_width(alarm_list_panel, 390);
        lv_obj_set_height(alarm_list_panel, 350);
        lv_obj_set_flag(alarm_list_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(alarm_list_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_list_panel, 0, 0);
        lv_obj_set_style_pad_all(alarm_list_panel, 0, 0);
        lv_obj_t * alarm_list = lv_obj_create(alarm_list_panel);
        lv_obj_set_name(alarm_list, "alarm_list");
        lv_obj_set_x(alarm_list, 24);
        lv_obj_set_y(alarm_list, 2);
        lv_obj_set_width(alarm_list, 342);
        lv_obj_set_height(alarm_list, 350);
        lv_obj_set_scrollbar_mode(alarm_list, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(alarm_list, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_list, 0, 0);
        lv_obj_set_style_pad_all(alarm_list, 0, 0);
        lv_obj_set_style_pad_bottom(alarm_list, 16, 0);
        lv_obj_set_style_layout(alarm_list, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(alarm_list, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_style_flex_cross_place(alarm_list, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_row(alarm_list, 8, 0);
        lv_obj_t * alarm_empty_group = lv_obj_create(alarm_list);
        lv_obj_set_name(alarm_empty_group, "alarm_empty_group");
        lv_obj_set_width(alarm_empty_group, 342);
        lv_obj_set_height(alarm_empty_group, 269);
        lv_obj_set_flag(alarm_empty_group, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(alarm_empty_group, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_empty_group, 0, 0);
        lv_obj_set_style_pad_all(alarm_empty_group, 0, 0);
        lv_obj_t * alarm_empty_icon = lv_label_create(alarm_empty_group);
        lv_obj_set_name(alarm_empty_icon, "alarm_empty_icon");
        lv_obj_set_x(alarm_empty_icon, 127);
        lv_obj_set_y(alarm_empty_icon, 70);
        lv_obj_set_width(alarm_empty_icon, 88);
        lv_label_set_text(alarm_empty_icon, "");
        lv_obj_set_style_text_font(alarm_empty_icon, icons_44, 0);
        lv_obj_set_style_text_color(alarm_empty_icon, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_text_align(alarm_empty_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_empty = lv_label_create(alarm_empty_group);
        lv_obj_set_name(alarm_empty, "alarm_empty");
        lv_obj_set_x(alarm_empty, 0);
        lv_obj_set_y(alarm_empty, 154);
        lv_obj_set_width(alarm_empty, 342);
        lv_label_set_text(alarm_empty, "暂无闹钟");
        lv_obj_set_style_text_font(alarm_empty, title_24, 0);
        lv_obj_set_style_text_color(alarm_empty, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(alarm_empty, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_row_0 = alarm_item_create(alarm_list);
        lv_obj_set_name(alarm_row_0, "alarm_row_0");
        lv_obj_set_flag(alarm_row_0, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * alarm_row_1 = alarm_item_create(alarm_list);
        lv_obj_set_name(alarm_row_1, "alarm_row_1");
        lv_obj_set_flag(alarm_row_1, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * alarm_row_2 = alarm_item_create(alarm_list);
        lv_obj_set_name(alarm_row_2, "alarm_row_2");
        lv_obj_set_flag(alarm_row_2, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * alarm_row_3 = alarm_item_create(alarm_list);
        lv_obj_set_name(alarm_row_3, "alarm_row_3");
        lv_obj_set_flag(alarm_row_3, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * alarm_row_4 = alarm_item_create(alarm_list);
        lv_obj_set_name(alarm_row_4, "alarm_row_4");
        lv_obj_set_flag(alarm_row_4, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * alarm_list_status = lv_label_create(alarm_list);
        lv_obj_set_name(alarm_list_status, "alarm_list_status");
        lv_obj_set_width(alarm_list_status, 342);
        lv_obj_set_height(alarm_list_status, 32);
        lv_obj_set_flag(alarm_list_status, LV_OBJ_FLAG_HIDDEN, true);
        lv_label_set_text(alarm_list_status, "等待校时");
        lv_obj_set_style_text_color(alarm_list_status, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(alarm_list_status, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_limit = lv_label_create(alarm_list);
        lv_obj_set_name(alarm_limit, "alarm_limit");
        lv_obj_set_width(alarm_limit, 342);
        lv_obj_set_height(alarm_limit, 32);
        lv_obj_set_flag(alarm_limit, LV_OBJ_FLAG_HIDDEN, true);
        lv_label_set_text(alarm_limit, "最多只能设置5个闹钟");
        lv_obj_set_style_text_color(alarm_limit, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(alarm_limit, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_add = lv_button_create(alarm_list);
        lv_obj_set_name(alarm_add, "alarm_add");
        lv_obj_set_width(alarm_add, 150);
        lv_obj_set_height(alarm_add, 66);
        lv_obj_set_style_bg_color(alarm_add, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_radius(alarm_add, 33, 0);
        lv_obj_set_style_shadow_width(alarm_add, 0, 0);
        lv_obj_set_style_pad_all(alarm_add, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(alarm_add);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_44, 0);

        lv_obj_t * alarm_time_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(alarm_time_panel, "alarm_time_panel");
        lv_obj_set_x(alarm_time_panel, 0);
        lv_obj_set_y(alarm_time_panel, 78);
        lv_obj_set_width(alarm_time_panel, 390);
        lv_obj_set_height(alarm_time_panel, 350);
        lv_obj_set_flag(alarm_time_panel, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(alarm_time_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(alarm_time_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_time_panel, 0, 0);
        lv_obj_set_style_pad_all(alarm_time_panel, 0, 0);
        lv_obj_t * alarm_hours = lv_roller_create(alarm_time_panel);
        lv_obj_set_name(alarm_hours, "alarm_hours");
        lv_obj_set_x(alarm_hours, 60);
        lv_obj_set_y(alarm_hours, 46);
        lv_obj_set_width(alarm_hours, 110);
        lv_roller_set_options(alarm_hours, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23", LV_ROLLER_MODE_INFINITE);
        lv_obj_set_style_bg_opa(alarm_hours, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_hours, 0, 0);
        lv_obj_set_style_radius(alarm_hours, 0, 0);
        lv_obj_set_style_text_font(alarm_hours, value_36, 0);
        lv_obj_set_style_text_color(alarm_hours, lv_color_hex(0x737980), 0);
        lv_obj_set_style_text_line_space(alarm_hours, 22, 0);
        lv_obj_set_style_text_align(alarm_hours, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(alarm_hours, 3);
        lv_obj_add_style(alarm_hours, &alarm_wheel_selected, LV_PART_SELECTED);

        lv_obj_t * alarm_minutes = lv_roller_create(alarm_time_panel);
        lv_obj_set_name(alarm_minutes, "alarm_minutes");
        lv_obj_set_x(alarm_minutes, 220);
        lv_obj_set_y(alarm_minutes, 46);
        lv_obj_set_width(alarm_minutes, 110);
        lv_roller_set_options(alarm_minutes, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_INFINITE);
        lv_obj_set_style_bg_opa(alarm_minutes, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_minutes, 0, 0);
        lv_obj_set_style_radius(alarm_minutes, 0, 0);
        lv_obj_set_style_text_font(alarm_minutes, value_36, 0);
        lv_obj_set_style_text_color(alarm_minutes, lv_color_hex(0x737980), 0);
        lv_obj_set_style_text_line_space(alarm_minutes, 22, 0);
        lv_obj_set_style_text_align(alarm_minutes, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(alarm_minutes, 3);
        lv_obj_add_style(alarm_minutes, &alarm_wheel_selected, LV_PART_SELECTED);

        lv_obj_t * alarm_time_separator = lv_label_create(alarm_time_panel);
        lv_obj_set_name(alarm_time_separator, "alarm_time_separator");
        lv_obj_set_x(alarm_time_separator, 180);
        lv_obj_set_y(alarm_time_separator, 121);
        lv_obj_set_width(alarm_time_separator, 30);
        lv_label_set_text(alarm_time_separator, ":");
        lv_obj_set_style_text_font(alarm_time_separator, value_36, 0);
        lv_obj_set_style_text_color(alarm_time_separator, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(alarm_time_separator, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_time_accept = lv_button_create(alarm_time_panel);
        lv_obj_set_name(alarm_time_accept, "alarm_time_accept");
        lv_obj_set_x(alarm_time_accept, 120);
        lv_obj_set_y(alarm_time_accept, 279);
        lv_obj_set_width(alarm_time_accept, 150);
        lv_obj_set_height(alarm_time_accept, 66);
        lv_obj_set_style_bg_color(alarm_time_accept, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_radius(alarm_time_accept, 33, 0);
        lv_obj_set_style_shadow_width(alarm_time_accept, 0, 0);
        lv_obj_set_style_pad_all(alarm_time_accept, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(alarm_time_accept);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_44, 0);

        lv_obj_t * alarm_settings_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(alarm_settings_panel, "alarm_settings_panel");
        lv_obj_set_x(alarm_settings_panel, 0);
        lv_obj_set_y(alarm_settings_panel, 78);
        lv_obj_set_width(alarm_settings_panel, 390);
        lv_obj_set_height(alarm_settings_panel, 350);
        lv_obj_set_flag(alarm_settings_panel, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(alarm_settings_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(alarm_settings_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_settings_panel, 0, 0);
        lv_obj_set_style_pad_all(alarm_settings_panel, 0, 0);
        lv_obj_t * alarm_edit_time = lv_button_create(alarm_settings_panel);
        lv_obj_set_name(alarm_edit_time, "alarm_edit_time");
        lv_obj_set_x(alarm_edit_time, 24);
        lv_obj_set_y(alarm_edit_time, 2);
        lv_obj_set_width(alarm_edit_time, 342);
        lv_obj_set_height(alarm_edit_time, 104);
        lv_obj_set_style_bg_color(alarm_edit_time, lv_color_hex(0x202124), 0);
        lv_obj_set_style_radius(alarm_edit_time, 24, 0);
        lv_obj_set_style_shadow_width(alarm_edit_time, 0, 0);
        lv_obj_set_style_pad_all(alarm_edit_time, 0, 0);
        lv_obj_t * lv_label_2 = lv_label_create(alarm_edit_time);
        lv_obj_set_x(lv_label_2, 20);
        lv_obj_set_y(lv_label_2, 12);
        lv_label_set_text(lv_label_2, "时间设置");
        lv_obj_set_style_text_font(lv_label_2, title_24, 0);

        lv_obj_t * alarm_draft_time = lv_label_create(alarm_edit_time);
        lv_obj_set_name(alarm_draft_time, "alarm_draft_time");
        lv_obj_set_x(alarm_draft_time, 20);
        lv_obj_set_y(alarm_draft_time, 54);
        lv_label_set_text(alarm_draft_time, "07:00");
        lv_obj_set_style_text_font(alarm_draft_time, value_36, 0);

        lv_obj_t * alarm_edit_repeat = lv_button_create(alarm_settings_panel);
        lv_obj_set_name(alarm_edit_repeat, "alarm_edit_repeat");
        lv_obj_set_x(alarm_edit_repeat, 24);
        lv_obj_set_y(alarm_edit_repeat, 120);
        lv_obj_set_width(alarm_edit_repeat, 342);
        lv_obj_set_height(alarm_edit_repeat, 104);
        lv_obj_set_style_bg_color(alarm_edit_repeat, lv_color_hex(0x202124), 0);
        lv_obj_set_style_radius(alarm_edit_repeat, 24, 0);
        lv_obj_set_style_shadow_width(alarm_edit_repeat, 0, 0);
        lv_obj_set_style_pad_all(alarm_edit_repeat, 0, 0);
        lv_obj_t * lv_label_3 = lv_label_create(alarm_edit_repeat);
        lv_obj_set_x(lv_label_3, 20);
        lv_obj_set_y(lv_label_3, 12);
        lv_label_set_text(lv_label_3, "重复");
        lv_obj_set_style_text_font(lv_label_3, title_24, 0);

        lv_obj_t * alarm_draft_rule = lv_label_create(alarm_edit_repeat);
        lv_obj_set_name(alarm_draft_rule, "alarm_draft_rule");
        lv_obj_set_x(alarm_draft_rule, 20);
        lv_obj_set_y(alarm_draft_rule, 57);
        lv_obj_set_width(alarm_draft_rule, 302);
        lv_label_set_text(alarm_draft_rule, "仅一次");
        lv_obj_set_style_text_font(alarm_draft_rule, body_20, 0);
        lv_obj_set_style_text_color(alarm_draft_rule, FG_SECONDARY, 0);

        lv_obj_t * alarm_delete = lv_button_create(alarm_settings_panel);
        lv_obj_set_name(alarm_delete, "alarm_delete");
        lv_obj_set_x(alarm_delete, 42);
        lv_obj_set_y(alarm_delete, 279);
        lv_obj_set_width(alarm_delete, 144);
        lv_obj_set_height(alarm_delete, 66);
        lv_obj_set_style_bg_color(alarm_delete, lv_color_hex(0x202124), 0);
        lv_obj_set_style_radius(alarm_delete, 33, 0);
        lv_obj_set_style_shadow_width(alarm_delete, 0, 0);
        lv_obj_set_style_pad_all(alarm_delete, 0, 0);
        lv_obj_t * lv_label_4 = lv_label_create(alarm_delete);
        lv_obj_set_align(lv_label_4, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_4, "");
        lv_obj_set_style_text_font(lv_label_4, icons_44, 0);

        lv_obj_t * alarm_save = lv_button_create(alarm_settings_panel);
        lv_obj_set_name(alarm_save, "alarm_save");
        lv_obj_set_x(alarm_save, 204);
        lv_obj_set_y(alarm_save, 279);
        lv_obj_set_width(alarm_save, 144);
        lv_obj_set_height(alarm_save, 66);
        lv_obj_set_style_bg_color(alarm_save, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_radius(alarm_save, 33, 0);
        lv_obj_set_style_shadow_width(alarm_save, 0, 0);
        lv_obj_set_style_pad_all(alarm_save, 0, 0);
        lv_obj_t * lv_label_5 = lv_label_create(alarm_save);
        lv_obj_set_align(lv_label_5, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_5, "");
        lv_obj_set_style_text_font(lv_label_5, icons_44, 0);

        lv_obj_t * alarm_repeat_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(alarm_repeat_panel, "alarm_repeat_panel");
        lv_obj_set_x(alarm_repeat_panel, 0);
        lv_obj_set_y(alarm_repeat_panel, 78);
        lv_obj_set_width(alarm_repeat_panel, 390);
        lv_obj_set_height(alarm_repeat_panel, 350);
        lv_obj_set_flag(alarm_repeat_panel, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(alarm_repeat_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(alarm_repeat_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_repeat_panel, 0, 0);
        lv_obj_set_style_pad_all(alarm_repeat_panel, 0, 0);
        lv_obj_t * alarm_repeat_scroll = lv_obj_create(alarm_repeat_panel);
        lv_obj_set_name(alarm_repeat_scroll, "alarm_repeat_scroll");
        lv_obj_set_x(alarm_repeat_scroll, 24);
        lv_obj_set_y(alarm_repeat_scroll, 2);
        lv_obj_set_width(alarm_repeat_scroll, 342);
        lv_obj_set_height(alarm_repeat_scroll, 267);
        lv_obj_set_scrollbar_mode(alarm_repeat_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(alarm_repeat_scroll, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_repeat_scroll, 0, 0);
        lv_obj_set_style_pad_all(alarm_repeat_scroll, 0, 0);
        lv_obj_t * alarm_repeat_0 = alarm_option_create(alarm_repeat_scroll, "仅一次");
        lv_obj_set_name(alarm_repeat_0, "alarm_repeat_0");
        lv_obj_set_y(alarm_repeat_0, 0);

        lv_obj_t * alarm_repeat_1 = alarm_option_create(alarm_repeat_scroll, "每天");
        lv_obj_set_name(alarm_repeat_1, "alarm_repeat_1");
        lv_obj_set_y(alarm_repeat_1, 88);

        lv_obj_t * alarm_repeat_2 = alarm_option_create(alarm_repeat_scroll, "工作日（周一至五）");
        lv_obj_set_name(alarm_repeat_2, "alarm_repeat_2");
        lv_obj_set_y(alarm_repeat_2, 176);

        lv_obj_t * alarm_repeat_3 = alarm_option_create(alarm_repeat_scroll, "周末（周六、日）");
        lv_obj_set_name(alarm_repeat_3, "alarm_repeat_3");
        lv_obj_set_y(alarm_repeat_3, 264);

        lv_obj_t * alarm_repeat_4 = alarm_option_create(alarm_repeat_scroll, "自定义");
        lv_obj_set_name(alarm_repeat_4, "alarm_repeat_4");
        lv_obj_set_y(alarm_repeat_4, 352);

        lv_obj_t * alarm_repeat_accept = lv_button_create(alarm_repeat_panel);
        lv_obj_set_name(alarm_repeat_accept, "alarm_repeat_accept");
        lv_obj_set_x(alarm_repeat_accept, 120);
        lv_obj_set_y(alarm_repeat_accept, 279);
        lv_obj_set_width(alarm_repeat_accept, 150);
        lv_obj_set_height(alarm_repeat_accept, 66);
        lv_obj_set_style_bg_color(alarm_repeat_accept, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_radius(alarm_repeat_accept, 33, 0);
        lv_obj_set_style_shadow_width(alarm_repeat_accept, 0, 0);
        lv_obj_set_style_pad_all(alarm_repeat_accept, 0, 0);
        lv_obj_t * lv_label_6 = lv_label_create(alarm_repeat_accept);
        lv_obj_set_align(lv_label_6, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_6, "");
        lv_obj_set_style_text_font(lv_label_6, icons_44, 0);

        lv_obj_t * alarm_weekdays_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(alarm_weekdays_panel, "alarm_weekdays_panel");
        lv_obj_set_x(alarm_weekdays_panel, 0);
        lv_obj_set_y(alarm_weekdays_panel, 78);
        lv_obj_set_width(alarm_weekdays_panel, 390);
        lv_obj_set_height(alarm_weekdays_panel, 350);
        lv_obj_set_flag(alarm_weekdays_panel, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(alarm_weekdays_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(alarm_weekdays_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_weekdays_panel, 0, 0);
        lv_obj_set_style_pad_all(alarm_weekdays_panel, 0, 0);
        lv_obj_t * alarm_day_scroll = lv_obj_create(alarm_weekdays_panel);
        lv_obj_set_name(alarm_day_scroll, "alarm_day_scroll");
        lv_obj_set_x(alarm_day_scroll, 24);
        lv_obj_set_y(alarm_day_scroll, 2);
        lv_obj_set_width(alarm_day_scroll, 342);
        lv_obj_set_height(alarm_day_scroll, 267);
        lv_obj_set_scrollbar_mode(alarm_day_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(alarm_day_scroll, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_day_scroll, 0, 0);
        lv_obj_set_style_pad_all(alarm_day_scroll, 0, 0);
        lv_obj_t * alarm_day_0 = alarm_option_create(alarm_day_scroll, "周一");
        lv_obj_set_name(alarm_day_0, "alarm_day_0");
        lv_obj_set_y(alarm_day_0, 0);

        lv_obj_t * alarm_day_1 = alarm_option_create(alarm_day_scroll, "周二");
        lv_obj_set_name(alarm_day_1, "alarm_day_1");
        lv_obj_set_y(alarm_day_1, 88);

        lv_obj_t * alarm_day_2 = alarm_option_create(alarm_day_scroll, "周三");
        lv_obj_set_name(alarm_day_2, "alarm_day_2");
        lv_obj_set_y(alarm_day_2, 176);

        lv_obj_t * alarm_day_3 = alarm_option_create(alarm_day_scroll, "周四");
        lv_obj_set_name(alarm_day_3, "alarm_day_3");
        lv_obj_set_y(alarm_day_3, 264);

        lv_obj_t * alarm_day_4 = alarm_option_create(alarm_day_scroll, "周五");
        lv_obj_set_name(alarm_day_4, "alarm_day_4");
        lv_obj_set_y(alarm_day_4, 352);

        lv_obj_t * alarm_day_5 = alarm_option_create(alarm_day_scroll, "周六");
        lv_obj_set_name(alarm_day_5, "alarm_day_5");
        lv_obj_set_y(alarm_day_5, 440);

        lv_obj_t * alarm_day_6 = alarm_option_create(alarm_day_scroll, "周日");
        lv_obj_set_name(alarm_day_6, "alarm_day_6");
        lv_obj_set_y(alarm_day_6, 528);

        lv_obj_t * alarm_days_accept = lv_button_create(alarm_weekdays_panel);
        lv_obj_set_name(alarm_days_accept, "alarm_days_accept");
        lv_obj_set_x(alarm_days_accept, 120);
        lv_obj_set_y(alarm_days_accept, 279);
        lv_obj_set_width(alarm_days_accept, 150);
        lv_obj_set_height(alarm_days_accept, 66);
        lv_obj_set_style_bg_color(alarm_days_accept, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_radius(alarm_days_accept, 33, 0);
        lv_obj_set_style_shadow_width(alarm_days_accept, 0, 0);
        lv_obj_set_style_pad_all(alarm_days_accept, 0, 0);
        lv_obj_t * lv_label_7 = lv_label_create(alarm_days_accept);
        lv_obj_set_align(lv_label_7, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_7, "");
        lv_obj_set_style_text_font(lv_label_7, icons_44, 0);

        lv_obj_t * alarm_error = lv_label_create(lv_obj_0);
        lv_obj_set_name(alarm_error, "alarm_error");
        lv_obj_set_x(alarm_error, 24);
        lv_obj_set_y(alarm_error, 330);
        lv_obj_set_width(alarm_error, 342);
        lv_obj_set_flag(alarm_error, LV_OBJ_FLAG_HIDDEN, true);
        lv_label_set_text(alarm_error, "保存失败");
        lv_obj_set_style_text_color(alarm_error, ACCENT_RED, 0);
        lv_obj_set_style_text_align(alarm_error, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * alarm_status = lv_label_create(lv_obj_0);
        lv_obj_set_name(alarm_status, "alarm_status");
        lv_obj_set_x(alarm_status, 24);
        lv_obj_set_y(alarm_status, 330);
        lv_obj_set_width(alarm_status, 342);
        lv_obj_set_flag(alarm_status, LV_OBJ_FLAG_HIDDEN, true);
        lv_label_set_text(alarm_status, "等待校时");
        lv_obj_set_style_text_color(alarm_status, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(alarm_status, LV_TEXT_ALIGN_CENTER, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

