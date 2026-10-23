/**
 * @file aod_thumbnail_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "aod_thumbnail_gen.h"
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

lv_obj_t * aod_thumbnail_create(lv_obj_t * parent, bool center_hidden, bool corner_hidden)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "aod_thumbnail_#");
        lv_obj_set_y(lv_obj_0, 6);
        lv_obj_set_width(lv_obj_0, 234);
        lv_obj_set_height(lv_obj_0, 286);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_border_width(lv_obj_0, 1, 0);
        lv_obj_set_style_border_color(lv_obj_0, lv_color_hex(0x454a50), 0);
        lv_obj_set_style_radius(lv_obj_0, 28, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * aod_thumb_center = lv_obj_create(lv_obj_0);
        lv_obj_set_name(aod_thumb_center, "aod_thumb_center");
        lv_obj_set_x(aod_thumb_center, 2);
        lv_obj_set_y(aod_thumb_center, 8);
        lv_obj_set_width(aod_thumb_center, 228);
        lv_obj_set_height(aod_thumb_center, 264);
        lv_obj_set_flag(aod_thumb_center, LV_OBJ_FLAG_HIDDEN, center_hidden);
        lv_obj_set_flag(aod_thumb_center, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(aod_thumb_center, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(aod_thumb_center, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(aod_thumb_center, 0, 0);
        lv_obj_set_style_pad_all(aod_thumb_center, 0, 0);
        lv_obj_set_style_radius(aod_thumb_center, 0, 0);
        lv_obj_set_style_text_color(aod_thumb_center, FG_PRIMARY, 0);
        lv_obj_t * lv_label_0 = lv_label_create(aod_thumb_center);
        lv_obj_set_x(lv_label_0, 12);
        lv_obj_set_y(lv_label_0, 91);
        lv_obj_set_width(lv_label_0, 204);
        lv_label_set_text(lv_label_0, "09:28");
        lv_obj_set_style_text_font(lv_label_0, aod_preview_48, 0);
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_label_1 = lv_label_create(aod_thumb_center);
        lv_obj_set_x(lv_label_1, 14);
        lv_obj_set_y(lv_label_1, 146);
        lv_obj_set_width(lv_label_1, 200);
        lv_label_set_text(lv_label_1, "5月18日 周一");
        lv_obj_set_style_text_font(lv_label_1, aod_preview_14, 0);
        lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * aod_thumb_corner = lv_obj_create(lv_obj_0);
        lv_obj_set_name(aod_thumb_corner, "aod_thumb_corner");
        lv_obj_set_x(aod_thumb_corner, 2);
        lv_obj_set_y(aod_thumb_corner, 8);
        lv_obj_set_width(aod_thumb_corner, 228);
        lv_obj_set_height(aod_thumb_corner, 264);
        lv_obj_set_flag(aod_thumb_corner, LV_OBJ_FLAG_HIDDEN, corner_hidden);
        lv_obj_set_flag(aod_thumb_corner, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(aod_thumb_corner, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(aod_thumb_corner, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(aod_thumb_corner, 0, 0);
        lv_obj_set_style_pad_all(aod_thumb_corner, 0, 0);
        lv_obj_set_style_radius(aod_thumb_corner, 0, 0);
        lv_obj_set_style_text_color(aod_thumb_corner, FG_PRIMARY, 0);
        lv_obj_t * lv_label_2 = lv_label_create(aod_thumb_corner);
        lv_obj_set_x(lv_label_2, 25);
        lv_obj_set_y(lv_label_2, 39);
        lv_obj_set_width(lv_label_2, 182);
        lv_label_set_text(lv_label_2, "09:28");
        lv_obj_set_style_text_font(lv_label_2, aod_preview_32, 0);

        lv_obj_t * lv_label_3 = lv_label_create(aod_thumb_corner);
        lv_obj_set_x(lv_label_3, 26);
        lv_obj_set_y(lv_label_3, 80);
        lv_obj_set_width(lv_label_3, 111);
        lv_label_set_text(lv_label_3, "5/18");
        lv_obj_set_style_text_font(lv_label_3, aod_preview_32, 0);

        lv_obj_t * lv_label_4 = lv_label_create(aod_thumb_corner);
        lv_obj_set_x(lv_label_4, 140);
        lv_obj_set_y(lv_label_4, 94);
        lv_obj_set_width(lv_label_4, 58);
        lv_label_set_text(lv_label_4, "周一");
        lv_obj_set_style_text_font(lv_label_4, aod_preview_14, 0);

        lv_obj_t * lv_label_5 = lv_label_create(aod_thumb_corner);
        lv_obj_set_x(lv_label_5, 26);
        lv_obj_set_y(lv_label_5, 124);
        lv_obj_set_width(lv_label_5, 175);
        lv_label_set_text(lv_label_5, "~83%");
        lv_obj_set_style_text_font(lv_label_5, aod_preview_32, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

