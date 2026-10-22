/**
 * @file screen_endurance_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_endurance_gen.h"
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

lv_obj_t * screen_endurance_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t hidden_knob;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&hidden_knob);

        lv_style_set_bg_opa(&hidden_knob, (255 * 0 / 100));
        lv_style_set_pad_all(&hidden_knob, 0);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * endurance_page = lv_obj_create(NULL);
        lv_obj_set_name_static(endurance_page, "screen_endurance_#");
        lv_obj_set_name(endurance_page, "endurance_page");
        lv_obj_set_width(endurance_page, 390);
        lv_obj_set_height(endurance_page, 450);
        lv_obj_set_flag(endurance_page, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(endurance_page, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(endurance_page, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(endurance_page, 0, 0);
        lv_obj_set_style_pad_all(endurance_page, 0, 0);
        lv_obj_set_style_radius(endurance_page, 0, 0);
        lv_obj_set_style_text_color(endurance_page, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(endurance_page, notification_22, 0);

        lv_obj_t * endurance_date = lv_label_create(endurance_page);
        lv_obj_set_name(endurance_date, "endurance_date");
        lv_obj_set_x(endurance_date, 24);
        lv_obj_set_y(endurance_date, 102);
        lv_obj_set_width(endurance_date, 342);
        lv_label_set_text(endurance_date, "10/22 周四");
        lv_obj_set_style_text_align(endurance_date, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * endurance_time = lv_label_create(endurance_page);
        lv_obj_set_name(endurance_time, "endurance_time");
        lv_obj_set_x(endurance_time, 24);
        lv_obj_set_y(endurance_time, 139);
        lv_obj_set_width(endurance_time, 342);
        lv_label_set_text(endurance_time, "15:55");
        lv_obj_set_style_text_font(endurance_time, clock_80, 0);
        lv_obj_set_style_text_align(endurance_time, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * endurance_battery = lv_label_create(endurance_page);
        lv_obj_set_name(endurance_battery, "endurance_battery");
        lv_obj_set_x(endurance_battery, 24);
        lv_obj_set_y(endurance_battery, 245);
        lv_obj_set_width(endurance_battery, 342);
        lv_label_set_text(endurance_battery, "~83%");
        lv_obj_set_style_text_align(endurance_battery, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * endurance_steps = lv_label_create(endurance_page);
        lv_obj_set_name(endurance_steps, "endurance_steps");
        lv_obj_set_x(endurance_steps, 24);
        lv_obj_set_y(endurance_steps, 360);
        lv_obj_set_width(endurance_steps, 342);
        lv_label_set_text(endurance_steps, "步数 0");
        lv_obj_set_style_text_align(endurance_steps, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * endurance_status = lv_label_create(endurance_page);
        lv_obj_set_name(endurance_status, "endurance_status");
        lv_obj_set_x(endurance_status, 24);
        lv_obj_set_y(endurance_status, 296);
        lv_obj_set_width(endurance_status, 342);
        lv_label_set_text(endurance_status, "");
        lv_obj_set_style_text_align(endurance_status, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(endurance_status, FG_SECONDARY, 0);

        lv_obj_t * endurance_save = lv_label_create(endurance_page);
        lv_obj_set_name(endurance_save, "endurance_save");
        lv_obj_set_x(endurance_save, 24);
        lv_obj_set_y(endurance_save, 401);
        lv_obj_set_width(endurance_save, 342);
        lv_label_set_text(endurance_save, "");
        lv_obj_set_style_text_align(endurance_save, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(endurance_save, FG_SECONDARY, 0);

        lv_obj_t * endurance_exit = lv_obj_create(endurance_page);
        lv_obj_set_name(endurance_exit, "endurance_exit");
        lv_obj_set_width(endurance_exit, 390);
        lv_obj_set_height(endurance_exit, 450);
        lv_obj_set_flag(endurance_exit, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(endurance_exit, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(endurance_exit, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(endurance_exit, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(endurance_exit, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(endurance_exit, 0, 0);
        lv_obj_set_style_pad_all(endurance_exit, 0, 0);
        lv_obj_set_style_radius(endurance_exit, 0, 0);
        lv_obj_t * endurance_ring = lv_arc_create(endurance_exit);
        lv_obj_set_name(endurance_ring, "endurance_ring");
        lv_obj_set_x(endurance_ring, 121);
        lv_obj_set_y(endurance_ring, 93);
        lv_obj_set_width(endurance_ring, 148);
        lv_obj_set_height(endurance_ring, 148);
        lv_obj_set_flag(endurance_ring, LV_OBJ_FLAG_CLICKABLE, false);
        lv_arc_set_value(endurance_ring, 0);
        lv_arc_set_bg_start_angle(endurance_ring, 0);
        lv_arc_set_bg_end_angle(endurance_ring, 360);
        lv_arc_set_rotation(endurance_ring, 270);
        lv_obj_set_style_arc_color(endurance_ring, lv_color_hex(0x303030), 0);
        lv_obj_set_style_arc_width(endurance_ring, 10, 0);
        lv_obj_add_style(endurance_ring, &hidden_knob, LV_PART_KNOB);

        lv_obj_t * lv_image_0 = lv_image_create(endurance_exit);
        lv_obj_set_x(lv_image_0, 163);
        lv_obj_set_y(lv_image_0, 135);
        lv_image_set_src(lv_image_0, control_battery);

        lv_obj_t * lv_label_0 = lv_label_create(endurance_exit);
        lv_obj_set_x(lv_label_0, 24);
        lv_obj_set_y(lv_label_0, 278);
        lv_obj_set_width(lv_label_0, 342);
        lv_label_set_text(lv_label_0, "按住按键3秒退出长续航模式");
        lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(lv_label_0, notification_22, 0);
        lv_obj_set_style_text_color(lv_label_0, FG_PRIMARY, 0);

        lv_obj_t * lv_label_1 = lv_label_create(endurance_exit);
        lv_obj_set_x(lv_label_1, 24);
        lv_obj_set_y(lv_label_1, 344);
        lv_obj_set_width(lv_label_1, 342);
        lv_label_set_text(lv_label_1, "松开取消");
        lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(lv_label_1, notification_22, 0);
        lv_obj_set_style_text_color(lv_label_1, FG_SECONDARY, 0);

        the_root = endurance_page;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

