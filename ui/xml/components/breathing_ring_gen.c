/**
 * @file breathing_ring_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "breathing_ring_gen.h"
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

lv_obj_t * breathing_ring_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t breath_glow;
    static lv_style_t breath_halo;
    static lv_style_t breath_line;
    static lv_style_t breath_knob;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&breath_glow);
        lv_style_init(&breath_halo);
        lv_style_init(&breath_line);
        lv_style_init(&breath_knob);

        lv_style_set_arc_color(&breath_glow, lv_color_hex(0x0065ff));
        lv_style_set_arc_width(&breath_glow, 26);
        lv_style_set_arc_opa(&breath_glow, (255 * 15 / 100));
        lv_style_set_arc_rounded(&breath_glow, true);
        lv_style_set_arc_color(&breath_halo, lv_color_hex(0x008cff));
        lv_style_set_arc_width(&breath_halo, 14);
        lv_style_set_arc_opa(&breath_halo, (255 * 40 / 100));
        lv_style_set_arc_rounded(&breath_halo, true);
        lv_style_set_arc_color(&breath_line, lv_color_hex(0x58e7ff));
        lv_style_set_arc_width(&breath_line, 4);
        lv_style_set_arc_opa(&breath_line, (255 * 100 / 100));
        lv_style_set_arc_rounded(&breath_line, true);
        lv_style_set_bg_opa(&breath_knob, (255 * 0 / 100));
        lv_style_set_pad_all(&breath_knob, 0);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "breathing_ring_#");
        lv_obj_set_width(lv_obj_0, 180);
        lv_obj_set_height(lv_obj_0, 180);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * ring_glow = lv_arc_create(lv_obj_0);
        lv_obj_set_name(ring_glow, "ring_glow");
        lv_obj_set_width(ring_glow, lv_pct(100));
        lv_obj_set_height(ring_glow, lv_pct(100));
        lv_arc_set_start_angle(ring_glow, 315);
        lv_arc_set_end_angle(ring_glow, 270);
        lv_obj_set_flag(ring_glow, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(ring_glow, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(ring_glow, 0, 0);
        lv_obj_set_style_arc_opa(ring_glow, (255 * 0 / 100), 0);
        lv_obj_add_style(ring_glow, &breath_glow, LV_PART_INDICATOR);
        lv_obj_add_style(ring_glow, &breath_knob, LV_PART_KNOB);

        lv_obj_t * ring_halo = lv_arc_create(lv_obj_0);
        lv_obj_set_name(ring_halo, "ring_halo");
        lv_obj_set_width(ring_halo, lv_pct(100));
        lv_obj_set_height(ring_halo, lv_pct(100));
        lv_arc_set_start_angle(ring_halo, 315);
        lv_arc_set_end_angle(ring_halo, 270);
        lv_obj_set_flag(ring_halo, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(ring_halo, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(ring_halo, 0, 0);
        lv_obj_set_style_arc_opa(ring_halo, (255 * 0 / 100), 0);
        lv_obj_add_style(ring_halo, &breath_halo, LV_PART_INDICATOR);
        lv_obj_add_style(ring_halo, &breath_knob, LV_PART_KNOB);

        lv_obj_t * ring_line = lv_arc_create(lv_obj_0);
        lv_obj_set_name(ring_line, "ring_line");
        lv_obj_set_width(ring_line, lv_pct(100));
        lv_obj_set_height(ring_line, lv_pct(100));
        lv_arc_set_start_angle(ring_line, 315);
        lv_arc_set_end_angle(ring_line, 270);
        lv_obj_set_flag(ring_line, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(ring_line, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(ring_line, 0, 0);
        lv_obj_set_style_arc_opa(ring_line, (255 * 0 / 100), 0);
        lv_obj_add_style(ring_line, &breath_line, LV_PART_INDICATOR);
        lv_obj_add_style(ring_line, &breath_knob, LV_PART_KNOB);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

