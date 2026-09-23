/**
 * @file screen_picker_full_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_picker_full_gen.h"
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

lv_obj_t * screen_picker_full_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_picker_full_#");
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
        lv_obj_t * activity_full = metric_full_create(choices, "今日步数", "6,280", "目标 8,000 步", "", ACCENT_GREEN);
        lv_obj_set_name(activity_full, "activity_full");
        lv_obj_set_x(activity_full, 0);
        lv_obj_set_y(activity_full, 0);

        lv_obj_t * battery_full = metric_full_create(choices, "电池", "53%", "未充电", "", lv_color_hex(0x67e1f7));
        lv_obj_set_name(battery_full, "battery_full");
        lv_obj_set_x(battery_full, 0);
        lv_obj_set_y(battery_full, 384);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

