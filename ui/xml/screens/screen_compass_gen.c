/**
 * @file screen_compass_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_compass_gen.h"
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

lv_obj_t * screen_compass_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t compass_knob;
    static lv_style_t compass_indicator;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&compass_knob);
        lv_style_init(&compass_indicator);

        lv_style_set_bg_opa(&compass_knob, (255 * 0 / 100));
        lv_style_set_pad_all(&compass_knob, 0);
        lv_style_set_arc_opa(&compass_indicator, (255 * 0 / 100));

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * compass_page = lv_obj_create(NULL);
        lv_obj_set_name_static(compass_page, "screen_compass_#");
        lv_obj_set_name(compass_page, "compass_page");
        lv_obj_set_width(compass_page, 390);
        lv_obj_set_height(compass_page, 450);
        lv_obj_set_flag(compass_page, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(compass_page, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(compass_page, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(compass_page, 0, 0);
        lv_obj_set_style_pad_all(compass_page, 0, 0);
        lv_obj_set_style_radius(compass_page, 0, 0);
        lv_obj_set_style_text_color(compass_page, lv_color_hex(0xb9ddff), 0);
        lv_obj_set_style_text_font(compass_page, compass_32, 0);

        lv_obj_t * compass_dial = lv_obj_create(compass_page);
        lv_obj_set_name(compass_dial, "compass_dial");
        lv_obj_set_width(compass_dial, 390);
        lv_obj_set_height(compass_dial, 450);
        lv_obj_set_flag(compass_dial, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(compass_dial, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(compass_dial, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(compass_dial, 0, 0);
        lv_obj_set_style_pad_all(compass_dial, 0, 0);
        lv_obj_set_style_radius(compass_dial, 0, 0);
        lv_obj_set_style_text_color(compass_dial, lv_color_hex(0xb9ddff), 0);
        lv_obj_set_style_text_font(compass_dial, compass_32, 0);
        lv_obj_t * compass_index = lv_obj_create(compass_dial);
        lv_obj_set_name(compass_index, "compass_index");
        lv_obj_set_x(compass_index, 192);
        lv_obj_set_y(compass_index, 38);
        lv_obj_set_width(compass_index, 6);
        lv_obj_set_height(compass_index, 22);
        lv_obj_set_flag(compass_index, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(compass_index, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(compass_index, lv_color_hex(0x56595e), 0);
        lv_obj_set_style_bg_opa(compass_index, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(compass_index, 0, 0);
        lv_obj_set_style_radius(compass_index, 3, 0);
        lv_obj_set_style_pad_all(compass_index, 0, 0);

        lv_obj_t * compass_ring_a = lv_arc_create(compass_dial);
        lv_obj_set_name(compass_ring_a, "compass_ring_a");
        lv_obj_set_x(compass_ring_a, 21);
        lv_obj_set_y(compass_ring_a, 67);
        lv_obj_set_width(compass_ring_a, 348);
        lv_obj_set_height(compass_ring_a, 348);
        lv_arc_set_value(compass_ring_a, 0);
        lv_arc_set_bg_start_angle(compass_ring_a, 0);
        lv_arc_set_bg_end_angle(compass_ring_a, 154);
        lv_arc_set_rotation(compass_ring_a, 339);
        lv_obj_set_flag(compass_ring_a, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(compass_ring_a, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(compass_ring_a, 0, 0);
        lv_obj_set_style_pad_all(compass_ring_a, 0, 0);
        lv_obj_set_style_arc_color(compass_ring_a, lv_color_hex(0x56595e), 0);
        lv_obj_set_style_arc_width(compass_ring_a, 22, 0);
        lv_obj_set_style_arc_rounded(compass_ring_a, true, 0);
        lv_obj_add_style(compass_ring_a, &compass_knob, LV_PART_KNOB);
        lv_obj_add_style(compass_ring_a, &compass_indicator, LV_PART_INDICATOR);

        lv_obj_t * compass_ring_b = lv_arc_create(compass_dial);
        lv_obj_set_name(compass_ring_b, "compass_ring_b");
        lv_obj_set_x(compass_ring_b, 21);
        lv_obj_set_y(compass_ring_b, 67);
        lv_obj_set_width(compass_ring_b, 348);
        lv_obj_set_height(compass_ring_b, 348);
        lv_arc_set_value(compass_ring_b, 0);
        lv_arc_set_bg_start_angle(compass_ring_b, 0);
        lv_arc_set_bg_end_angle(compass_ring_b, 154);
        lv_arc_set_rotation(compass_ring_b, 159);
        lv_obj_set_flag(compass_ring_b, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(compass_ring_b, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(compass_ring_b, 0, 0);
        lv_obj_set_style_pad_all(compass_ring_b, 0, 0);
        lv_obj_set_style_arc_color(compass_ring_b, lv_color_hex(0x56595e), 0);
        lv_obj_set_style_arc_width(compass_ring_b, 22, 0);
        lv_obj_set_style_arc_rounded(compass_ring_b, true, 0);
        lv_obj_add_style(compass_ring_b, &compass_knob, LV_PART_KNOB);
        lv_obj_add_style(compass_ring_b, &compass_indicator, LV_PART_INDICATOR);

        lv_obj_t * compass_north_arc = lv_arc_create(compass_dial);
        lv_obj_set_name(compass_north_arc, "compass_north_arc");
        lv_obj_set_x(compass_north_arc, 21);
        lv_obj_set_y(compass_north_arc, 67);
        lv_obj_set_width(compass_north_arc, 348);
        lv_obj_set_height(compass_north_arc, 348);
        lv_arc_set_value(compass_north_arc, 0);
        lv_arc_set_bg_start_angle(compass_north_arc, 0);
        lv_arc_set_bg_end_angle(compass_north_arc, 32);
        lv_arc_set_rotation(compass_north_arc, 280);
        lv_obj_set_flag(compass_north_arc, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(compass_north_arc, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(compass_north_arc, 0, 0);
        lv_obj_set_style_pad_all(compass_north_arc, 0, 0);
        lv_obj_set_style_arc_color(compass_north_arc, lv_color_hex(0xff354f), 0);
        lv_obj_set_style_arc_width(compass_north_arc, 16, 0);
        lv_obj_set_style_arc_rounded(compass_north_arc, true, 0);
        lv_obj_add_style(compass_north_arc, &compass_knob, LV_PART_KNOB);
        lv_obj_add_style(compass_north_arc, &compass_indicator, LV_PART_INDICATOR);

        lv_obj_t * compass_north_pointer = lv_image_create(compass_dial);
        lv_obj_set_name(compass_north_pointer, "compass_north_pointer");
        lv_obj_set_x(compass_north_pointer, 316);
        lv_obj_set_y(compass_north_pointer, 135);
        lv_image_set_src(compass_north_pointer, compass_pointer);
        lv_image_set_rotation(compass_north_pointer, 550);
        lv_obj_set_style_image_recolor(compass_north_pointer, lv_color_hex(0xff354f), 0);
        lv_obj_set_style_image_recolor_opa(compass_north_pointer, (255 * 100 / 100), 0);

        lv_obj_t * compass_south_pointer = lv_image_create(compass_dial);
        lv_obj_set_name(compass_south_pointer, "compass_south_pointer");
        lv_obj_set_x(compass_south_pointer, 50);
        lv_obj_set_y(compass_south_pointer, 323);
        lv_image_set_src(compass_south_pointer, compass_pointer);
        lv_image_set_rotation(compass_south_pointer, 2350);
        lv_obj_set_style_image_recolor(compass_south_pointer, lv_color_hex(0x71b9ff), 0);
        lv_obj_set_style_image_recolor_opa(compass_south_pointer, (255 * 100 / 100), 0);

        lv_obj_t * compass_n = lv_label_create(compass_dial);
        lv_obj_set_name(compass_n, "compass_n");
        lv_obj_set_x(compass_n, 274);
        lv_obj_set_y(compass_n, 147);
        lv_obj_set_width(compass_n, 48);
        lv_obj_set_height(compass_n, 44);
        lv_label_set_text(compass_n, "N");
        lv_obj_set_style_text_color(compass_n, lv_color_hex(0xff354f), 0);
        lv_obj_set_style_text_align(compass_n, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_transform_pivot_x(compass_n, 24, 0);
        lv_obj_set_style_transform_pivot_y(compass_n, 22, 0);
        lv_obj_set_style_transform_rotation(compass_n, 550, 0);

        lv_obj_t * compass_e = lv_label_create(compass_dial);
        lv_obj_set_name(compass_e, "compass_e");
        lv_obj_set_x(compass_e, 243);
        lv_obj_set_y(compass_e, 322);
        lv_obj_set_width(compass_e, 48);
        lv_obj_set_height(compass_e, 44);
        lv_label_set_text(compass_e, "E");
        lv_obj_set_style_text_align(compass_e, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_transform_pivot_x(compass_e, 24, 0);
        lv_obj_set_style_transform_pivot_y(compass_e, 22, 0);
        lv_obj_set_style_transform_rotation(compass_e, 1450, 0);

        lv_obj_t * compass_s = lv_label_create(compass_dial);
        lv_obj_set_name(compass_s, "compass_s");
        lv_obj_set_x(compass_s, 68);
        lv_obj_set_y(compass_s, 291);
        lv_obj_set_width(compass_s, 48);
        lv_obj_set_height(compass_s, 44);
        lv_label_set_text(compass_s, "S");
        lv_obj_set_style_text_align(compass_s, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_transform_pivot_x(compass_s, 24, 0);
        lv_obj_set_style_transform_pivot_y(compass_s, 22, 0);
        lv_obj_set_style_transform_rotation(compass_s, 2350, 0);

        lv_obj_t * compass_w = lv_label_create(compass_dial);
        lv_obj_set_name(compass_w, "compass_w");
        lv_obj_set_x(compass_w, 99);
        lv_obj_set_y(compass_w, 116);
        lv_obj_set_width(compass_w, 48);
        lv_obj_set_height(compass_w, 44);
        lv_label_set_text(compass_w, "W");
        lv_obj_set_style_text_align(compass_w, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_transform_pivot_x(compass_w, 24, 0);
        lv_obj_set_style_transform_pivot_y(compass_w, 22, 0);
        lv_obj_set_style_transform_rotation(compass_w, 3250, 0);

        lv_obj_t * compass_heading = lv_label_create(compass_dial);
        lv_obj_set_name(compass_heading, "compass_heading");
        lv_obj_set_x(compass_heading, 85);
        lv_obj_set_y(compass_heading, 173);
        lv_obj_set_width(compass_heading, 220);
        lv_obj_set_height(compass_heading, 84);
        lv_label_set_text(compass_heading, "305°");
        lv_obj_set_style_text_font(compass_heading, metric_56, 0);
        lv_obj_set_style_text_align(compass_heading, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * compass_direction = lv_label_create(compass_dial);
        lv_obj_set_name(compass_direction, "compass_direction");
        lv_obj_set_x(compass_direction, 95);
        lv_obj_set_y(compass_direction, 259);
        lv_obj_set_width(compass_direction, 200);
        lv_obj_set_height(compass_direction, 52);
        lv_label_set_text(compass_direction, "西北");
        lv_obj_set_style_text_align(compass_direction, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * compass_status = lv_label_create(compass_dial);
        lv_obj_set_name(compass_status, "compass_status");
        lv_obj_set_x(compass_status, 55);
        lv_obj_set_y(compass_status, 312);
        lv_obj_set_width(compass_status, 280);
        lv_label_set_text(compass_status, "");
        lv_obj_set_style_text_font(compass_status, body_20, 0);
        lv_obj_set_style_text_color(compass_status, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(compass_status, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * compass_notice = lv_label_create(compass_dial);
        lv_obj_set_name(compass_notice, "compass_notice");
        lv_obj_set_x(compass_notice, 24);
        lv_obj_set_y(compass_notice, 420);
        lv_obj_set_width(compass_notice, 342);
        lv_obj_set_height(compass_notice, 26);
        lv_label_set_text(compass_notice, "");
        lv_obj_set_style_text_font(compass_notice, body_20, 0);
        lv_obj_set_style_text_color(compass_notice, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(compass_notice, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * compass_calibration_panel = compass_calibration_create(compass_page);
        lv_obj_set_name(compass_calibration_panel, "compass_calibration_panel");
        lv_obj_set_flag(compass_calibration_panel, LV_OBJ_FLAG_HIDDEN, true);

        the_root = compass_page;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

