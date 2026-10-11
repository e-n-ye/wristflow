/**
 * @file alarm_item_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "alarm_item_gen.h"
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

lv_obj_t * alarm_item_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t alarm_switch_on;
    static lv_style_t alarm_switch_knob;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&alarm_switch_on);
        lv_style_init(&alarm_switch_knob);

        lv_style_set_bg_color(&alarm_switch_on, lv_color_hex(0x008cff));
        lv_style_set_bg_opa(&alarm_switch_on, (255 * 100 / 100));
        lv_style_set_radius(&alarm_switch_on, 20);
        lv_style_set_bg_color(&alarm_switch_knob, FG_PRIMARY);
        lv_style_set_bg_opa(&alarm_switch_knob, (255 * 100 / 100));
        lv_style_set_radius(&alarm_switch_knob, 16);
        lv_style_set_pad_all(&alarm_switch_knob, -4);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "alarm_item_#");
        lv_obj_set_width(lv_obj_0, 342);
        lv_obj_set_height(lv_obj_0, 94);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x202124), 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 24, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * alarm_open = lv_button_create(lv_obj_0);
        lv_obj_set_name(alarm_open, "alarm_open");
        lv_obj_set_x(alarm_open, 0);
        lv_obj_set_y(alarm_open, 0);
        lv_obj_set_width(alarm_open, 244);
        lv_obj_set_height(alarm_open, 94);
        lv_obj_set_style_bg_opa(alarm_open, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(alarm_open, 0, 0);
        lv_obj_set_style_shadow_width(alarm_open, 0, 0);
        lv_obj_set_style_pad_all(alarm_open, 0, 0);
        lv_obj_t * alarm_time = lv_label_create(alarm_open);
        lv_obj_set_name(alarm_time, "alarm_time");
        lv_obj_set_x(alarm_time, 20);
        lv_obj_set_y(alarm_time, 8);
        lv_label_set_text(alarm_time, "07:30");
        lv_obj_set_style_text_font(alarm_time, value_36, 0);

        lv_obj_t * alarm_rule = lv_label_create(alarm_open);
        lv_obj_set_name(alarm_rule, "alarm_rule");
        lv_obj_set_x(alarm_rule, 20);
        lv_obj_set_y(alarm_rule, 55);
        lv_obj_set_width(alarm_rule, 222);
        lv_label_set_text(alarm_rule, "仅一次");
        lv_obj_set_style_text_font(alarm_rule, body_20, 0);
        lv_obj_set_style_text_color(alarm_rule, FG_SECONDARY, 0);

        lv_obj_t * alarm_enabled = lv_switch_create(lv_obj_0);
        lv_obj_set_name(alarm_enabled, "alarm_enabled");
        lv_obj_set_x(alarm_enabled, 256);
        lv_obj_set_y(alarm_enabled, 26);
        lv_obj_set_width(alarm_enabled, 66);
        lv_obj_set_height(alarm_enabled, 40);
        lv_obj_set_state(alarm_enabled, LV_STATE_CHECKED, true);
        lv_obj_set_style_bg_color(alarm_enabled, lv_color_hex(0x44484e), 0);
        lv_obj_set_style_bg_opa(alarm_enabled, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(alarm_enabled, 20, 0);
        lv_obj_set_style_pad_all(alarm_enabled, 0, 0);
        lv_obj_set_style_border_width(alarm_enabled, 0, 0);
        lv_obj_add_style(alarm_enabled, &alarm_switch_on, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(alarm_enabled, &alarm_switch_knob, LV_PART_KNOB);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

