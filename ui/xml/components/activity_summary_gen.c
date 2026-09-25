/**
 * @file activity_summary_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "activity_summary_gen.h"
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

lv_obj_t * activity_summary_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t empty_indicator;
    static lv_style_t hidden_knob;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&empty_indicator);
        lv_style_init(&hidden_knob);

        lv_style_set_arc_opa(&empty_indicator, (255 * 0 / 100));
        lv_style_set_bg_opa(&hidden_knob, (255 * 0 / 100));
        lv_style_set_pad_all(&hidden_knob, 0);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * tile_half_0 = tile_half_create(parent, BG_SURFACE);
        lv_obj_set_name_static(tile_half_0, "activity_summary_#");

        lv_obj_t * caption_label = lv_label_create(tile_half_0);
        lv_obj_set_name(caption_label, "caption_label");
        lv_obj_set_x(caption_label, 20);
        lv_obj_set_y(caption_label, 12);
        lv_obj_set_width(caption_label, 250);
        lv_label_set_text(caption_label, "活力指标");
        lv_obj_set_style_text_font(caption_label, body_20, 0);
        lv_obj_set_style_text_color(caption_label, FG_SECONDARY, 0);

        lv_obj_t * value_label = lv_label_create(tile_half_0);
        lv_obj_set_name(value_label, "value_label");
        lv_obj_set_x(value_label, 20);
        lv_obj_set_y(value_label, 38);
        lv_label_set_text(value_label, "--");
        lv_obj_set_style_text_font(value_label, value_36, 0);
        lv_obj_set_style_text_color(value_label, ACCENT_ORANGE, 0);

        lv_obj_t * activity_value_1 = lv_label_create(tile_half_0);
        lv_obj_set_name(activity_value_1, "activity_value_1");
        lv_obj_set_x(activity_value_1, 20);
        lv_obj_set_y(activity_value_1, 78);
        lv_label_set_text(activity_value_1, "--");
        lv_obj_set_style_text_font(activity_value_1, value_36, 0);
        lv_obj_set_style_text_color(activity_value_1, lv_color_hex(0xe2db48), 0);

        lv_obj_t * activity_value_2 = lv_label_create(tile_half_0);
        lv_obj_set_name(activity_value_2, "activity_value_2");
        lv_obj_set_x(activity_value_2, 20);
        lv_obj_set_y(activity_value_2, 118);
        lv_label_set_text(activity_value_2, "--");
        lv_obj_set_style_text_font(activity_value_2, value_36, 0);
        lv_obj_set_style_text_color(activity_value_2, ACCENT_BLUE, 0);

        lv_obj_t * activity_ring_0 = lv_arc_create(tile_half_0);
        lv_obj_set_name(activity_ring_0, "activity_ring_0");
        lv_obj_set_x(activity_ring_0, 160);
        lv_obj_set_y(activity_ring_0, 46);
        lv_obj_set_width(activity_ring_0, 156);
        lv_obj_set_height(activity_ring_0, 156);
        lv_obj_set_flag(activity_ring_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(activity_ring_0, 0);
        lv_arc_set_bg_start_angle(activity_ring_0, 180);
        lv_arc_set_bg_end_angle(activity_ring_0, 360);
        lv_obj_set_style_arc_width(activity_ring_0, 12, 0);
        lv_obj_set_style_arc_color(activity_ring_0, lv_color_hex(0x593820), 0);
        lv_obj_add_style(activity_ring_0, &empty_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_0, &hidden_knob, LV_PART_KNOB);

        lv_obj_t * activity_ring_1 = lv_arc_create(tile_half_0);
        lv_obj_set_name(activity_ring_1, "activity_ring_1");
        lv_obj_set_x(activity_ring_1, 182);
        lv_obj_set_y(activity_ring_1, 68);
        lv_obj_set_width(activity_ring_1, 112);
        lv_obj_set_height(activity_ring_1, 112);
        lv_obj_set_flag(activity_ring_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(activity_ring_1, 0);
        lv_arc_set_bg_start_angle(activity_ring_1, 180);
        lv_arc_set_bg_end_angle(activity_ring_1, 360);
        lv_obj_set_style_arc_width(activity_ring_1, 12, 0);
        lv_obj_set_style_arc_color(activity_ring_1, lv_color_hex(0x4c491f), 0);
        lv_obj_add_style(activity_ring_1, &empty_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_1, &hidden_knob, LV_PART_KNOB);

        lv_obj_t * activity_ring_2 = lv_arc_create(tile_half_0);
        lv_obj_set_name(activity_ring_2, "activity_ring_2");
        lv_obj_set_x(activity_ring_2, 204);
        lv_obj_set_y(activity_ring_2, 90);
        lv_obj_set_width(activity_ring_2, 68);
        lv_obj_set_height(activity_ring_2, 68);
        lv_obj_set_flag(activity_ring_2, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(activity_ring_2, 0);
        lv_arc_set_bg_start_angle(activity_ring_2, 180);
        lv_arc_set_bg_end_angle(activity_ring_2, 360);
        lv_obj_set_style_arc_width(activity_ring_2, 12, 0);
        lv_obj_set_style_arc_color(activity_ring_2, lv_color_hex(0x183f50), 0);
        lv_obj_add_style(activity_ring_2, &empty_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_2, &hidden_knob, LV_PART_KNOB);

        the_root = tile_half_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

