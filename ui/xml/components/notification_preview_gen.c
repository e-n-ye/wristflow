/**
 * @file notification_preview_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "notification_preview_gen.h"
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

lv_obj_t * notification_preview_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "notification_preview_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_0, 24);
        lv_obj_set_y(lv_label_0, 26);
        lv_label_set_text(lv_label_0, "新消息");
        lv_obj_set_style_text_color(lv_label_0, FG_SECONDARY, 0);

        lv_obj_t * preview_open = lv_button_create(lv_obj_0);
        lv_obj_set_name(preview_open, "preview_open");
        lv_obj_set_x(preview_open, 24);
        lv_obj_set_y(preview_open, 86);
        lv_obj_set_width(preview_open, 342);
        lv_obj_set_height(preview_open, 248);
        lv_obj_set_flag(preview_open, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(preview_open, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(preview_open, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(preview_open, 0, 0);
        lv_obj_set_style_radius(preview_open, 28, 0);
        lv_obj_set_style_pad_all(preview_open, 0, 0);
        lv_obj_set_style_shadow_width(preview_open, 0, 0);
        lv_obj_set_style_text_color(preview_open, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(preview_open, notification_22, 0);
        lv_obj_t * message_source = lv_label_create(preview_open);
        lv_obj_set_name(message_source, "message_source");
        lv_obj_set_x(message_source, 20);
        lv_obj_set_y(message_source, 18);
        lv_obj_set_width(message_source, 302);
        lv_obj_set_height(message_source, 34);
        lv_label_set_long_mode(message_source, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_source, "QQ");
        lv_obj_set_style_text_color(message_source, ACCENT_BLUE, 0);

        lv_obj_t * message_title = lv_label_create(preview_open);
        lv_obj_set_name(message_title, "message_title");
        lv_obj_set_x(message_title, 20);
        lv_obj_set_y(message_title, 68);
        lv_obj_set_width(message_title, 302);
        lv_obj_set_height(message_title, 68);
        lv_label_set_long_mode(message_title, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_title, "中文通知");

        lv_obj_t * message_body = lv_label_create(preview_open);
        lv_obj_set_name(message_body, "message_body");
        lv_obj_set_x(message_body, 20);
        lv_obj_set_y(message_body, 148);
        lv_obj_set_width(message_body, 302);
        lv_obj_set_height(message_body, 72);
        lv_label_set_long_mode(message_body, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_body, "下午一起去图书馆。");
        lv_obj_set_style_text_color(message_body, FG_SECONDARY, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_obj_set_align(lv_label_1, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(lv_label_1, -48);
        lv_label_set_text(lv_label_1, "轻点查看 · 上滑收起");
        lv_obj_set_style_text_color(lv_label_1, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

