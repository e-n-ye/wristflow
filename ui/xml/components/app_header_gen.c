/**
 * @file app_header_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "app_header_gen.h"
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

lv_obj_t * app_header_create(lv_obj_t * parent, const char * title)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "app_header_#");
        lv_obj_set_x(lv_obj_0, 24);
        lv_obj_set_y(lv_obj_0, 18);
        lv_obj_set_width(lv_obj_0, 342);
        lv_obj_set_height(lv_obj_0, 48);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * app_back = lv_button_create(lv_obj_0);
        lv_obj_set_name(app_back, "app_back");
        lv_obj_set_x(app_back, 0);
        lv_obj_set_y(app_back, 0);
        lv_obj_set_width(app_back, 48);
        lv_obj_set_height(app_back, 48);
        lv_obj_set_style_bg_opa(app_back, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(app_back, 0, 0);
        lv_obj_set_style_shadow_width(app_back, 0, 0);
        lv_obj_set_style_pad_all(app_back, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(app_back);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_1, 58);
        lv_obj_set_y(lv_label_1, 7);
        lv_obj_set_width(lv_label_1, 280);
        lv_label_set_text(lv_label_1, title);
        lv_obj_set_style_text_font(lv_label_1, title_24, 0);
        lv_obj_set_style_text_color(lv_label_1, FG_PRIMARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

