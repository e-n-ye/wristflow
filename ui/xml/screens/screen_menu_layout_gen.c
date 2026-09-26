/**
 * @file screen_menu_layout_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_menu_layout_gen.h"
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

lv_obj_t * screen_menu_layout_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_menu_layout_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, title_24, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        app_header_create(lv_obj_0, "应用布局");

        lv_obj_t * layout_grid = lv_button_create(lv_obj_0);
        lv_obj_set_name(layout_grid, "layout_grid");
        lv_obj_set_x(layout_grid, 24);
        lv_obj_set_y(layout_grid, 104);
        lv_obj_set_width(layout_grid, 342);
        lv_obj_set_height(layout_grid, 100);
        lv_obj_set_style_bg_color(layout_grid, BG_SURFACE, 0);
        lv_obj_set_style_border_width(layout_grid, 0, 0);
        lv_obj_set_style_radius(layout_grid, 24, 0);
        lv_obj_set_style_pad_all(layout_grid, 0, 0);
        lv_obj_set_style_shadow_width(layout_grid, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(layout_grid);
        lv_obj_set_x(lv_label_0, 24);
        lv_obj_set_align(lv_label_0, LV_ALIGN_LEFT_MID);
        lv_label_set_text(lv_label_0, "多列布局");

        lv_obj_t * layout_grid_check = lv_obj_create(layout_grid);
        lv_obj_set_name(layout_grid_check, "layout_grid_check");
        lv_obj_set_x(layout_grid_check, -24);
        lv_obj_set_align(layout_grid_check, LV_ALIGN_RIGHT_MID);
        lv_obj_set_width(layout_grid_check, 42);
        lv_obj_set_height(layout_grid_check, 42);
        lv_obj_set_flag(layout_grid_check, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(layout_grid_check, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(layout_grid_check, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(layout_grid_check, ACCENT_BLUE, 0);
        lv_obj_set_style_bg_opa(layout_grid_check, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(layout_grid_check, 0, 0);
        lv_obj_set_style_radius(layout_grid_check, 21, 0);
        lv_obj_set_style_pad_all(layout_grid_check, 0, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(layout_grid_check);
        lv_obj_set_align(lv_obj_1, LV_ALIGN_CENTER);
        lv_obj_set_width(lv_obj_1, 18);
        lv_obj_set_height(lv_obj_1, 18);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_1, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(lv_obj_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_radius(lv_obj_1, 9, 0);
        lv_obj_set_style_pad_all(lv_obj_1, 0, 0);

        lv_obj_t * layout_list = lv_button_create(lv_obj_0);
        lv_obj_set_name(layout_list, "layout_list");
        lv_obj_set_x(layout_list, 24);
        lv_obj_set_y(layout_list, 216);
        lv_obj_set_width(layout_list, 342);
        lv_obj_set_height(layout_list, 100);
        lv_obj_set_style_bg_color(layout_list, BG_SURFACE, 0);
        lv_obj_set_style_border_width(layout_list, 0, 0);
        lv_obj_set_style_radius(layout_list, 24, 0);
        lv_obj_set_style_pad_all(layout_list, 0, 0);
        lv_obj_set_style_shadow_width(layout_list, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(layout_list);
        lv_obj_set_x(lv_label_1, 24);
        lv_obj_set_align(lv_label_1, LV_ALIGN_LEFT_MID);
        lv_label_set_text(lv_label_1, "列表布局");

        lv_obj_t * layout_list_check = lv_obj_create(layout_list);
        lv_obj_set_name(layout_list_check, "layout_list_check");
        lv_obj_set_x(layout_list_check, -24);
        lv_obj_set_align(layout_list_check, LV_ALIGN_RIGHT_MID);
        lv_obj_set_width(layout_list_check, 42);
        lv_obj_set_height(layout_list_check, 42);
        lv_obj_set_flag(layout_list_check, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(layout_list_check, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(layout_list_check, ACCENT_BLUE, 0);
        lv_obj_set_style_bg_opa(layout_list_check, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(layout_list_check, 0, 0);
        lv_obj_set_style_radius(layout_list_check, 21, 0);
        lv_obj_set_style_pad_all(layout_list_check, 0, 0);
        lv_obj_t * lv_obj_2 = lv_obj_create(layout_list_check);
        lv_obj_set_align(lv_obj_2, LV_ALIGN_CENTER);
        lv_obj_set_width(lv_obj_2, 18);
        lv_obj_set_height(lv_obj_2, 18);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_2, FG_PRIMARY, 0);
        lv_obj_set_style_bg_opa(lv_obj_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_2, 0, 0);
        lv_obj_set_style_radius(lv_obj_2, 9, 0);
        lv_obj_set_style_pad_all(lv_obj_2, 0, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

