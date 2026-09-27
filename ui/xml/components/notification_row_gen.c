/**
 * @file notification_row_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "notification_row_gen.h"
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

lv_obj_t * notification_row_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "notification_row_#");
        lv_obj_set_width(lv_obj_0, 342);
        lv_obj_set_height(lv_obj_0, 142);
        lv_obj_set_scrollbar_mode(lv_obj_0, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_layout(lv_obj_0, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_style_pad_column(lv_obj_0, 12, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);

        lv_obj_t * notification_row_body = lv_button_create(lv_obj_0);
        lv_obj_set_name(notification_row_body, "notification_row_body");
        lv_obj_set_width(notification_row_body, 342);
        lv_obj_set_height(notification_row_body, 142);
        lv_obj_set_flag(notification_row_body, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(notification_row_body, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(notification_row_body, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(notification_row_body, 0, 0);
        lv_obj_set_style_radius(notification_row_body, 22, 0);
        lv_obj_set_style_pad_all(notification_row_body, 0, 0);
        lv_obj_set_style_shadow_width(notification_row_body, 0, 0);
        lv_obj_t * message_source = lv_label_create(notification_row_body);
        lv_obj_set_name(message_source, "message_source");
        lv_obj_set_x(message_source, 18);
        lv_obj_set_y(message_source, 8);
        lv_obj_set_width(message_source, 306);
        lv_obj_set_height(message_source, 34);
        lv_label_set_long_mode(message_source, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_source, "QQ");
        lv_obj_set_style_text_color(message_source, ACCENT_BLUE, 0);

        lv_obj_t * message_title = lv_label_create(notification_row_body);
        lv_obj_set_name(message_title, "message_title");
        lv_obj_set_x(message_title, 18);
        lv_obj_set_y(message_title, 52);
        lv_obj_set_width(message_title, 306);
        lv_obj_set_height(message_title, 34);
        lv_label_set_long_mode(message_title, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_title, "中文通知");

        lv_obj_t * message_body = lv_label_create(notification_row_body);
        lv_obj_set_name(message_body, "message_body");
        lv_obj_set_x(message_body, 18);
        lv_obj_set_y(message_body, 96);
        lv_obj_set_width(message_body, 306);
        lv_obj_set_height(message_body, 34);
        lv_label_set_long_mode(message_body, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_body, "下午一起去图书馆。");
        lv_obj_set_style_text_color(message_body, FG_SECONDARY, 0);

        lv_obj_t * notification_row_delete = lv_button_create(lv_obj_0);
        lv_obj_set_name(notification_row_delete, "notification_row_delete");
        lv_obj_set_width(notification_row_delete, 130);
        lv_obj_set_height(notification_row_delete, 142);
        lv_obj_set_flag(notification_row_delete, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(notification_row_delete, ACCENT_BLUE, 0);
        lv_obj_set_style_bg_opa(notification_row_delete, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(notification_row_delete, 0, 0);
        lv_obj_set_style_radius(notification_row_delete, 22, 0);
        lv_obj_set_style_pad_all(notification_row_delete, 0, 0);
        lv_obj_set_style_shadow_width(notification_row_delete, 0, 0);
        lv_obj_set_style_text_font(notification_row_delete, icons_44, 0);
        lv_obj_set_style_text_color(notification_row_delete, FG_PRIMARY, 0);
        lv_obj_t * lv_label_0 = lv_label_create(notification_row_delete);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

