/**
 * @file screen_notification_detail_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_notification_detail_gen.h"
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

lv_obj_t * screen_notification_detail_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_notification_detail_#");
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

        lv_obj_t * notification_scroll = lv_obj_create(lv_obj_0);
        lv_obj_set_name(notification_scroll, "notification_scroll");
        lv_obj_set_x(notification_scroll, 24);
        lv_obj_set_y(notification_scroll, 82);
        lv_obj_set_width(notification_scroll, 342);
        lv_obj_set_height(notification_scroll, 348);
        lv_obj_set_style_layout(notification_scroll, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(notification_scroll, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_scrollbar_mode(notification_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(notification_scroll, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(notification_scroll, 0, 0);
        lv_obj_set_style_radius(notification_scroll, 0, 0);
        lv_obj_set_style_pad_all(notification_scroll, 0, 0);
        lv_obj_set_style_pad_row(notification_scroll, 18, 0);
        lv_obj_set_style_pad_bottom(notification_scroll, 20, 0);
        lv_obj_set_style_text_font(notification_scroll, notification_22, 0);
        lv_obj_set_style_text_color(notification_scroll, FG_PRIMARY, 0);
        lv_obj_t * message_source = lv_label_create(notification_scroll);
        lv_obj_set_name(message_source, "message_source");
        lv_obj_set_width(message_source, 342);
        lv_label_set_text(message_source, "QQ");
        lv_obj_set_style_text_color(message_source, ACCENT_BLUE, 0);

        lv_obj_t * message_title = lv_label_create(notification_scroll);
        lv_obj_set_name(message_title, "message_title");
        lv_obj_set_width(message_title, 342);
        lv_label_set_text(message_title, "中文通知");

        lv_obj_t * message_body = lv_label_create(notification_scroll);
        lv_obj_set_name(message_body, "message_body");
        lv_obj_set_width(message_body, 342);
        lv_label_set_text(message_body, "这是一条多行中文消息。向上滚动查看完整正文。");
        lv_obj_set_style_text_color(message_body, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

