/**
 * @file activity_rings_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "activity_rings_gen.h"
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

lv_obj_t * activity_rings_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t ring_progress;
    static lv_style_t no_knob;
    static lv_style_t calorie_progress;
    static lv_style_t steps_progress;
    static lv_style_t minutes_progress;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&ring_progress);
        lv_style_init(&no_knob);
        lv_style_init(&calorie_progress);
        lv_style_init(&steps_progress);
        lv_style_init(&minutes_progress);

        lv_style_set_arc_width(&ring_progress, 22);
        lv_style_set_arc_rounded(&ring_progress, true);
        lv_style_set_bg_opa(&no_knob, (255 * 0 / 100));
        lv_style_set_pad_all(&no_knob, 0);
        lv_style_set_arc_color(&calorie_progress, lv_color_hex(0xf97316));
        lv_style_set_arc_color(&steps_progress, lv_color_hex(0xe2db48));
        lv_style_set_arc_color(&minutes_progress, lv_color_hex(0x38bdf8));

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "activity_rings_#");
        lv_obj_set_width(lv_obj_0, 342);
        lv_obj_set_height(lv_obj_0, 150);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * activity_ring_0 = lv_arc_create(lv_obj_0);
        lv_obj_set_name(activity_ring_0, "activity_ring_0");
        lv_obj_set_x(activity_ring_0, 26);
        lv_obj_set_y(activity_ring_0, 0);
        lv_obj_set_width(activity_ring_0, 290);
        lv_obj_set_height(activity_ring_0, 290);
        lv_obj_set_flag(activity_ring_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(activity_ring_0, 0);
        lv_arc_set_bg_start_angle(activity_ring_0, 180);
        lv_arc_set_bg_end_angle(activity_ring_0, 360);
        lv_obj_set_style_arc_width(activity_ring_0, 22, 0);
        lv_obj_set_style_arc_color(activity_ring_0, lv_color_hex(0x492819), 0);
        lv_obj_add_style(activity_ring_0, &ring_progress, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_0, &calorie_progress, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_0, &no_knob, LV_PART_KNOB);

        lv_obj_t * activity_ring_1 = lv_arc_create(lv_obj_0);
        lv_obj_set_name(activity_ring_1, "activity_ring_1");
        lv_obj_set_x(activity_ring_1, 56);
        lv_obj_set_y(activity_ring_1, 30);
        lv_obj_set_width(activity_ring_1, 230);
        lv_obj_set_height(activity_ring_1, 230);
        lv_obj_set_flag(activity_ring_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(activity_ring_1, 0);
        lv_arc_set_bg_start_angle(activity_ring_1, 180);
        lv_arc_set_bg_end_angle(activity_ring_1, 360);
        lv_obj_set_style_arc_width(activity_ring_1, 22, 0);
        lv_obj_set_style_arc_color(activity_ring_1, lv_color_hex(0x3d3b1a), 0);
        lv_obj_add_style(activity_ring_1, &ring_progress, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_1, &steps_progress, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_1, &no_knob, LV_PART_KNOB);

        lv_obj_t * activity_ring_2 = lv_arc_create(lv_obj_0);
        lv_obj_set_name(activity_ring_2, "activity_ring_2");
        lv_obj_set_x(activity_ring_2, 86);
        lv_obj_set_y(activity_ring_2, 60);
        lv_obj_set_width(activity_ring_2, 170);
        lv_obj_set_height(activity_ring_2, 170);
        lv_obj_set_flag(activity_ring_2, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(activity_ring_2, 0);
        lv_arc_set_bg_start_angle(activity_ring_2, 180);
        lv_arc_set_bg_end_angle(activity_ring_2, 360);
        lv_obj_set_style_arc_width(activity_ring_2, 22, 0);
        lv_obj_set_style_arc_color(activity_ring_2, lv_color_hex(0x163847), 0);
        lv_obj_add_style(activity_ring_2, &ring_progress, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_2, &minutes_progress, LV_PART_INDICATOR);
        lv_obj_add_style(activity_ring_2, &no_knob, LV_PART_KNOB);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

