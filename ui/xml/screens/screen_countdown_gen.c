/**
 * @file screen_countdown_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_countdown_gen.h"
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

lv_obj_t * screen_countdown_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t wheel_selected;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&wheel_selected);

        lv_style_set_bg_opa(&wheel_selected, (255 * 0 / 100));
        lv_style_set_text_color(&wheel_selected, FG_PRIMARY);
        lv_style_set_text_font(&wheel_selected, value_36);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_countdown_#");
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
        lv_obj_set_width(app_back, 218);
        lv_obj_set_height(app_back, 52);
        lv_obj_set_style_bg_opa(app_back, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(app_back, 0, 0);
        lv_obj_set_style_shadow_width(app_back, 0, 0);
        lv_obj_set_style_pad_all(app_back, 0, 0);
        lv_obj_t * countdown_title = lv_label_create(app_back);
        lv_obj_set_name(countdown_title, "countdown_title");
        lv_obj_set_x(countdown_title, 6);
        lv_obj_set_y(countdown_title, 12);
        lv_label_set_text(countdown_title, "倒计时");
        lv_obj_set_style_text_font(countdown_title, title_24, 0);

        lv_obj_t * countdown_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(countdown_clock, "countdown_clock");
        lv_obj_set_x(countdown_clock, 270);
        lv_obj_set_y(countdown_clock, 26);
        lv_obj_set_width(countdown_clock, 96);
        lv_label_set_text(countdown_clock, "13:40");
        lv_obj_set_style_text_font(countdown_clock, title_24, 0);
        lv_obj_set_style_text_align(countdown_clock, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * countdown_picker = lv_obj_create(lv_obj_0);
        lv_obj_set_name(countdown_picker, "countdown_picker");
        lv_obj_set_x(countdown_picker, 0);
        lv_obj_set_y(countdown_picker, 78);
        lv_obj_set_width(countdown_picker, 390);
        lv_obj_set_height(countdown_picker, 350);
        lv_obj_set_flag(countdown_picker, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(countdown_picker, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(countdown_picker, 0, 0);
        lv_obj_set_style_pad_all(countdown_picker, 0, 0);
        lv_obj_t * countdown_preset_1 = countdown_preset_create(countdown_picker, "1");
        lv_obj_set_name(countdown_preset_1, "countdown_preset_1");
        lv_obj_set_x(countdown_preset_1, 24);
        lv_obj_set_y(countdown_preset_1, 16);

        lv_obj_t * countdown_preset_2 = countdown_preset_create(countdown_picker, "2");
        lv_obj_set_name(countdown_preset_2, "countdown_preset_2");
        lv_obj_set_x(countdown_preset_2, 143);
        lv_obj_set_y(countdown_preset_2, 16);

        lv_obj_t * countdown_preset_3 = countdown_preset_create(countdown_picker, "3");
        lv_obj_set_name(countdown_preset_3, "countdown_preset_3");
        lv_obj_set_x(countdown_preset_3, 262);
        lv_obj_set_y(countdown_preset_3, 16);

        lv_obj_t * countdown_preset_5 = countdown_preset_create(countdown_picker, "5");
        lv_obj_set_name(countdown_preset_5, "countdown_preset_5");
        lv_obj_set_x(countdown_preset_5, 24);
        lv_obj_set_y(countdown_preset_5, 140);

        lv_obj_t * countdown_preset_10 = countdown_preset_create(countdown_picker, "10");
        lv_obj_set_name(countdown_preset_10, "countdown_preset_10");
        lv_obj_set_x(countdown_preset_10, 143);
        lv_obj_set_y(countdown_preset_10, 140);

        lv_obj_t * countdown_preset_30 = countdown_preset_create(countdown_picker, "30");
        lv_obj_set_name(countdown_preset_30, "countdown_preset_30");
        lv_obj_set_x(countdown_preset_30, 262);
        lv_obj_set_y(countdown_preset_30, 140);

        lv_obj_t * countdown_custom = lv_button_create(countdown_picker);
        lv_obj_set_name(countdown_custom, "countdown_custom");
        lv_obj_set_x(countdown_custom, 30);
        lv_obj_set_y(countdown_custom, 279);
        lv_obj_set_width(countdown_custom, 330);
        lv_obj_set_height(countdown_custom, 66);
        lv_obj_set_style_radius(countdown_custom, 33, 0);
        lv_obj_set_style_bg_color(countdown_custom, lv_color_hex(0x143d63), 0);
        lv_obj_set_style_shadow_width(countdown_custom, 0, 0);
        lv_obj_set_style_pad_all(countdown_custom, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(countdown_custom);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "自定义");
        lv_obj_set_style_text_font(lv_label_0, title_24, 0);
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0x008cff), 0);

        lv_obj_t * countdown_custom_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(countdown_custom_panel, "countdown_custom_panel");
        lv_obj_set_x(countdown_custom_panel, 0);
        lv_obj_set_y(countdown_custom_panel, 78);
        lv_obj_set_width(countdown_custom_panel, 390);
        lv_obj_set_height(countdown_custom_panel, 350);
        lv_obj_set_flag(countdown_custom_panel, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(countdown_custom_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(countdown_custom_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(countdown_custom_panel, 0, 0);
        lv_obj_set_style_pad_all(countdown_custom_panel, 0, 0);
        lv_obj_t * countdown_hours = lv_roller_create(countdown_custom_panel);
        lv_obj_set_name(countdown_hours, "countdown_hours");
        lv_obj_set_x(countdown_hours, 38);
        lv_obj_set_y(countdown_hours, 46);
        lv_obj_set_width(countdown_hours, 94);
        lv_roller_set_options(countdown_hours, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23", LV_ROLLER_MODE_INFINITE);
        lv_obj_set_style_bg_opa(countdown_hours, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(countdown_hours, 0, 0);
        lv_obj_set_style_radius(countdown_hours, 0, 0);
        lv_obj_set_style_text_font(countdown_hours, value_36, 0);
        lv_obj_set_style_text_color(countdown_hours, lv_color_hex(0x7895b3), 0);
        lv_obj_set_style_text_line_space(countdown_hours, 20, 0);
        lv_obj_set_style_text_align(countdown_hours, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(countdown_hours, 3);
        lv_obj_add_style(countdown_hours, &wheel_selected, LV_PART_SELECTED);

        lv_obj_t * countdown_minutes = lv_roller_create(countdown_custom_panel);
        lv_obj_set_name(countdown_minutes, "countdown_minutes");
        lv_obj_set_x(countdown_minutes, 148);
        lv_obj_set_y(countdown_minutes, 46);
        lv_obj_set_width(countdown_minutes, 94);
        lv_roller_set_options(countdown_minutes, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_INFINITE);
        lv_roller_set_selected(countdown_minutes, 1, false);
        lv_obj_set_style_bg_opa(countdown_minutes, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(countdown_minutes, 0, 0);
        lv_obj_set_style_radius(countdown_minutes, 0, 0);
        lv_obj_set_style_text_font(countdown_minutes, value_36, 0);
        lv_obj_set_style_text_color(countdown_minutes, lv_color_hex(0x7895b3), 0);
        lv_obj_set_style_text_line_space(countdown_minutes, 20, 0);
        lv_obj_set_style_text_align(countdown_minutes, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(countdown_minutes, 3);
        lv_obj_add_style(countdown_minutes, &wheel_selected, LV_PART_SELECTED);

        lv_obj_t * countdown_seconds = lv_roller_create(countdown_custom_panel);
        lv_obj_set_name(countdown_seconds, "countdown_seconds");
        lv_obj_set_x(countdown_seconds, 258);
        lv_obj_set_y(countdown_seconds, 46);
        lv_obj_set_width(countdown_seconds, 94);
        lv_roller_set_options(countdown_seconds, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_INFINITE);
        lv_obj_set_style_bg_opa(countdown_seconds, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(countdown_seconds, 0, 0);
        lv_obj_set_style_radius(countdown_seconds, 0, 0);
        lv_obj_set_style_text_font(countdown_seconds, value_36, 0);
        lv_obj_set_style_text_color(countdown_seconds, lv_color_hex(0x7895b3), 0);
        lv_obj_set_style_text_line_space(countdown_seconds, 20, 0);
        lv_obj_set_style_text_align(countdown_seconds, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(countdown_seconds, 3);
        lv_obj_add_style(countdown_seconds, &wheel_selected, LV_PART_SELECTED);

        lv_obj_t * countdown_separator_1 = lv_label_create(countdown_custom_panel);
        lv_obj_set_name(countdown_separator_1, "countdown_separator_1");
        lv_obj_set_x(countdown_separator_1, 131);
        lv_obj_set_y(countdown_separator_1, 118);
        lv_obj_set_width(countdown_separator_1, 18);
        lv_label_set_text(countdown_separator_1, ":");
        lv_obj_set_style_text_font(countdown_separator_1, value_36, 0);
        lv_obj_set_style_text_align(countdown_separator_1, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * countdown_separator_2 = lv_label_create(countdown_custom_panel);
        lv_obj_set_name(countdown_separator_2, "countdown_separator_2");
        lv_obj_set_x(countdown_separator_2, 241);
        lv_obj_set_y(countdown_separator_2, 118);
        lv_obj_set_width(countdown_separator_2, 18);
        lv_label_set_text(countdown_separator_2, ":");
        lv_obj_set_style_text_font(countdown_separator_2, value_36, 0);
        lv_obj_set_style_text_align(countdown_separator_2, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * countdown_start = lv_button_create(countdown_custom_panel);
        lv_obj_set_name(countdown_start, "countdown_start");
        lv_obj_set_x(countdown_start, 120);
        lv_obj_set_y(countdown_start, 279);
        lv_obj_set_width(countdown_start, 150);
        lv_obj_set_height(countdown_start, 66);
        lv_obj_set_style_bg_color(countdown_start, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_radius(countdown_start, 33, 0);
        lv_obj_set_style_shadow_width(countdown_start, 0, 0);
        lv_obj_set_style_pad_all(countdown_start, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(countdown_start);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_44, 0);

        lv_obj_t * countdown_active_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(countdown_active_panel, "countdown_active_panel");
        lv_obj_set_x(countdown_active_panel, 0);
        lv_obj_set_y(countdown_active_panel, 78);
        lv_obj_set_width(countdown_active_panel, 390);
        lv_obj_set_height(countdown_active_panel, 350);
        lv_obj_set_flag(countdown_active_panel, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(countdown_active_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(countdown_active_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(countdown_active_panel, 0, 0);
        lv_obj_set_style_pad_all(countdown_active_panel, 0, 0);
        lv_obj_t * countdown_remaining = lv_label_create(countdown_active_panel);
        lv_obj_set_name(countdown_remaining, "countdown_remaining");
        lv_obj_set_x(countdown_remaining, 24);
        lv_obj_set_y(countdown_remaining, 93);
        lv_obj_set_width(countdown_remaining, 342);
        lv_label_set_text(countdown_remaining, "00:00:57");
        lv_obj_set_style_text_font(countdown_remaining, metric_56, 0);
        lv_obj_set_style_text_align(countdown_remaining, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * countdown_paused = lv_label_create(countdown_active_panel);
        lv_obj_set_name(countdown_paused, "countdown_paused");
        lv_obj_set_x(countdown_paused, 24);
        lv_obj_set_y(countdown_paused, 162);
        lv_obj_set_width(countdown_paused, 342);
        lv_label_set_text(countdown_paused, "已暂停");
        lv_obj_set_flag(countdown_paused, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_style_text_color(countdown_paused, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(countdown_paused, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * countdown_cancel = lv_button_create(countdown_active_panel);
        lv_obj_set_name(countdown_cancel, "countdown_cancel");
        lv_obj_set_x(countdown_cancel, 42);
        lv_obj_set_y(countdown_cancel, 279);
        lv_obj_set_width(countdown_cancel, 144);
        lv_obj_set_height(countdown_cancel, 66);
        lv_obj_set_style_radius(countdown_cancel, 33, 0);
        lv_obj_set_style_bg_color(countdown_cancel, lv_color_hex(0x143d63), 0);
        lv_obj_set_style_shadow_width(countdown_cancel, 0, 0);
        lv_obj_set_style_pad_all(countdown_cancel, 0, 0);
        lv_obj_t * lv_label_2 = lv_label_create(countdown_cancel);
        lv_obj_set_align(lv_label_2, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_2, "");
        lv_obj_set_style_text_font(lv_label_2, icons_44, 0);

        lv_obj_t * countdown_toggle = lv_button_create(countdown_active_panel);
        lv_obj_set_name(countdown_toggle, "countdown_toggle");
        lv_obj_set_x(countdown_toggle, 204);
        lv_obj_set_y(countdown_toggle, 279);
        lv_obj_set_width(countdown_toggle, 144);
        lv_obj_set_height(countdown_toggle, 66);
        lv_obj_set_style_radius(countdown_toggle, 33, 0);
        lv_obj_set_style_bg_color(countdown_toggle, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_shadow_width(countdown_toggle, 0, 0);
        lv_obj_set_style_pad_all(countdown_toggle, 0, 0);
        lv_obj_t * countdown_toggle_icon = lv_label_create(countdown_toggle);
        lv_obj_set_name(countdown_toggle_icon, "countdown_toggle_icon");
        lv_obj_set_align(countdown_toggle_icon, LV_ALIGN_CENTER);
        lv_label_set_text(countdown_toggle_icon, "");
        lv_obj_set_style_text_font(countdown_toggle_icon, icons_44, 0);

        lv_obj_t * countdown_alert_panel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(countdown_alert_panel, "countdown_alert_panel");
        lv_obj_set_x(countdown_alert_panel, 0);
        lv_obj_set_y(countdown_alert_panel, 78);
        lv_obj_set_width(countdown_alert_panel, 390);
        lv_obj_set_height(countdown_alert_panel, 350);
        lv_obj_set_flag(countdown_alert_panel, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(countdown_alert_panel, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(countdown_alert_panel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(countdown_alert_panel, 0, 0);
        lv_obj_set_style_pad_all(countdown_alert_panel, 0, 0);
        lv_obj_t * lv_label_3 = lv_label_create(countdown_alert_panel);
        lv_obj_set_x(lv_label_3, 24);
        lv_obj_set_y(lv_label_3, 81);
        lv_obj_set_width(lv_label_3, 342);
        lv_label_set_text(lv_label_3, "计时结束");
        lv_obj_set_style_text_font(lv_label_3, value_36, 0);
        lv_obj_set_style_text_align(lv_label_3, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * countdown_original = lv_label_create(countdown_alert_panel);
        lv_obj_set_name(countdown_original, "countdown_original");
        lv_obj_set_x(countdown_original, 24);
        lv_obj_set_y(countdown_original, 134);
        lv_obj_set_width(countdown_original, 342);
        lv_label_set_text(countdown_original, "00:01:00");
        lv_obj_set_style_text_font(countdown_original, title_24, 0);
        lv_obj_set_style_text_align(countdown_original, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * countdown_close = lv_button_create(countdown_alert_panel);
        lv_obj_set_name(countdown_close, "countdown_close");
        lv_obj_set_x(countdown_close, 42);
        lv_obj_set_y(countdown_close, 279);
        lv_obj_set_width(countdown_close, 144);
        lv_obj_set_height(countdown_close, 66);
        lv_obj_set_style_radius(countdown_close, 33, 0);
        lv_obj_set_style_bg_color(countdown_close, lv_color_hex(0x143d63), 0);
        lv_obj_set_style_shadow_width(countdown_close, 0, 0);
        lv_obj_set_style_pad_all(countdown_close, 0, 0);
        lv_obj_t * lv_label_4 = lv_label_create(countdown_close);
        lv_obj_set_align(lv_label_4, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_4, "");
        lv_obj_set_style_text_font(lv_label_4, icons_44, 0);

        lv_obj_t * countdown_repeat = lv_button_create(countdown_alert_panel);
        lv_obj_set_name(countdown_repeat, "countdown_repeat");
        lv_obj_set_x(countdown_repeat, 204);
        lv_obj_set_y(countdown_repeat, 279);
        lv_obj_set_width(countdown_repeat, 144);
        lv_obj_set_height(countdown_repeat, 66);
        lv_obj_set_style_radius(countdown_repeat, 33, 0);
        lv_obj_set_style_bg_color(countdown_repeat, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_shadow_width(countdown_repeat, 0, 0);
        lv_obj_set_style_pad_all(countdown_repeat, 0, 0);
        lv_obj_t * lv_label_5 = lv_label_create(countdown_repeat);
        lv_obj_set_align(lv_label_5, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_5, "");
        lv_obj_set_style_text_font(lv_label_5, icons_44, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

