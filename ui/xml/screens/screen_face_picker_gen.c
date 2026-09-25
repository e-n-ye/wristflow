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

        lv_obj_t * face_name = lv_label_create(lv_obj_0);
        lv_obj_set_name(face_name, "face_name");
        lv_obj_set_x(face_name, 24);
        lv_obj_set_y(face_name, 28);
        lv_obj_set_width(face_name, 342);
        lv_label_set_text(face_name, "扩散");
        lv_obj_set_style_text_font(face_name, title_24, 0);
        lv_obj_set_style_text_align(face_name, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * face_carousel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(face_carousel, "face_carousel");
        lv_obj_set_x(face_carousel, 0);
        lv_obj_set_y(face_carousel, 80);
        lv_obj_set_width(face_carousel, 390);
        lv_obj_set_height(face_carousel, 304);
        lv_obj_set_scrollbar_mode(face_carousel, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(face_carousel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(face_carousel, 0, 0);
        lv_obj_set_style_pad_all(face_carousel, 0, 0);
        lv_obj_set_style_radius(face_carousel, 0, 0);
        lv_obj_t * face_slot_0 = face_thumbnail_create(face_carousel, true);
        lv_obj_set_name(face_slot_0, "face_slot_0");
        lv_obj_set_x(face_slot_0, 78);

        lv_obj_t * face_slot_1 = face_thumbnail_create(face_carousel, false);
        lv_obj_set_name(face_slot_1, "face_slot_1");
        lv_obj_set_x(face_slot_1, 344);

        lv_obj_t * face_slot_2 = face_thumbnail_create(face_carousel, true);
        lv_obj_set_name(face_slot_2, "face_slot_2");
        lv_obj_set_x(face_slot_2, 610);

        lv_obj_t * face_slot_3 = face_thumbnail_create(face_carousel, false);
        lv_obj_set_name(face_slot_3, "face_slot_3");
        lv_obj_set_x(face_slot_3, 876);

        lv_obj_t * lv_obj_1 = lv_obj_create(face_carousel);
        lv_obj_set_x(lv_obj_1, 1110);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_set_width(lv_obj_1, 78);
        lv_obj_set_height(lv_obj_1, 1);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_1, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);

        lv_obj_t * face_selected = lv_label_create(lv_obj_0);
        lv_obj_set_name(face_selected, "face_selected");
        lv_obj_set_x(face_selected, 24);
        lv_obj_set_y(face_selected, 407);
        lv_obj_set_width(face_selected, 342);
        lv_label_set_text(face_selected, "已选");
        lv_obj_set_style_text_color(face_selected, FG_SECONDARY, 0);
        lv_obj_set_style_text_align(face_selected, LV_TEXT_ALIGN_CENTER, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

