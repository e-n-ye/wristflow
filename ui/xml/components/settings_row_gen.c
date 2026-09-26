/**
 * @file settings_row_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_row_gen.h"
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

lv_obj_t * settings_row_create(lv_obj_t * parent, const char * title, const char * detail)
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
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "settings_row_#");
        lv_obj_set_width(lv_button_0, 342);
        lv_obj_set_height(lv_button_0, 96);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, BG_SURFACE, 0);
        lv_obj_set_style_border_width(lv_button_0, 0, 0);
        lv_obj_set_style_radius(lv_button_0, 22, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);
        lv_obj_set_style_text_color(lv_button_0, FG_PRIMARY, 0);

        lv_obj_t * row_title = lv_label_create(lv_button_0);
        lv_obj_set_name(row_title, "row_title");
        lv_obj_set_x(row_title, 20);
        lv_obj_set_y(row_title, 17);
        lv_obj_set_width(row_title, 234);
        lv_label_set_text(row_title, title);
        lv_obj_set_style_text_font(row_title, title_24, 0);

        lv_obj_t * row_detail = lv_label_create(lv_button_0);
        lv_obj_set_name(row_detail, "row_detail");
        lv_obj_set_x(row_detail, 20);
        lv_obj_set_y(row_detail, 55);
        lv_obj_set_width(row_detail, 280);
        lv_label_set_text(row_detail, detail);
        lv_obj_set_style_text_font(row_detail, body_20, 0);
        lv_obj_set_style_text_color(row_detail, FG_SECONDARY, 0);

        lv_obj_t * row_check = lv_obj_create(lv_button_0);
        lv_obj_set_name(row_check, "row_check");
        lv_obj_set_x(row_check, -20);
        lv_obj_set_align(row_check, LV_ALIGN_RIGHT_MID);
        lv_obj_set_width(row_check, 42);
        lv_obj_set_height(row_check, 42);
        lv_obj_set_flag(row_check, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(row_check, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(row_check, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(row_check, ACCENT_BLUE, 0);
        lv_obj_set_style_bg_opa(row_check, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(row_check, 0, 0);
        lv_obj_set_style_radius(row_check, 21, 0);
        lv_obj_set_style_pad_all(row_check, 0, 0);
        lv_obj_t * lv_obj_0 = lv_obj_create(row_check);
        lv_obj_set_align(lv_obj_0, LV_ALIGN_CENTER);
        lv_obj_set_width(lv_obj_0, 18);
        lv_obj_set_height(lv_obj_0, 18);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 9, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);

        lv_obj_t * row_switch = lv_switch_create(lv_button_0);
        lv_obj_set_name(row_switch, "row_switch");
        lv_obj_set_x(row_switch, -20);
        lv_obj_set_align(row_switch, LV_ALIGN_RIGHT_MID);
        lv_obj_set_width(row_switch, 64);
        lv_obj_set_height(row_switch, 36);
        lv_obj_set_flag(row_switch, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_add_style(row_switch, &switch_track, 0);
        lv_obj_add_style(row_switch, &switch_on, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(row_switch, &switch_knob, LV_PART_KNOB);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

