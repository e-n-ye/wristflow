/**
 * @file screen_face_picker_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_face_picker_gen.h"
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

lv_obj_t * screen_face_picker_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_face_picker_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        app_header_create(lv_obj_0, "选择表盘");

        lv_obj_t * face_preview = lv_obj_create(lv_obj_0);
        lv_obj_set_name(face_preview, "face_preview");
        lv_obj_set_x(face_preview, 78);
        lv_obj_set_y(face_preview, 82);
        lv_obj_set_width(face_preview, 234);
        lv_obj_set_height(face_preview, 270);
        lv_obj_set_flag(face_preview, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(face_preview, BG_BLACK, 0);
        lv_obj_set_style_border_width(face_preview, 1, 0);
        lv_obj_set_style_border_color(face_preview, lv_color_hex(0x454a50), 0);
        lv_obj_set_style_radius(face_preview, 8, 0);
        lv_obj_set_style_pad_all(face_preview, 0, 0);
        lv_obj_t * face_preview_hour = lv_image_create(face_preview);
        lv_obj_set_name(face_preview_hour, "face_preview_hour");
        lv_obj_set_x(face_preview_hour, 31);
        lv_obj_set_y(face_preview_hour, 21);
        lv_obj_set_width(face_preview_hour, 171);
        lv_obj_set_height(face_preview_hour, 141);
        lv_image_set_src(face_preview_hour, hour_10);
        lv_image_set_inner_align(face_preview_hour, LV_IMAGE_ALIGN_STRETCH);

        lv_obj_t * face_preview_minute = lv_label_create(face_preview);
        lv_obj_set_name(face_preview_minute, "face_preview_minute");
        lv_obj_set_x(face_preview_minute, 0);
        lv_obj_set_y(face_preview_minute, 206);
        lv_obj_set_width(face_preview_minute, 234);
        lv_label_set_text(face_preview_minute, "48");
        lv_obj_set_style_text_font(face_preview_minute, value_36, 0);
        lv_obj_set_style_text_align(face_preview_minute, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * face_preview_simple = lv_label_create(face_preview);
        lv_obj_set_name(face_preview_simple, "face_preview_simple");
        lv_obj_set_x(face_preview_simple, 0);
        lv_obj_set_y(face_preview_simple, 102);
        lv_obj_set_width(face_preview_simple, 234);
        lv_label_set_text(face_preview_simple, "22:48");
        lv_obj_set_style_text_font(face_preview_simple, value_36, 0);
        lv_obj_set_style_text_align(face_preview_simple, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_flag(face_preview_simple, LV_OBJ_FLAG_HIDDEN, true);

        lv_obj_t * face_previous = lv_button_create(lv_obj_0);
        lv_obj_set_name(face_previous, "face_previous");
        lv_obj_set_x(face_previous, 12);
        lv_obj_set_y(face_previous, 181);
        lv_obj_set_width(face_previous, 54);
        lv_obj_set_height(face_previous, 64);
        lv_obj_set_style_bg_opa(face_previous, (255 * 0 / 100), 0);
        lv_obj_set_style_shadow_width(face_previous, 0, 0);
        lv_obj_set_style_pad_all(face_previous, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(face_previous);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);

        lv_obj_t * face_next = lv_button_create(lv_obj_0);
        lv_obj_set_name(face_next, "face_next");
        lv_obj_set_x(face_next, 324);
        lv_obj_set_y(face_next, 181);
        lv_obj_set_width(face_next, 54);
        lv_obj_set_height(face_next, 64);
        lv_obj_set_style_bg_opa(face_next, (255 * 0 / 100), 0);
        lv_obj_set_style_shadow_width(face_next, 0, 0);
        lv_obj_set_style_pad_all(face_next, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(face_next);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_20, 0);

        lv_obj_t * face_name = lv_label_create(lv_obj_0);
        lv_obj_set_name(face_name, "face_name");
        lv_obj_set_x(face_name, 24);
        lv_obj_set_y(face_name, 365);
        lv_obj_set_width(face_name, 240);
        lv_label_set_text(face_name, "扩散");
        lv_obj_set_style_text_font(face_name, title_24, 0);

        lv_obj_t * face_selected = lv_label_create(lv_obj_0);
        lv_obj_set_name(face_selected, "face_selected");
        lv_obj_set_x(face_selected, 24);
        lv_obj_set_y(face_selected, 401);
        lv_obj_set_width(face_selected, 240);
        lv_label_set_text(face_selected, "已选");
        lv_obj_set_style_text_color(face_selected, FG_SECONDARY, 0);

        lv_obj_t * face_apply = lv_button_create(lv_obj_0);
        lv_obj_set_name(face_apply, "face_apply");
        lv_obj_set_x(face_apply, 288);
        lv_obj_set_y(face_apply, 365);
        lv_obj_set_width(face_apply, 72);
        lv_obj_set_height(face_apply, 64);
        lv_obj_set_style_radius(face_apply, 32, 0);
        lv_obj_set_style_bg_color(face_apply, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_color(face_apply, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_width(face_apply, 0, 0);
        lv_obj_set_style_pad_all(face_apply, 0, 0);
        lv_obj_t * lv_label_2 = lv_label_create(face_apply);
        lv_obj_set_align(lv_label_2, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_2, "");
        lv_obj_set_style_text_font(lv_label_2, icons_20, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

