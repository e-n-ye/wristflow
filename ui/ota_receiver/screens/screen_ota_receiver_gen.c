/**
 * @file screen_ota_receiver_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_ota_receiver_gen.h"
#include "../wristflow_ota_ui.h"

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

lv_obj_t * screen_ota_receiver_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t progress_indicator;
    static lv_style_t no_knob;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&progress_indicator);
        lv_style_init(&no_knob);

        lv_style_set_arc_color(&progress_indicator, lv_color_hex(0x0396fa));
        lv_style_set_arc_width(&progress_indicator, 8);
        lv_style_set_arc_rounded(&progress_indicator, true);
        lv_style_set_bg_opa(&no_knob, (255 * 0 / 100));
        lv_style_set_pad_all(&no_knob, 0);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_OTA_UI_CHECK_COMPILE_TARGET(WRISTFLOW_OTA_UI_TARGET_ALL)
    if (wristflow_ota_ui_check_target(WRISTFLOW_OTA_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_ota_receiver_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, lv_color_hex(0xffffff), 0);

        lv_obj_t * ota_state = lv_label_create(lv_obj_0);
        lv_obj_set_name(ota_state, "ota_state");
        lv_obj_set_x(ota_state, 24);
        lv_obj_set_y(ota_state, 50);
        lv_obj_set_width(ota_state, 342);
        lv_label_set_text(ota_state, "等待手机连接");
        lv_obj_set_style_text_font(ota_state, ota_title, 0);
        lv_obj_set_style_text_align(ota_state, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * ota_progress = lv_arc_create(lv_obj_0);
        lv_obj_set_name(ota_progress, "ota_progress");
        lv_obj_set_x(ota_progress, 107);
        lv_obj_set_y(ota_progress, 121);
        lv_obj_set_width(ota_progress, 176);
        lv_obj_set_height(ota_progress, 176);
        lv_obj_set_flag(ota_progress, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(ota_progress, 0);
        lv_arc_set_bg_start_angle(ota_progress, 0);
        lv_arc_set_bg_end_angle(ota_progress, 360);
        lv_arc_set_rotation(ota_progress, 270);
        lv_obj_set_style_bg_opa(ota_progress, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(ota_progress, 0, 0);
        lv_obj_set_style_pad_all(ota_progress, 0, 0);
        lv_obj_set_style_arc_color(ota_progress, lv_color_hex(0x24262a), 0);
        lv_obj_set_style_arc_width(ota_progress, 8, 0);
        lv_obj_add_style(ota_progress, &progress_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(ota_progress, &no_knob, LV_PART_KNOB);

        lv_obj_t * ota_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(ota_value, "ota_value");
        lv_obj_set_x(ota_value, 24);
        lv_obj_set_y(ota_value, 189);
        lv_obj_set_width(ota_value, 342);
        lv_label_set_text(ota_value, "0%");
        lv_obj_set_flag(ota_value, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_style_text_font(ota_value, ota_digits, 0);
        lv_obj_set_style_text_align(ota_value, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * ota_peer = lv_label_create(lv_obj_0);
        lv_obj_set_name(ota_peer, "ota_peer");
        lv_obj_set_x(ota_peer, 24);
        lv_obj_set_y(ota_peer, 325);
        lv_obj_set_width(ota_peer, 342);
        lv_label_set_text(ota_peer, "WristFlow OTA");
        lv_obj_set_style_text_font(ota_peer, ota_title, 0);
        lv_obj_set_style_text_align(ota_peer, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * ota_icon = lv_label_create(lv_obj_0);
        lv_obj_set_name(ota_icon, "ota_icon");
        lv_obj_set_x(ota_icon, 24);
        lv_obj_set_y(ota_icon, 185);
        lv_obj_set_width(ota_icon, 342);
        lv_label_set_text(ota_icon, "");
        lv_obj_set_style_text_font(ota_icon, ota_bluetooth, 0);
        lv_obj_set_style_text_color(ota_icon, lv_color_hex(0x0396fa), 0);
        lv_obj_set_style_text_align(ota_icon, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * ota_instructions = lv_label_create(lv_obj_0);
        lv_obj_set_name(ota_instructions, "ota_instructions");
        lv_obj_set_x(ota_instructions, 24);
        lv_obj_set_y(ota_instructions, 369);
        lv_obj_set_width(ota_instructions, 342);
        lv_label_set_text(ota_instructions, "在 SIFLI BLE 中选择此设备");
        lv_obj_set_style_text_font(ota_instructions, ota_hint, 0);
        lv_obj_set_style_text_color(ota_instructions, lv_color_hex(0xa7adb5), 0);
        lv_obj_set_style_text_align(ota_instructions, LV_TEXT_ALIGN_CENTER, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

