/**
 * @file breathing_duration_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "breathing_duration_gen.h"
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

lv_obj_t * breathing_duration_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t breathing_selected;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&breathing_selected);

        lv_style_set_bg_opa(&breathing_selected, (255 * 0 / 100));
        lv_style_set_text_color(&breathing_selected, FG_PRIMARY);
        lv_style_set_text_font(&breathing_selected, metric_56);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "breathing_duration_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 374);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * breathing_minutes = lv_roller_create(lv_obj_0);
        lv_obj_set_name(breathing_minutes, "breathing_minutes");
        lv_obj_set_x(breathing_minutes, 122);
        lv_obj_set_y(breathing_minutes, 15);
        lv_obj_set_width(breathing_minutes, 100);
        lv_roller_set_options(breathing_minutes, "1\n2\n3\n4\n5", LV_ROLLER_MODE_INFINITE);
        lv_obj_set_style_bg_opa(breathing_minutes, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(breathing_minutes, 0, 0);
        lv_obj_set_style_radius(breathing_minutes, 0, 0);
        lv_obj_set_style_text_font(breathing_minutes, metric_56, 0);
        lv_obj_set_style_text_color(breathing_minutes, lv_color_hex(0x60656c), 0);
        lv_obj_set_style_text_line_space(breathing_minutes, 12, 0);
        lv_obj_set_style_text_align(breathing_minutes, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(breathing_minutes, 3);
        lv_obj_add_style(breathing_minutes, &breathing_selected, LV_PART_SELECTED);

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 226);
        lv_obj_set_y(lv_label_0, 102);
        lv_label_set_text(lv_label_0, "分钟");
        lv_obj_set_style_text_font(lv_label_0, title_24, 0);

        lv_obj_t * breathing_cycles = lv_label_create(lv_obj_0);
        lv_obj_set_name(breathing_cycles, "breathing_cycles");
        lv_obj_set_x(breathing_cycles, 24);
        lv_obj_set_y(breathing_cycles, 239);
        lv_obj_set_width(breathing_cycles, 342);
        lv_label_set_text(breathing_cycles, "呼吸7次");
        lv_obj_set_style_text_font(breathing_cycles, title_24, 0);
        lv_obj_set_style_text_color(breathing_cycles, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_text_align(breathing_cycles, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * breathing_duration_confirm = lv_button_create(lv_obj_0);
        lv_obj_set_name(breathing_duration_confirm, "breathing_duration_confirm");
        lv_obj_set_x(breathing_duration_confirm, 120);
        lv_obj_set_y(breathing_duration_confirm, 289);
        lv_obj_set_width(breathing_duration_confirm, 150);
        lv_obj_set_height(breathing_duration_confirm, 66);
        lv_obj_set_style_bg_color(breathing_duration_confirm, lv_color_hex(0x143d63), 0);
        lv_obj_set_style_radius(breathing_duration_confirm, 33, 0);
        lv_obj_set_style_shadow_width(breathing_duration_confirm, 0, 0);
        lv_obj_set_style_pad_all(breathing_duration_confirm, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(breathing_duration_confirm);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_44, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

