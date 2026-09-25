/**
 * @file face_thumbnail_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "face_thumbnail_gen.h"
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

lv_obj_t * face_thumbnail_create(lv_obj_t * parent, bool simple)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "face_thumbnail_#");
        lv_obj_set_y(lv_obj_0, 12);
        lv_obj_set_width(lv_obj_0, 234);
        lv_obj_set_height(lv_obj_0, 280);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_border_width(lv_obj_0, 1, 0);
        lv_obj_set_style_border_color(lv_obj_0, lv_color_hex(0x454a50), 0);
        lv_obj_set_style_radius(lv_obj_0, 24, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);

        lv_obj_t * thumb_hour = lv_image_create(lv_obj_0);
        lv_obj_set_name(thumb_hour, "thumb_hour");
        lv_obj_set_x(thumb_hour, 31);
        lv_obj_set_y(thumb_hour, 26);
        lv_obj_set_width(thumb_hour, 171);
        lv_obj_set_height(thumb_hour, 141);
        lv_image_set_src(thumb_hour, hour_10);
        lv_image_set_inner_align(thumb_hour, LV_IMAGE_ALIGN_STRETCH);
        lv_obj_set_flag(thumb_hour, LV_OBJ_FLAG_HIDDEN, simple);

        lv_obj_t * thumb_minute = lv_label_create(lv_obj_0);
        lv_obj_set_name(thumb_minute, "thumb_minute");
        lv_obj_set_x(thumb_minute, 0);
        lv_obj_set_y(thumb_minute, 216);
        lv_obj_set_width(thumb_minute, 234);
        lv_label_set_text(thumb_minute, "48");
        lv_obj_set_style_text_font(thumb_minute, value_36, 0);
        lv_obj_set_style_text_align(thumb_minute, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_flag(thumb_minute, LV_OBJ_FLAG_HIDDEN, simple);

        lv_obj_t * thumb_simple = lv_label_create(lv_obj_0);
        lv_obj_set_name(thumb_simple, "thumb_simple");
        lv_obj_set_x(thumb_simple, 0);
        lv_obj_set_y(thumb_simple, 111);
        lv_obj_set_width(thumb_simple, 234);
        lv_label_set_text(thumb_simple, "22:48");
        lv_obj_set_style_text_font(thumb_simple, value_36, 0);
        lv_obj_set_style_text_align(thumb_simple, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_flag(thumb_simple, LV_OBJ_FLAG_HIDDEN, true);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

