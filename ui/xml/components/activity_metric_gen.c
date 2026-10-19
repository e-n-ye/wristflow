/**
 * @file activity_metric_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "activity_metric_gen.h"
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

lv_obj_t * activity_metric_create(lv_obj_t * parent, const char * title, const char * icon, lv_color_t accent, const char * unit)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "activity_metric_#");
        lv_obj_set_width(lv_obj_0, 342);
        lv_obj_set_height(lv_obj_0, 410);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 24, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 20);
        lv_obj_set_y(lv_obj_1, 18);
        lv_obj_set_width(lv_obj_1, 44);
        lv_obj_set_height(lv_obj_1, 44);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_1, accent, 0);
        lv_obj_set_style_bg_opa(lv_obj_1, (255 * 20 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_radius(lv_obj_1, 22, 0);
        lv_obj_set_style_pad_all(lv_obj_1, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_1);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, icon);
        lv_obj_set_style_text_font(lv_label_0, icons_20, 0);
        lv_obj_set_style_text_color(lv_label_0, accent, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_1, 78);
        lv_obj_set_y(lv_label_1, 22);
        lv_obj_set_width(lv_label_1, 250);
        lv_label_set_text(lv_label_1, title);
        lv_obj_set_style_text_font(lv_label_1, title_24, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 20);
        lv_obj_set_y(lv_obj_2, 78);
        lv_obj_set_width(lv_obj_2, 302);
        lv_obj_set_height(lv_obj_2, 1);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_2, lv_color_hex(0x363b43), 0);
        lv_obj_set_style_bg_opa(lv_obj_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_2, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_2, 0, 0);

        lv_obj_t * metric_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(metric_value, "metric_value");
        lv_obj_set_x(metric_value, 20);
        lv_obj_set_y(metric_value, 100);
        lv_obj_set_width(metric_value, 216);
        lv_label_set_text(metric_value, "--");
        lv_obj_set_style_text_font(metric_value, metric_56, 0);

        lv_obj_t * metric_unit = lv_label_create(lv_obj_0);
        lv_obj_set_name(metric_unit, "metric_unit");
        lv_obj_set_x(metric_unit, 244);
        lv_obj_set_y(metric_unit, 126);
        lv_obj_set_width(metric_unit, 80);
        lv_label_set_text(metric_unit, unit);
        lv_obj_set_style_text_color(metric_unit, FG_SECONDARY, 0);

        lv_obj_t * metric_goal = lv_label_create(lv_obj_0);
        lv_obj_set_name(metric_goal, "metric_goal");
        lv_obj_set_x(metric_goal, 20);
        lv_obj_set_y(metric_goal, 178);
        lv_obj_set_width(metric_goal, 302);
        lv_label_set_text(metric_goal, "目标 --");
        lv_obj_set_style_text_color(metric_goal, FG_SECONDARY, 0);

        lv_obj_t * chart_max = lv_label_create(lv_obj_0);
        lv_obj_set_name(chart_max, "chart_max");
        lv_obj_set_x(chart_max, 162);
        lv_obj_set_y(chart_max, 222);
        lv_obj_set_width(chart_max, 160);
        lv_label_set_text(chart_max, "--");
        lv_obj_set_style_text_font(chart_max, body_20, 0);
        lv_obj_set_style_text_align(chart_max, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(chart_max, FG_SECONDARY, 0);

        lv_obj_t * metric_plot = activity_plot_create(lv_obj_0);
        lv_obj_set_name(metric_plot, "metric_plot");
        lv_obj_set_x(metric_plot, 20);
        lv_obj_set_y(metric_plot, 254);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_2, 20);
        lv_obj_set_y(lv_label_2, 372);
        lv_label_set_text(lv_label_2, "0");
        lv_obj_set_style_text_font(lv_label_2, body_20, 0);
        lv_obj_set_style_text_color(lv_label_2, FG_SECONDARY, 0);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_3, 152);
        lv_obj_set_y(lv_label_3, 372);
        lv_label_set_text(lv_label_3, "12");
        lv_obj_set_style_text_font(lv_label_3, body_20, 0);
        lv_obj_set_style_text_color(lv_label_3, FG_SECONDARY, 0);

        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_0);
        lv_obj_set_x(lv_label_4, 296);
        lv_obj_set_y(lv_label_4, 372);
        lv_label_set_text(lv_label_4, "24");
        lv_obj_set_style_text_font(lv_label_4, body_20, 0);
        lv_obj_set_style_text_color(lv_label_4, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

