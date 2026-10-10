/**
 * @file countdown_preset_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "countdown_preset_gen.h"
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

lv_obj_t * countdown_preset_create(lv_obj_t * parent, const char * minutes)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t dial_minor;
    static lv_style_t dial_major;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&dial_minor);
        lv_style_init(&dial_major);

        lv_style_set_line_width(&dial_minor, 2);
        lv_style_set_length(&dial_minor, 4);
        lv_style_set_line_color(&dial_minor, lv_color_hex(0x9ac4ea));
        lv_style_set_line_width(&dial_major, 2);
        lv_style_set_length(&dial_major, 6);
        lv_style_set_line_color(&dial_major, lv_color_hex(0x9ac4ea));

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "countdown_preset_#");
        lv_obj_set_width(lv_button_0, 104);
        lv_obj_set_height(lv_button_0, 104);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, lv_color_hex(0x143d63), 0);
        lv_obj_set_style_bg_opa(lv_button_0, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(lv_button_0, 52, 0);
        lv_obj_set_style_border_width(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);

        lv_obj_t * lv_scale_0 = lv_scale_create(lv_button_0);
        lv_obj_set_x(lv_scale_0, 7);
        lv_obj_set_y(lv_scale_0, 7);
        lv_obj_set_width(lv_scale_0, 90);
        lv_obj_set_height(lv_scale_0, 90);
        lv_scale_set_mode(lv_scale_0, LV_SCALE_MODE_ROUND_INNER);
        lv_scale_set_total_tick_count(lv_scale_0, 31);
        lv_scale_set_major_tick_every(lv_scale_0, 5);
        lv_scale_set_label_show(lv_scale_0, false);
        lv_scale_set_angle_range(lv_scale_0, 360);
        lv_scale_set_rotation(lv_scale_0, 270);
        lv_obj_set_flag(lv_scale_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(lv_scale_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_scale_0, 0, 0);
        lv_obj_set_style_pad_all(lv_scale_0, 0, 0);
        lv_obj_set_style_line_width(lv_scale_0, 0, 0);
        lv_obj_add_style(lv_scale_0, &dial_minor, LV_PART_ITEMS);
        lv_obj_add_style(lv_scale_0, &dial_major, LV_PART_INDICATOR);

        lv_obj_t * lv_label_0 = lv_label_create(lv_button_0);
        lv_obj_set_x(lv_label_0, 0);
        lv_obj_set_y(lv_label_0, 21);
        lv_obj_set_width(lv_label_0, 104);
        lv_label_set_text(lv_label_0, minutes);
        lv_obj_set_style_text_font(lv_label_0, value_36, 0);
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_button_0);
        lv_obj_set_x(lv_label_1, 0);
        lv_obj_set_y(lv_label_1, 61);
        lv_obj_set_width(lv_label_1, 104);
        lv_label_set_text(lv_label_1, "min");
        lv_obj_set_style_text_font(lv_label_1, body_20, 0);
        lv_obj_set_style_text_color(lv_label_1, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

