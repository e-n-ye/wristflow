/**
 * @file time_picker_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "time_picker_gen.h"
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

lv_obj_t * time_picker_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t time_wheel_selected;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&time_wheel_selected);

        lv_style_set_bg_opa(&time_wheel_selected, (255 * 0 / 100));
        lv_style_set_text_color(&time_wheel_selected, FG_PRIMARY);
        lv_style_set_text_font(&time_wheel_selected, metric_56);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "time_picker_#");
        lv_obj_set_x(lv_obj_0, 0);
        lv_obj_set_y(lv_obj_0, 78);
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 350);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * time_hours = lv_roller_create(lv_obj_0);
        lv_obj_set_name(time_hours, "time_hours");
        lv_obj_set_x(time_hours, 60);
        lv_obj_set_y(time_hours, 46);
        lv_obj_set_width(time_hours, 110);
        lv_roller_set_options(time_hours, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23", LV_ROLLER_MODE_INFINITE);
        lv_obj_set_style_bg_opa(time_hours, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(time_hours, 0, 0);
        lv_obj_set_style_radius(time_hours, 0, 0);
        lv_obj_set_style_text_font(time_hours, value_36, 0);
        lv_obj_set_style_text_color(time_hours, lv_color_hex(0x737980), 0);
        lv_obj_set_style_text_line_space(time_hours, 22, 0);
        lv_obj_set_style_text_align(time_hours, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(time_hours, 3);
        lv_obj_add_style(time_hours, &time_wheel_selected, LV_PART_SELECTED);

        lv_obj_t * time_minutes = lv_roller_create(lv_obj_0);
        lv_obj_set_name(time_minutes, "time_minutes");
        lv_obj_set_x(time_minutes, 220);
        lv_obj_set_y(time_minutes, 46);
        lv_obj_set_width(time_minutes, 110);
        lv_roller_set_options(time_minutes, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_INFINITE);
        lv_obj_set_style_bg_opa(time_minutes, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(time_minutes, 0, 0);
        lv_obj_set_style_radius(time_minutes, 0, 0);
        lv_obj_set_style_text_font(time_minutes, value_36, 0);
        lv_obj_set_style_text_color(time_minutes, lv_color_hex(0x737980), 0);
        lv_obj_set_style_text_line_space(time_minutes, 22, 0);
        lv_obj_set_style_text_align(time_minutes, LV_TEXT_ALIGN_CENTER, 0);
        lv_roller_set_visible_row_count(time_minutes, 3);
        lv_obj_add_style(time_minutes, &time_wheel_selected, LV_PART_SELECTED);

        lv_obj_t * time_separator = lv_label_create(lv_obj_0);
        lv_obj_set_name(time_separator, "time_separator");
        lv_obj_set_x(time_separator, 180);
        lv_obj_set_y(time_separator, 121);
        lv_obj_set_width(time_separator, 30);
        lv_label_set_text(time_separator, ":");
        lv_obj_set_style_text_font(time_separator, value_36, 0);
        lv_obj_set_style_text_color(time_separator, FG_PRIMARY, 0);
        lv_obj_set_style_text_align(time_separator, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * time_accept = lv_button_create(lv_obj_0);
        lv_obj_set_name(time_accept, "time_accept");
        lv_obj_set_x(time_accept, 120);
        lv_obj_set_y(time_accept, 279);
        lv_obj_set_width(time_accept, 150);
        lv_obj_set_height(time_accept, 66);
        lv_obj_set_style_bg_color(time_accept, lv_color_hex(0x008cff), 0);
        lv_obj_set_style_radius(time_accept, 33, 0);
        lv_obj_set_style_shadow_width(time_accept, 0, 0);
        lv_obj_set_style_pad_all(time_accept, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(time_accept);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_44, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_PRIMARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

