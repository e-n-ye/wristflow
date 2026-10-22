/**
 * @file screen_control_center_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_control_center_gen.h"
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

lv_obj_t * screen_control_center_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_control_center_#");
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

        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 170);
        lv_obj_set_y(lv_obj_1, 17);
        lv_obj_set_width(lv_obj_1, 50);
        lv_obj_set_height(lv_obj_1, 4);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_1, lv_color_hex(0x777777), 0);
        lv_obj_set_style_bg_opa(lv_obj_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_radius(lv_obj_1, 2, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 24);
        lv_obj_set_y(lv_obj_2, 36);
        lv_obj_set_width(lv_obj_2, 68);
        lv_obj_set_height(lv_obj_2, 28);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_2, (255 * 0 / 100), 0);
        lv_obj_set_style_border_color(lv_obj_2, FG_PRIMARY, 0);
        lv_obj_set_style_border_width(lv_obj_2, 2, 0);
        lv_obj_set_style_radius(lv_obj_2, 6, 0);
        lv_obj_set_style_pad_all(lv_obj_2, 0, 0);
        lv_obj_t * control_battery_value = lv_label_create(lv_obj_2);
        lv_obj_set_name(control_battery_value, "control_battery_value");
        lv_obj_set_align(control_battery_value, LV_ALIGN_CENTER);
        lv_label_set_text(control_battery_value, "~98%");
        lv_obj_set_style_text_font(control_battery_value, body_20, 0);
        lv_obj_set_style_text_color(control_battery_value, FG_PRIMARY, 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_3, 94);
        lv_obj_set_y(lv_obj_3, 45);
        lv_obj_set_width(lv_obj_3, 3);
        lv_obj_set_height(lv_obj_3, 10);
        lv_obj_set_flag(lv_obj_3, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_3, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(lv_obj_3, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_3, 0, 0);
        lv_obj_set_style_radius(lv_obj_3, 1, 0);

        lv_obj_t * connection_status = lv_label_create(lv_obj_0);
        lv_obj_set_name(connection_status, "connection_status");
        lv_obj_set_x(connection_status, 108);
        lv_obj_set_y(connection_status, 39);
        lv_label_set_text(connection_status, "");
        lv_obj_set_style_text_font(connection_status, icons_20, 0);
        lv_obj_set_style_text_color(connection_status, lv_color_hex(0x707070), 0);

        lv_obj_t * control_clock = lv_label_create(lv_obj_0);
        lv_obj_set_name(control_clock, "control_clock");
        lv_obj_set_x(control_clock, 270);
        lv_obj_set_y(control_clock, 33);
        lv_obj_set_width(control_clock, 96);
        lv_label_set_text(control_clock, "13:35");
        lv_obj_set_style_text_align(control_clock, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(control_clock, title_24, 0);

        lv_obj_t * dnd_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(dnd_button, "dnd_button");
        lv_obj_set_x(dnd_button, 24);
        lv_obj_set_y(dnd_button, 88);
        lv_obj_set_width(dnd_button, 100);
        lv_obj_set_height(dnd_button, 100);
        lv_obj_set_style_bg_color(dnd_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(dnd_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(dnd_button, 0, 0);
        lv_obj_set_style_radius(dnd_button, 50, 0);
        lv_obj_set_style_pad_all(dnd_button, 0, 0);
        lv_obj_set_style_shadow_width(dnd_button, 0, 0);
        lv_obj_t * lv_image_0 = lv_image_create(dnd_button);
        lv_obj_set_align(lv_image_0, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_0, control_dnd);

        lv_obj_t * silent_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(silent_button, "silent_button");
        lv_obj_set_x(silent_button, 145);
        lv_obj_set_y(silent_button, 88);
        lv_obj_set_width(silent_button, 100);
        lv_obj_set_height(silent_button, 100);
        lv_obj_set_style_bg_color(silent_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(silent_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(silent_button, 0, 0);
        lv_obj_set_style_radius(silent_button, 50, 0);
        lv_obj_set_style_pad_all(silent_button, 0, 0);
        lv_obj_set_style_shadow_width(silent_button, 0, 0);
        lv_obj_t * lv_image_1 = lv_image_create(silent_button);
        lv_obj_set_align(lv_image_1, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_1, control_silent);

        lv_obj_t * wrist_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(wrist_button, "wrist_button");
        lv_obj_set_x(wrist_button, 266);
        lv_obj_set_y(wrist_button, 88);
        lv_obj_set_width(wrist_button, 100);
        lv_obj_set_height(wrist_button, 100);
        lv_obj_set_style_bg_color(wrist_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(wrist_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(wrist_button, 0, 0);
        lv_obj_set_style_radius(wrist_button, 50, 0);
        lv_obj_set_style_pad_all(wrist_button, 0, 0);
        lv_obj_set_style_shadow_width(wrist_button, 0, 0);
        lv_obj_t * lv_image_2 = lv_image_create(wrist_button);
        lv_obj_set_align(lv_image_2, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_2, control_wrist);

        lv_obj_t * settings_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(settings_button, "settings_button");
        lv_obj_set_x(settings_button, 24);
        lv_obj_set_y(settings_button, 204);
        lv_obj_set_width(settings_button, 100);
        lv_obj_set_height(settings_button, 100);
        lv_obj_set_style_bg_color(settings_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(settings_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(settings_button, 0, 0);
        lv_obj_set_style_radius(settings_button, 50, 0);
        lv_obj_set_style_pad_all(settings_button, 0, 0);
        lv_obj_set_style_shadow_width(settings_button, 0, 0);
        lv_obj_t * lv_image_3 = lv_image_create(settings_button);
        lv_obj_set_align(lv_image_3, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_3, control_settings);

        lv_obj_t * flashlight_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(flashlight_button, "flashlight_button");
        lv_obj_set_x(flashlight_button, 145);
        lv_obj_set_y(flashlight_button, 204);
        lv_obj_set_width(flashlight_button, 100);
        lv_obj_set_height(flashlight_button, 100);
        lv_obj_set_style_bg_color(flashlight_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(flashlight_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(flashlight_button, 0, 0);
        lv_obj_set_style_radius(flashlight_button, 50, 0);
        lv_obj_set_style_pad_all(flashlight_button, 0, 0);
        lv_obj_set_style_shadow_width(flashlight_button, 0, 0);
        lv_obj_t * lv_image_4 = lv_image_create(flashlight_button);
        lv_obj_set_align(lv_image_4, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_4, control_flashlight);

        lv_obj_t * phone_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(phone_button, "phone_button");
        lv_obj_set_x(phone_button, 266);
        lv_obj_set_y(phone_button, 204);
        lv_obj_set_width(phone_button, 100);
        lv_obj_set_height(phone_button, 100);
        lv_obj_set_flag(phone_button, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(phone_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(phone_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(phone_button, 0, 0);
        lv_obj_set_style_radius(phone_button, 50, 0);
        lv_obj_set_style_pad_all(phone_button, 0, 0);
        lv_obj_set_style_shadow_width(phone_button, 0, 0);
        lv_obj_t * lv_image_5 = lv_image_create(phone_button);
        lv_obj_set_align(lv_image_5, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_5, control_phone);

        lv_obj_t * drain_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(drain_button, "drain_button");
        lv_obj_set_x(drain_button, 24);
        lv_obj_set_y(drain_button, 320);
        lv_obj_set_width(drain_button, 100);
        lv_obj_set_height(drain_button, 100);
        lv_obj_set_flag(drain_button, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(drain_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(drain_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(drain_button, 0, 0);
        lv_obj_set_style_radius(drain_button, 50, 0);
        lv_obj_set_style_pad_all(drain_button, 0, 0);
        lv_obj_set_style_shadow_width(drain_button, 0, 0);
        lv_obj_t * lv_image_6 = lv_image_create(drain_button);
        lv_obj_set_align(lv_image_6, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_6, control_drain);

        lv_obj_t * endurance_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(endurance_button, "endurance_button");
        lv_obj_set_x(endurance_button, 145);
        lv_obj_set_y(endurance_button, 320);
        lv_obj_set_width(endurance_button, 100);
        lv_obj_set_height(endurance_button, 100);
        lv_obj_set_style_bg_color(endurance_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(endurance_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(endurance_button, 0, 0);
        lv_obj_set_style_radius(endurance_button, 50, 0);
        lv_obj_set_style_pad_all(endurance_button, 0, 0);
        lv_obj_set_style_shadow_width(endurance_button, 0, 0);
        lv_obj_t * lv_image_7 = lv_image_create(endurance_button);
        lv_obj_set_align(lv_image_7, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_7, control_battery);

        lv_obj_t * keep_awake_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(keep_awake_button, "keep_awake_button");
        lv_obj_set_x(keep_awake_button, 266);
        lv_obj_set_y(keep_awake_button, 320);
        lv_obj_set_width(keep_awake_button, 100);
        lv_obj_set_height(keep_awake_button, 100);
        lv_obj_set_style_bg_color(keep_awake_button, lv_color_hex(0x252525), 0);
        lv_obj_set_style_bg_opa(keep_awake_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(keep_awake_button, 0, 0);
        lv_obj_set_style_radius(keep_awake_button, 50, 0);
        lv_obj_set_style_pad_all(keep_awake_button, 0, 0);
        lv_obj_set_style_shadow_width(keep_awake_button, 0, 0);
        lv_obj_t * lv_image_8 = lv_image_create(keep_awake_button);
        lv_obj_set_align(lv_image_8, LV_ALIGN_CENTER);
        lv_image_set_src(lv_image_8, control_awake);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

