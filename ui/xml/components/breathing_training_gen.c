/**
 * @file breathing_training_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "breathing_training_gen.h"
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

lv_obj_t * breathing_training_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "breathing_training_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 374);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * breathing_training_ring = breathing_ring_create(lv_obj_0);
        lv_obj_set_name(breathing_training_ring, "breathing_training_ring");
        lv_obj_set_x(breathing_training_ring, 125);
        lv_obj_set_y(breathing_training_ring, 74);
        lv_obj_set_width(breathing_training_ring, 140);
        lv_obj_set_height(breathing_training_ring, 140);

        lv_obj_t * breathing_guide = lv_label_create(lv_obj_0);
        lv_obj_set_name(breathing_guide, "breathing_guide");
        lv_obj_set_x(breathing_guide, 24);
        lv_obj_set_y(breathing_guide, 272);
        lv_obj_set_width(breathing_guide, 342);
        lv_label_set_text(breathing_guide, "吸气");
        lv_obj_set_style_text_font(breathing_guide, guide_40, 0);
        lv_obj_set_style_text_align(breathing_guide, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 185);
        lv_obj_set_y(lv_obj_1, 346);
        lv_obj_set_width(lv_obj_1, 8);
        lv_obj_set_height(lv_obj_1, 8);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_1, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(lv_obj_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_radius(lv_obj_1, 4, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 201);
        lv_obj_set_y(lv_obj_2, 346);
        lv_obj_set_width(lv_obj_2, 8);
        lv_obj_set_height(lv_obj_2, 8);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_2, lv_color_hex(0x164e80), 0);
        lv_obj_set_style_bg_opa(lv_obj_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_2, 0, 0);
        lv_obj_set_style_radius(lv_obj_2, 4, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

