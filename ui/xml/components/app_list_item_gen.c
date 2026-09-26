/**
 * @file app_list_item_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "app_list_item_gen.h"
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

lv_obj_t * app_list_item_create(lv_obj_t * parent, const char * title, const char * icon, lv_color_t tint)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "app_list_item_#");
        lv_obj_set_width(lv_button_0, 342);
        lv_obj_set_height(lv_button_0, 88);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_button_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_button_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_button_0, 0, 0);
        lv_obj_set_style_radius(lv_button_0, 0, 0);
        lv_obj_set_style_pad_all(lv_button_0, 0, 0);
        lv_obj_set_style_shadow_width(lv_button_0, 0, 0);

        lv_obj_t * menu_circle = lv_obj_create(lv_button_0);
        lv_obj_set_name(menu_circle, "menu_circle");
        lv_obj_set_x(menu_circle, 0);
        lv_obj_set_y(menu_circle, 4);
        lv_obj_set_width(menu_circle, 80);
        lv_obj_set_height(menu_circle, 80);
        lv_obj_set_flag(menu_circle, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(menu_circle, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(menu_circle, tint, 0);
        lv_obj_set_style_bg_opa(menu_circle, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(menu_circle, 0, 0);
        lv_obj_set_style_radius(menu_circle, 999, 0);
        lv_obj_set_style_pad_all(menu_circle, 0, 0);
        lv_obj_t * launcher_icon_label = lv_label_create(menu_circle);
        lv_obj_set_name(launcher_icon_label, "launcher_icon_label");
        lv_obj_set_align(launcher_icon_label, LV_ALIGN_CENTER);
        lv_label_set_text(launcher_icon_label, icon);
        lv_obj_set_style_text_font(launcher_icon_label, icons_44, 0);
        lv_obj_set_style_text_color(launcher_icon_label, FG_PRIMARY, 0);

        lv_obj_t * menu_title = lv_label_create(lv_button_0);
        lv_obj_set_name(menu_title, "menu_title");
        lv_obj_set_x(menu_title, 100);
        lv_obj_set_y(menu_title, 27);
        lv_obj_set_width(menu_title, 242);
        lv_label_set_text(menu_title, title);
        lv_obj_set_style_text_font(menu_title, title_24, 0);
        lv_obj_set_style_text_color(menu_title, FG_PRIMARY, 0);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

