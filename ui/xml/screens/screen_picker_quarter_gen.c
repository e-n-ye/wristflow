/**
 * @file screen_picker_quarter_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_picker_quarter_gen.h"
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

lv_obj_t * screen_picker_quarter_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_picker_quarter_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * picker_header_0 = picker_header_create(lv_obj_0, "选择组件");
        lv_obj_set_x(picker_header_0, 24);
        lv_obj_set_y(picker_header_0, 20);

        lv_obj_t * choices = picker_list_create(lv_obj_0);
        lv_obj_set_name(choices, "choices");
        lv_obj_set_x(choices, 24);
        lv_obj_set_y(choices, 74);
        lv_obj_t * lv_label_0 = lv_label_create(choices);
        lv_obj_set_x(lv_label_0, 0);
        lv_obj_set_y(lv_label_0, 0);
        lv_obj_set_width(lv_label_0, 342);
        lv_label_set_text(lv_label_0, "心率");
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(lv_label_0, body_20, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_SECONDARY, 0);

        lv_obj_t * heart_dark = card_container_create(choices, "心率", "68", "", BG_SURFACE, FG_PRIMARY, FG_SECONDARY, lv_color_hex(0xff5678));
        lv_obj_set_name(heart_dark, "heart_dark");
        lv_obj_set_x(heart_dark, 0);
        lv_obj_set_y(heart_dark, 36);

        lv_obj_t * heart_color = card_container_create(choices, "心率", "68", "", lv_color_hex(0xff648a), lv_color_hex(0x24111b), lv_color_hex(0x4c2435), lv_color_hex(0x24111b));
        lv_obj_set_name(heart_color, "heart_color");
        lv_obj_set_x(heart_color, 177);
        lv_obj_set_y(heart_color, 36);

        lv_obj_t * lv_label_1 = lv_label_create(choices);
        lv_obj_set_x(lv_label_1, 0);
        lv_obj_set_y(lv_label_1, 234);
        lv_obj_set_width(lv_label_1, 342);
        lv_label_set_text(lv_label_1, "电池");
        lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(lv_label_1, body_20, 0);
        lv_obj_set_style_text_color(lv_label_1, FG_SECONDARY, 0);

        lv_obj_t * battery_dark = card_container_create(choices, "电池", "53%", "", BG_SURFACE, FG_PRIMARY, FG_SECONDARY, lv_color_hex(0x67e1f7));
        lv_obj_set_name(battery_dark, "battery_dark");
        lv_obj_set_x(battery_dark, 0);
        lv_obj_set_y(battery_dark, 270);

        lv_obj_t * battery_color = card_container_create(choices, "电池", "53%", "", lv_color_hex(0x67e1f7), lv_color_hex(0x09272c), lv_color_hex(0x234b54), lv_color_hex(0x09272c));
        lv_obj_set_name(battery_color, "battery_color");
        lv_obj_set_x(battery_color, 177);
        lv_obj_set_y(battery_color, 270);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

