/**
 * @file screen_activity_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_activity_gen.h"
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

lv_obj_t * screen_activity_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_activity_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * activity_scroll = lv_obj_create(lv_obj_0);
        lv_obj_set_name(activity_scroll, "activity_scroll");
        lv_obj_set_width(activity_scroll, 390);
        lv_obj_set_height(activity_scroll, 450);
        lv_obj_set_scrollbar_mode(activity_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(activity_scroll, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(activity_scroll, 0, 0);
        lv_obj_set_style_pad_all(activity_scroll, 0, 0);
        lv_obj_set_style_pad_bottom(activity_scroll, 24, 0);
        lv_obj_set_style_radius(activity_scroll, 0, 0);
        lv_obj_set_style_text_color(activity_scroll, FG_PRIMARY, 0);
        app_header_create(activity_scroll, "活力指标");

        lv_obj_t * activity_time = lv_label_create(activity_scroll);
        lv_obj_set_name(activity_time, "activity_time");
        lv_obj_set_x(activity_time, 284);
        lv_obj_set_y(activity_time, 30);
        lv_obj_set_width(activity_time, 82);
        lv_label_set_text(activity_time, "--:--");
        lv_obj_set_style_text_font(activity_time, body_20, 0);
        lv_obj_set_style_text_align(activity_time, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * activity_overview = lv_obj_create(activity_scroll);
        lv_obj_set_name(activity_overview, "activity_overview");
        lv_obj_set_x(activity_overview, 24);
        lv_obj_set_y(activity_overview, 82);
        lv_obj_set_width(activity_overview, 342);
        lv_obj_set_height(activity_overview, 344);
        lv_obj_set_flag(activity_overview, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(activity_overview, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(activity_overview, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(activity_overview, 0, 0);
        lv_obj_set_style_pad_all(activity_overview, 0, 0);
        lv_obj_set_style_text_color(activity_overview, FG_PRIMARY, 0);
        lv_obj_t * overview_rings = activity_rings_create(activity_overview);
        lv_obj_set_name(overview_rings, "overview_rings");

        lv_obj_t * lv_label_0 = lv_label_create(activity_overview);
        lv_obj_set_x(lv_label_0, 20);
        lv_obj_set_y(lv_label_0, 158);
        lv_label_set_text(lv_label_0, "");
        lv_obj_set_style_text_font(lv_label_0, icons_44, 0);
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0xf97316), 0);

        lv_obj_t * overview_calories = lv_label_create(activity_overview);
        lv_obj_set_name(overview_calories, "overview_calories");
        lv_obj_set_x(overview_calories, 80);
        lv_obj_set_y(overview_calories, 154);
        lv_obj_set_width(overview_calories, 136);
        lv_label_set_text(overview_calories, "--");
        lv_obj_set_style_text_font(overview_calories, value_36, 0);

        lv_obj_t * overview_goal_0 = lv_label_create(activity_overview);
        lv_obj_set_name(overview_goal_0, "overview_goal_0");
        lv_obj_set_x(overview_goal_0, 222);
        lv_obj_set_y(overview_goal_0, 170);
        lv_obj_set_width(overview_goal_0, 120);
        lv_label_set_long_mode(overview_goal_0, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(overview_goal_0, "/ --");
        lv_obj_set_style_text_font(overview_goal_0, body_20, 0);
        lv_obj_set_style_text_color(overview_goal_0, FG_SECONDARY, 0);

        lv_obj_t * lv_label_1 = lv_label_create(activity_overview);
        lv_obj_set_x(lv_label_1, 20);
        lv_obj_set_y(lv_label_1, 222);
        lv_label_set_text(lv_label_1, "");
        lv_obj_set_style_text_font(lv_label_1, icons_44, 0);
        lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0xe2db48), 0);

        lv_obj_t * overview_steps = lv_label_create(activity_overview);
        lv_obj_set_name(overview_steps, "overview_steps");
        lv_obj_set_x(overview_steps, 80);
        lv_obj_set_y(overview_steps, 218);
        lv_obj_set_width(overview_steps, 136);
        lv_label_set_text(overview_steps, "--");
        lv_obj_set_style_text_font(overview_steps, value_36, 0);

        lv_obj_t * overview_goal_1 = lv_label_create(activity_overview);
        lv_obj_set_name(overview_goal_1, "overview_goal_1");
        lv_obj_set_x(overview_goal_1, 222);
        lv_obj_set_y(overview_goal_1, 234);
        lv_obj_set_width(overview_goal_1, 120);
        lv_label_set_long_mode(overview_goal_1, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(overview_goal_1, "/ --");
        lv_obj_set_style_text_font(overview_goal_1, body_20, 0);
        lv_obj_set_style_text_color(overview_goal_1, FG_SECONDARY, 0);

        lv_obj_t * lv_label_2 = lv_label_create(activity_overview);
        lv_obj_set_x(lv_label_2, 20);
        lv_obj_set_y(lv_label_2, 286);
        lv_label_set_text(lv_label_2, "");
        lv_obj_set_style_text_font(lv_label_2, icons_44, 0);
        lv_obj_set_style_text_color(lv_label_2, lv_color_hex(0x38bdf8), 0);

        lv_obj_t * overview_minutes = lv_label_create(activity_overview);
        lv_obj_set_name(overview_minutes, "overview_minutes");
        lv_obj_set_x(overview_minutes, 80);
        lv_obj_set_y(overview_minutes, 282);
        lv_obj_set_width(overview_minutes, 136);
        lv_label_set_text(overview_minutes, "--");
        lv_obj_set_style_text_font(overview_minutes, value_36, 0);

        lv_obj_t * overview_goal_2 = lv_label_create(activity_overview);
        lv_obj_set_name(overview_goal_2, "overview_goal_2");
        lv_obj_set_x(overview_goal_2, 222);
        lv_obj_set_y(overview_goal_2, 298);
        lv_obj_set_width(overview_goal_2, 120);
        lv_label_set_long_mode(overview_goal_2, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(overview_goal_2, "/ --");
        lv_obj_set_style_text_font(overview_goal_2, body_20, 0);
        lv_obj_set_style_text_color(overview_goal_2, FG_SECONDARY, 0);

        lv_obj_t * activity_calories = activity_metric_create(activity_scroll, "卡路里", "", lv_color_hex(0xf97316), "千卡");
        lv_obj_set_name(activity_calories, "activity_calories");
        lv_obj_set_x(activity_calories, 24);
        lv_obj_set_y(activity_calories, 450);

        lv_obj_t * activity_steps = activity_metric_create(activity_scroll, "步数", "", lv_color_hex(0xe2db48), "步");
        lv_obj_set_name(activity_steps, "activity_steps");
        lv_obj_set_x(activity_steps, 24);
        lv_obj_set_y(activity_steps, 876);

        lv_obj_t * activity_minutes = activity_metric_create(activity_scroll, "中高强度活动", "", lv_color_hex(0x38bdf8), "分钟");
        lv_obj_set_name(activity_minutes, "activity_minutes");
        lv_obj_set_x(activity_minutes, 24);
        lv_obj_set_y(activity_minutes, 1302);

        lv_obj_t * activity_standing = activity_metric_create(activity_scroll, "站立", "", lv_color_hex(0x10d6b0), "小时");
        lv_obj_set_name(activity_standing, "activity_standing");
        lv_obj_set_x(activity_standing, 24);
        lv_obj_set_y(activity_standing, 1728);

        lv_obj_t * activity_info = lv_button_create(activity_scroll);
        lv_obj_set_name(activity_info, "activity_info");
        lv_obj_set_x(activity_info, 24);
        lv_obj_set_y(activity_info, 2154);
        lv_obj_set_width(activity_info, 342);
        lv_obj_set_height(activity_info, 76);
        lv_obj_set_style_bg_color(activity_info, BG_SURFACE, 0);
        lv_obj_set_style_bg_opa(activity_info, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(activity_info, 24, 0);
        lv_obj_set_style_border_width(activity_info, 0, 0);
        lv_obj_set_style_shadow_width(activity_info, 0, 0);
        lv_obj_set_style_pad_all(activity_info, 0, 0);
        lv_obj_t * lv_label_3 = lv_label_create(activity_info);
        lv_obj_set_x(lv_label_3, 24);
        lv_obj_set_align(lv_label_3, LV_ALIGN_LEFT_MID);
        lv_label_set_text(lv_label_3, "");
        lv_obj_set_style_text_font(lv_label_3, icons_20, 0);
        lv_obj_set_style_text_color(lv_label_3, ACCENT_BLUE, 0);

        lv_obj_t * lv_label_4 = lv_label_create(activity_info);
        lv_obj_set_x(lv_label_4, 66);
        lv_obj_set_align(lv_label_4, LV_ALIGN_LEFT_MID);
        lv_label_set_text(lv_label_4, "数据说明");
        lv_obj_set_style_text_font(lv_label_4, title_24, 0);

        lv_obj_t * lv_label_5 = lv_label_create(activity_info);
        lv_obj_set_x(lv_label_5, 298);
        lv_obj_set_align(lv_label_5, LV_ALIGN_LEFT_MID);
        lv_label_set_text(lv_label_5, "");
        lv_obj_set_style_text_font(lv_label_5, icons_20, 0);
        lv_obj_set_style_text_color(lv_label_5, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

