/**
 * @file notification_banner_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "notification_banner_gen.h"
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

lv_obj_t * notification_banner_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "notification_banner_#");
        lv_obj_set_x(lv_button_0, 16);
        lv_obj_set_y(lv_button_0, 16);
        lv_obj_set_width(lv_button_0, 358);
        lv_obj_set_height(lv_button_0, 124);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, lv_color_hex(0x25292e), 0);
        lv_obj_set_style_bg_opa(lv_button_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_color(lv_button_0, lv_color_hex(0x4b5563), 0);
        lv_obj_set_style_border_width(lv_button_0, 1, 0);
        lv_obj_set_style_radius(lv_button_0, 24, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);
        lv_obj_set_style_text_color(lv_button_0, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(lv_button_0, notification_22, 0);

        lv_obj_t * message_source = lv_label_create(lv_button_0);
        lv_obj_set_name(message_source, "message_source");
        lv_obj_set_x(message_source, 18);
        lv_obj_set_y(message_source, 4);
        lv_obj_set_width(message_source, 322);
        lv_obj_set_height(message_source, 34);
        lv_label_set_long_mode(message_source, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_source, "QQ");
        lv_obj_set_style_text_color(message_source, ACCENT_BLUE, 0);

        lv_obj_t * message_title = lv_label_create(lv_button_0);
        lv_obj_set_name(message_title, "message_title");
        lv_obj_set_x(message_title, 18);
        lv_obj_set_y(message_title, 41);
        lv_obj_set_width(message_title, 322);
        lv_obj_set_height(message_title, 34);
        lv_label_set_long_mode(message_title, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_title, "中文通知");

        lv_obj_t * message_body = lv_label_create(lv_button_0);
        lv_obj_set_name(message_body, "message_body");
        lv_obj_set_x(message_body, 18);
        lv_obj_set_y(message_body, 78);
        lv_obj_set_width(message_body, 322);
        lv_obj_set_height(message_body, 34);
        lv_label_set_long_mode(message_body, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(message_body, "下午一起去图书馆。");
        lv_obj_set_style_text_color(message_body, FG_SECONDARY, 0);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

