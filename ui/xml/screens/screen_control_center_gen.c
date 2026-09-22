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

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 24);
        lv_obj_set_y(lv_label_0, 24);
        lv_label_set_text(lv_label_0, "控制中心");
        lv_obj_set_style_text_font(lv_label_0, title_24, 0);

        lv_obj_t * dnd_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(dnd_button, "dnd_button");
        lv_obj_set_x(dnd_button, 24);
        lv_obj_set_y(dnd_button, 76);
        lv_obj_set_width(dnd_button, 165);
        lv_obj_set_height(dnd_button, 126);
        lv_obj_set_style_bg_color(dnd_button, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(dnd_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(dnd_button, 0, 0);
        lv_obj_set_style_radius(dnd_button, 28, 0);
        lv_obj_set_style_pad_all(dnd_button, 0, 0);
        lv_obj_set_style_shadow_width(dnd_button, 0, 0);
        lv_obj_set_style_text_color(dnd_button, FG_PRIMARY, 0);
        lv_obj_t * lv_label_1 = lv_label_create(dnd_button);
        lv_obj_set_align(lv_label_1, LV_ALIGN_TOP_MID);
        lv_obj_set_y(lv_label_1, 20);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_44, 0);

        lv_obj_t * lv_label_2 = lv_label_create(dnd_button);
        lv_obj_set_align(lv_label_2, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(lv_label_2, -14);
        lv_label_set_text(lv_label_2, "勿扰");

        lv_obj_t * flashlight_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(flashlight_button, "flashlight_button");
        lv_obj_set_x(flashlight_button, 201);
        lv_obj_set_y(flashlight_button, 76);
        lv_obj_set_width(flashlight_button, 165);
        lv_obj_set_height(flashlight_button, 126);
        lv_obj_set_style_bg_color(flashlight_button, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(flashlight_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(flashlight_button, 0, 0);
        lv_obj_set_style_radius(flashlight_button, 28, 0);
        lv_obj_set_style_pad_all(flashlight_button, 0, 0);
        lv_obj_set_style_shadow_width(flashlight_button, 0, 0);
        lv_obj_set_style_text_color(flashlight_button, FG_PRIMARY, 0);
        lv_obj_t * lv_label_3 = lv_label_create(flashlight_button);
        lv_obj_set_align(lv_label_3, LV_ALIGN_TOP_MID);
        lv_obj_set_y(lv_label_3, 20);
        lv_label_set_text(lv_label_3, "");
        lv_obj_set_style_text_font(lv_label_3, icons_44, 0);

        lv_obj_t * lv_label_4 = lv_label_create(flashlight_button);
        lv_obj_set_align(lv_label_4, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(lv_label_4, -14);
        lv_label_set_text(lv_label_4, "手电筒");

        lv_obj_t * keep_awake_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(keep_awake_button, "keep_awake_button");
        lv_obj_set_x(keep_awake_button, 24);
        lv_obj_set_y(keep_awake_button, 216);
        lv_obj_set_width(keep_awake_button, 165);
        lv_obj_set_height(keep_awake_button, 126);
        lv_obj_set_style_bg_color(keep_awake_button, lv_color_hex(0x67e1f7), 0);
        lv_obj_set_style_bg_opa(keep_awake_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(keep_awake_button, 0, 0);
        lv_obj_set_style_radius(keep_awake_button, 28, 0);
        lv_obj_set_style_pad_all(keep_awake_button, 0, 0);
        lv_obj_set_style_shadow_width(keep_awake_button, 0, 0);
        lv_obj_set_style_text_color(keep_awake_button, lv_color_hex(0x09272c), 0);
        lv_obj_t * lv_label_5 = lv_label_create(keep_awake_button);
        lv_obj_set_align(lv_label_5, LV_ALIGN_TOP_MID);
        lv_obj_set_y(lv_label_5, 20);
        lv_label_set_text(lv_label_5, "");
        lv_obj_set_style_text_font(lv_label_5, icons_44, 0);

        lv_obj_t * lv_label_6 = lv_label_create(keep_awake_button);
        lv_obj_set_align(lv_label_6, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(lv_label_6, -14);
        lv_label_set_text(lv_label_6, "常亮");

        lv_obj_t * settings_button = lv_button_create(lv_obj_0);
        lv_obj_set_name(settings_button, "settings_button");
        lv_obj_set_x(settings_button, 201);
        lv_obj_set_y(settings_button, 216);
        lv_obj_set_width(settings_button, 165);
        lv_obj_set_height(settings_button, 126);
        lv_obj_set_style_bg_color(settings_button, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(settings_button, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(settings_button, 0, 0);
        lv_obj_set_style_radius(settings_button, 28, 0);
        lv_obj_set_style_pad_all(settings_button, 0, 0);
        lv_obj_set_style_shadow_width(settings_button, 0, 0);
        lv_obj_set_style_text_color(settings_button, FG_PRIMARY, 0);
        lv_obj_t * lv_label_7 = lv_label_create(settings_button);
        lv_obj_set_align(lv_label_7, LV_ALIGN_TOP_MID);
        lv_obj_set_y(lv_label_7, 20);
        lv_label_set_text(lv_label_7, "");
        lv_obj_set_style_text_font(lv_label_7, icons_44, 0);

        lv_obj_t * lv_label_8 = lv_label_create(settings_button);
        lv_obj_set_align(lv_label_8, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(lv_label_8, -14);
        lv_label_set_text(lv_label_8, "设置");

        lv_obj_t * lv_label_9 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_9, 28);
        lv_obj_set_y(lv_label_9, 371);
        lv_label_set_text(lv_label_9, "");
        lv_obj_set_style_text_font(lv_label_9, icons_20, 0);

        lv_obj_t * brightness_slider = lv_slider_create(lv_obj_0);
        lv_obj_set_name(brightness_slider, "brightness_slider");
        lv_obj_set_x(brightness_slider, 70);
        lv_obj_set_y(brightness_slider, 377);
        lv_obj_set_width(brightness_slider, 292);
        lv_obj_set_height(brightness_slider, 12);
        lv_slider_set_value(brightness_slider, 60, false);
        lv_obj_set_style_bg_color(brightness_slider, BG_SURFACE_SUBTLE, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

