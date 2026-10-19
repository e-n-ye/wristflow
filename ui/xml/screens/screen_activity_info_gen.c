/**
 * @file screen_activity_info_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_activity_info_gen.h"
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

lv_obj_t * screen_activity_info_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_activity_info_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, notification_22, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        app_header_create(lv_obj_0, "数据说明");

        lv_obj_t * activity_time = lv_label_create(lv_obj_0);
        lv_obj_set_name(activity_time, "activity_time");
        lv_obj_set_x(activity_time, 284);
        lv_obj_set_y(activity_time, 30);
        lv_obj_set_width(activity_time, 82);
        lv_label_set_text(activity_time, "--:--");
        lv_obj_set_style_text_font(activity_time, body_20, 0);
        lv_obj_set_style_text_align(activity_time, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * activity_info_scroll = lv_obj_create(lv_obj_0);
        lv_obj_set_name(activity_info_scroll, "activity_info_scroll");
        lv_obj_set_x(activity_info_scroll, 24);
        lv_obj_set_y(activity_info_scroll, 82);
        lv_obj_set_width(activity_info_scroll, 342);
        lv_obj_set_height(activity_info_scroll, 348);
        lv_obj_set_scrollbar_mode(activity_info_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_layout(activity_info_scroll, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(activity_info_scroll, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_style_bg_opa(activity_info_scroll, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(activity_info_scroll, 0, 0);
        lv_obj_set_style_radius(activity_info_scroll, 0, 0);
        lv_obj_set_style_pad_all(activity_info_scroll, 0, 0);
        lv_obj_set_style_pad_row(activity_info_scroll, 16, 0);
        lv_obj_set_style_pad_bottom(activity_info_scroll, 24, 0);
        lv_obj_set_style_text_color(activity_info_scroll, FG_PRIMARY, 0);
        lv_obj_t * lv_label_0 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_0, 342);
        lv_label_set_text(lv_label_0, "步数");
        lv_obj_set_style_text_font(lv_label_0, title_24, 0);
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0xe2db48), 0);

        lv_obj_t * lv_label_1 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_1, 342);
        lv_label_set_text(lv_label_1, "每只脚落地一次算一步，左右脚各落地一次共两步。这里展示当日累计及各时段的步数。");

        lv_obj_t * lv_label_2 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_2, 342);
        lv_label_set_text(lv_label_2, "卡路里");
        lv_obj_set_style_text_font(lv_label_2, title_24, 0);
        lv_obj_set_style_text_color(lv_label_2, lv_color_hex(0xf97316), 0);

        lv_obj_t * lv_label_3 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_3, 342);
        lv_label_set_text(lv_label_3, "活动消耗的能量，以千卡显示。它属于估算值，不等于全天总消耗。");

        lv_obj_t * lv_label_4 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_4, 342);
        lv_label_set_text(lv_label_4, "中高强度活动");
        lv_obj_set_style_text_font(lv_label_4, title_24, 0);
        lv_obj_set_style_text_color(lv_label_4, lv_color_hex(0x38bdf8), 0);

        lv_obj_t * lv_label_5 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_5, 342);
        lv_label_set_text(lv_label_5, "达到活动强度条件的累计时长，以分钟显示。步数不直接等于活动时长。");

        lv_obj_t * lv_label_6 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_6, 342);
        lv_label_set_text(lv_label_6, "站立");
        lv_obj_set_style_text_font(lv_label_6, title_24, 0);
        lv_obj_set_style_text_color(lv_label_6, lv_color_hex(0x10d6b0), 0);

        lv_obj_t * lv_label_7 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_7, 342);
        lv_label_set_text(lv_label_7, "记录当日有起身活动的小时数，不代表持续站立的时长。");

        lv_obj_t * lv_label_8 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_8, 342);
        lv_label_set_text(lv_label_8, "数据与目标");
        lv_obj_set_style_text_font(lv_label_8, title_24, 0);

        lv_obj_t * lv_label_9 = lv_label_create(activity_info_scroll);
        lv_obj_set_width(lv_label_9, 342);
        lv_label_set_text(lv_label_9, "未测得或尚无来源的数据以 -- 显示，真实的零值显示为 0。图表横轴是一天的 0 至 24 时，只显示已有记录的时段。目标用于展示完成进度。");

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

