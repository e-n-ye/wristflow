/**
 * @file screen_aod_styles_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_aod_styles_gen.h"
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

lv_obj_t * screen_aod_styles_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_aod_styles_#");
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

        app_header_create(lv_obj_0, "默认息屏样式");

        lv_obj_t * aod_carousel = lv_obj_create(lv_obj_0);
        lv_obj_set_name(aod_carousel, "aod_carousel");
        lv_obj_set_x(aod_carousel, 0);
        lv_obj_set_y(aod_carousel, 84);
        lv_obj_set_width(aod_carousel, 390);
        lv_obj_set_height(aod_carousel, 304);
        lv_obj_set_scrollbar_mode(aod_carousel, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(aod_carousel, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(aod_carousel, 0, 0);
        lv_obj_set_style_pad_all(aod_carousel, 0, 0);
        lv_obj_set_style_radius(aod_carousel, 0, 0);
        lv_obj_t * aod_style_0 = aod_thumbnail_create(aod_carousel, false, true);
        lv_obj_set_name(aod_style_0, "aod_style_0");
        lv_obj_set_x(aod_style_0, 78);

        lv_obj_t * aod_style_1 = aod_thumbnail_create(aod_carousel, true, false);
        lv_obj_set_name(aod_style_1, "aod_style_1");
        lv_obj_set_x(aod_style_1, 344);

        lv_obj_t * aod_carousel_pad = lv_obj_create(aod_carousel);
        lv_obj_set_name(aod_carousel_pad, "aod_carousel_pad");
        lv_obj_set_x(aod_carousel_pad, 578);
        lv_obj_set_y(aod_carousel_pad, 0);
        lv_obj_set_width(aod_carousel_pad, 78);
        lv_obj_set_height(aod_carousel_pad, 1);
        lv_obj_set_flag(aod_carousel_pad, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(aod_carousel_pad, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(aod_carousel_pad, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(aod_carousel_pad, 0, 0);

        lv_obj_t * aod_style_name = lv_label_create(lv_obj_0);
        lv_obj_set_name(aod_style_name, "aod_style_name");
        lv_obj_set_x(aod_style_name, 24);
        lv_obj_set_y(aod_style_name, 401);
        lv_obj_set_width(aod_style_name, 342);
        lv_label_set_text(aod_style_name, "居中数字 · 已选");
        lv_obj_set_style_text_align(aod_style_name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(aod_style_name, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

