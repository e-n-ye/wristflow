/**
 * @file screen_notifications_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_notifications_gen.h"
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

lv_obj_t * screen_notifications_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_notifications_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        app_header_create(lv_obj_0, "消息通知");

        lv_obj_t * notification_list = lv_obj_create(lv_obj_0);
        lv_obj_set_name(notification_list, "notification_list");
        lv_obj_set_x(notification_list, 24);
        lv_obj_set_y(notification_list, 82);
        lv_obj_set_width(notification_list, 342);
        lv_obj_set_height(notification_list, 348);
        lv_obj_set_style_layout(notification_list, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(notification_list, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_scrollbar_mode(notification_list, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(notification_list, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(notification_list, 0, 0);
        lv_obj_set_style_radius(notification_list, 0, 0);
        lv_obj_set_style_pad_all(notification_list, 0, 0);
        lv_obj_set_style_pad_row(notification_list, 12, 0);
        lv_obj_set_style_pad_bottom(notification_list, 20, 0);
        lv_obj_t * sample_message = notification_row_create(notification_list);
        lv_obj_set_name(sample_message, "sample_message");

        lv_obj_t * notification_empty = lv_label_create(lv_obj_0);
        lv_obj_set_name(notification_empty, "notification_empty");
        lv_obj_set_align(notification_empty, LV_ALIGN_CENTER);
        lv_obj_set_width(notification_empty, 342);
        lv_obj_set_style_text_align(notification_empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(notification_empty, "暂无消息");
        lv_obj_set_flag(notification_empty, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_style_text_color(notification_empty, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

