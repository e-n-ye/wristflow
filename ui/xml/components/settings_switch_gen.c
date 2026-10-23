/**
 * @file settings_switch_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_switch_gen.h"
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

lv_obj_t * settings_switch_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t switch_track;
    static lv_style_t switch_on;
    static lv_style_t switch_knob;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&switch_track);
        lv_style_init(&switch_on);
        lv_style_init(&switch_knob);

        lv_style_set_bg_color(&switch_track, FG_MUTED);
        lv_style_set_bg_opa(&switch_track, (255 * 100 / 100));
        lv_style_set_radius(&switch_track, 18);
        lv_style_set_border_width(&switch_track, 0);
        lv_style_set_pad_all(&switch_track, 4);
        lv_style_set_bg_color(&switch_on, ACCENT_BLUE);
        lv_style_set_bg_opa(&switch_on, (255 * 100 / 100));
        lv_style_set_radius(&switch_on, 18);
        lv_style_set_bg_color(&switch_knob, FG_PRIMARY);
        lv_style_set_bg_opa(&switch_knob, (255 * 100 / 100));
        lv_style_set_radius(&switch_knob, 14);
        lv_style_set_pad_all(&switch_knob, 0);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_switch_0 = lv_switch_create(parent);
        lv_obj_set_name_static(lv_switch_0, "settings_switch_#");
        lv_obj_set_x(lv_switch_0, -20);
        lv_obj_set_align(lv_switch_0, LV_ALIGN_RIGHT_MID);
        lv_obj_set_width(lv_switch_0, 64);
        lv_obj_set_height(lv_switch_0, 36);

        lv_obj_add_style(lv_switch_0, &switch_track, 0);
        lv_obj_add_style(lv_switch_0, &switch_on, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(lv_switch_0, &switch_knob, LV_PART_KNOB);

        the_root = lv_switch_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

