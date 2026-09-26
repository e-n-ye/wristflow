/**
 * @file screen_component_editor_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_component_editor_gen.h"
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

lv_obj_t * screen_component_editor_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_component_editor_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        app_header_create(lv_obj_0, "编辑组件");

        lv_obj_t * edit_canvas = lv_obj_create(lv_obj_0);
        lv_obj_set_name(edit_canvas, "edit_canvas");
        lv_obj_set_x(edit_canvas, 63);
        lv_obj_set_y(edit_canvas, 88);
        lv_obj_set_width(edit_canvas, 264);
        lv_obj_set_height(edit_canvas, 266);
        lv_obj_set_flag(edit_canvas, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(edit_canvas, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(edit_canvas, 0, 0);
        lv_obj_set_style_pad_all(edit_canvas, 0, 0);
        lv_obj_set_style_radius(edit_canvas, 0, 0);
        lv_obj_set_style_layout(edit_canvas, LV_LAYOUT_GRID, 0);
        static const int32_t edit_canvas_style_grid_column_dsc_array_0[] = {126, 126, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_style_grid_column_dsc_array(edit_canvas, edit_canvas_style_grid_column_dsc_array_0, 0);
        static const int32_t edit_canvas_style_grid_row_dsc_array_0[] = {126, 126, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_style_grid_row_dsc_array(edit_canvas, edit_canvas_style_grid_row_dsc_array_0, 0);
        lv_obj_set_style_pad_column(edit_canvas, 12, 0);
        lv_obj_set_style_pad_row(edit_canvas, 14, 0);

        lv_obj_t * edit_left = lv_button_create(lv_obj_0);
        lv_obj_set_name(edit_left, "edit_left");
        lv_obj_set_x(edit_left, 8);
        lv_obj_set_y(edit_left, 198);
        lv_obj_set_width(edit_left, 48);
        lv_obj_set_height(edit_left, 48);
        lv_obj_set_style_bg_opa(edit_left, (255 * 0 / 100), 0);
        lv_obj_set_style_shadow_width(edit_left, 0, 0);
        lv_obj_set_style_pad_all(edit_left, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(edit_left);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "+");
        lv_obj_set_style_text_font(lv_label_0, value_36, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_PRIMARY, 0);

        lv_obj_t * edit_right = lv_button_create(lv_obj_0);
        lv_obj_set_name(edit_right, "edit_right");
        lv_obj_set_x(edit_right, 334);
        lv_obj_set_y(edit_right, 198);
        lv_obj_set_width(edit_right, 48);
        lv_obj_set_height(edit_right, 48);
        lv_obj_set_style_bg_opa(edit_right, (255 * 0 / 100), 0);
        lv_obj_set_style_shadow_width(edit_right, 0, 0);
        lv_obj_set_style_pad_all(edit_right, 0, 0);
        lv_obj_t * lv_label_1 = lv_label_create(edit_right);
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_1, "+");
        lv_obj_set_style_text_font(lv_label_1, value_36, 0);
        lv_obj_set_style_text_color(lv_label_1, FG_PRIMARY, 0);

        lv_obj_t * edit_action = lv_button_create(lv_obj_0);
        lv_obj_set_name(edit_action, "edit_action");
        lv_obj_set_x(edit_action, 159);
        lv_obj_set_y(edit_action, 366);
        lv_obj_set_width(edit_action, 72);
        lv_obj_set_height(edit_action, 48);
        lv_obj_set_style_bg_color(edit_action, BG_SURFACE, 0);
        lv_obj_set_style_radius(edit_action, 24, 0);
        lv_obj_set_style_shadow_width(edit_action, 0, 0);
        lv_obj_set_style_pad_all(edit_action, 0, 0);
        lv_obj_t * edit_action_icon = lv_label_create(edit_action);
        lv_obj_set_name(edit_action_icon, "edit_action_icon");
        lv_obj_set_align(edit_action_icon, LV_ALIGN_CENTER);
        lv_label_set_text(edit_action_icon, "");
        lv_obj_set_style_text_font(edit_action_icon, icons_20, 0);
        lv_obj_set_style_text_color(edit_action_icon, FG_PRIMARY, 0);

        lv_obj_t * edit_save = lv_label_create(lv_obj_0);
        lv_obj_set_name(edit_save, "edit_save");
        lv_obj_set_x(edit_save, 24);
        lv_obj_set_y(edit_save, 420);
        lv_obj_set_width(edit_save, 342);
        lv_label_set_text(edit_save, "");
        lv_obj_set_style_text_align(edit_save, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(edit_save, body_20, 0);
        lv_obj_set_style_text_color(edit_save, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

