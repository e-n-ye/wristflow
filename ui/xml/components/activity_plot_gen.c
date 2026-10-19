/**
 * @file activity_plot_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "activity_plot_gen.h"
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

lv_obj_t * activity_plot_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "activity_plot_#");
        lv_obj_set_width(lv_obj_0, 302);
        lv_obj_set_height(lv_obj_0, 110);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x121519), 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 6, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);

        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 0);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_set_width(lv_obj_1, 1);
        lv_obj_set_height(lv_obj_1, 110);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_1, lv_color_hex(0x363b43), 0);
        lv_obj_set_style_bg_opa(lv_obj_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_1, 0, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 150);
        lv_obj_set_y(lv_obj_2, 0);
        lv_obj_set_width(lv_obj_2, 1);
        lv_obj_set_height(lv_obj_2, 110);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_2, lv_color_hex(0x363b43), 0);
        lv_obj_set_style_bg_opa(lv_obj_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_2, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_2, 0, 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_3, 301);
        lv_obj_set_y(lv_obj_3, 0);
        lv_obj_set_width(lv_obj_3, 1);
        lv_obj_set_height(lv_obj_3, 110);
        lv_obj_set_flag(lv_obj_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(lv_obj_3, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_color(lv_obj_3, lv_color_hex(0x363b43), 0);
        lv_obj_set_style_bg_opa(lv_obj_3, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_3, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_3, 0, 0);

        lv_obj_t * hour_0 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_0, "hour_0");
        lv_obj_set_x(hour_0, 3);
        lv_obj_set_y(hour_0, 108);
        lv_obj_set_width(hour_0, 7);
        lv_obj_set_height(hour_0, 2);
        lv_obj_set_flag(hour_0, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_0, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_0, 0, 0);
        lv_obj_set_style_radius(hour_0, 2, 0);
        lv_obj_set_style_pad_all(hour_0, 0, 0);

        lv_obj_t * hour_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_1, "hour_1");
        lv_obj_set_x(hour_1, 15);
        lv_obj_set_y(hour_1, 108);
        lv_obj_set_width(hour_1, 7);
        lv_obj_set_height(hour_1, 2);
        lv_obj_set_flag(hour_1, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_1, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_1, 0, 0);
        lv_obj_set_style_radius(hour_1, 2, 0);
        lv_obj_set_style_pad_all(hour_1, 0, 0);

        lv_obj_t * hour_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_2, "hour_2");
        lv_obj_set_x(hour_2, 27);
        lv_obj_set_y(hour_2, 108);
        lv_obj_set_width(hour_2, 7);
        lv_obj_set_height(hour_2, 2);
        lv_obj_set_flag(hour_2, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_2, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_2, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_2, 0, 0);
        lv_obj_set_style_radius(hour_2, 2, 0);
        lv_obj_set_style_pad_all(hour_2, 0, 0);

        lv_obj_t * hour_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_3, "hour_3");
        lv_obj_set_x(hour_3, 39);
        lv_obj_set_y(hour_3, 108);
        lv_obj_set_width(hour_3, 7);
        lv_obj_set_height(hour_3, 2);
        lv_obj_set_flag(hour_3, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_3, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_3, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_3, 0, 0);
        lv_obj_set_style_radius(hour_3, 2, 0);
        lv_obj_set_style_pad_all(hour_3, 0, 0);

        lv_obj_t * hour_4 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_4, "hour_4");
        lv_obj_set_x(hour_4, 51);
        lv_obj_set_y(hour_4, 108);
        lv_obj_set_width(hour_4, 7);
        lv_obj_set_height(hour_4, 2);
        lv_obj_set_flag(hour_4, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_4, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_4, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_4, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_4, 0, 0);
        lv_obj_set_style_radius(hour_4, 2, 0);
        lv_obj_set_style_pad_all(hour_4, 0, 0);

        lv_obj_t * hour_5 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_5, "hour_5");
        lv_obj_set_x(hour_5, 63);
        lv_obj_set_y(hour_5, 108);
        lv_obj_set_width(hour_5, 7);
        lv_obj_set_height(hour_5, 2);
        lv_obj_set_flag(hour_5, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_5, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_5, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_5, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_5, 0, 0);
        lv_obj_set_style_radius(hour_5, 2, 0);
        lv_obj_set_style_pad_all(hour_5, 0, 0);

        lv_obj_t * hour_6 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_6, "hour_6");
        lv_obj_set_x(hour_6, 75);
        lv_obj_set_y(hour_6, 108);
        lv_obj_set_width(hour_6, 7);
        lv_obj_set_height(hour_6, 2);
        lv_obj_set_flag(hour_6, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_6, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_6, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_6, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_6, 0, 0);
        lv_obj_set_style_radius(hour_6, 2, 0);
        lv_obj_set_style_pad_all(hour_6, 0, 0);

        lv_obj_t * hour_7 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_7, "hour_7");
        lv_obj_set_x(hour_7, 87);
        lv_obj_set_y(hour_7, 108);
        lv_obj_set_width(hour_7, 7);
        lv_obj_set_height(hour_7, 2);
        lv_obj_set_flag(hour_7, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_7, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_7, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_7, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_7, 0, 0);
        lv_obj_set_style_radius(hour_7, 2, 0);
        lv_obj_set_style_pad_all(hour_7, 0, 0);

        lv_obj_t * hour_8 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_8, "hour_8");
        lv_obj_set_x(hour_8, 99);
        lv_obj_set_y(hour_8, 108);
        lv_obj_set_width(hour_8, 7);
        lv_obj_set_height(hour_8, 2);
        lv_obj_set_flag(hour_8, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_8, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_8, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_8, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_8, 0, 0);
        lv_obj_set_style_radius(hour_8, 2, 0);
        lv_obj_set_style_pad_all(hour_8, 0, 0);

        lv_obj_t * hour_9 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_9, "hour_9");
        lv_obj_set_x(hour_9, 111);
        lv_obj_set_y(hour_9, 108);
        lv_obj_set_width(hour_9, 7);
        lv_obj_set_height(hour_9, 2);
        lv_obj_set_flag(hour_9, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_9, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_9, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_9, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_9, 0, 0);
        lv_obj_set_style_radius(hour_9, 2, 0);
        lv_obj_set_style_pad_all(hour_9, 0, 0);

        lv_obj_t * hour_10 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_10, "hour_10");
        lv_obj_set_x(hour_10, 123);
        lv_obj_set_y(hour_10, 108);
        lv_obj_set_width(hour_10, 7);
        lv_obj_set_height(hour_10, 2);
        lv_obj_set_flag(hour_10, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_10, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_10, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_10, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_10, 0, 0);
        lv_obj_set_style_radius(hour_10, 2, 0);
        lv_obj_set_style_pad_all(hour_10, 0, 0);

        lv_obj_t * hour_11 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_11, "hour_11");
        lv_obj_set_x(hour_11, 135);
        lv_obj_set_y(hour_11, 108);
        lv_obj_set_width(hour_11, 7);
        lv_obj_set_height(hour_11, 2);
        lv_obj_set_flag(hour_11, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_11, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_11, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_11, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_11, 0, 0);
        lv_obj_set_style_radius(hour_11, 2, 0);
        lv_obj_set_style_pad_all(hour_11, 0, 0);

        lv_obj_t * hour_12 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_12, "hour_12");
        lv_obj_set_x(hour_12, 147);
        lv_obj_set_y(hour_12, 108);
        lv_obj_set_width(hour_12, 7);
        lv_obj_set_height(hour_12, 2);
        lv_obj_set_flag(hour_12, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_12, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_12, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_12, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_12, 0, 0);
        lv_obj_set_style_radius(hour_12, 2, 0);
        lv_obj_set_style_pad_all(hour_12, 0, 0);

        lv_obj_t * hour_13 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_13, "hour_13");
        lv_obj_set_x(hour_13, 159);
        lv_obj_set_y(hour_13, 108);
        lv_obj_set_width(hour_13, 7);
        lv_obj_set_height(hour_13, 2);
        lv_obj_set_flag(hour_13, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_13, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_13, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_13, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_13, 0, 0);
        lv_obj_set_style_radius(hour_13, 2, 0);
        lv_obj_set_style_pad_all(hour_13, 0, 0);

        lv_obj_t * hour_14 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_14, "hour_14");
        lv_obj_set_x(hour_14, 171);
        lv_obj_set_y(hour_14, 108);
        lv_obj_set_width(hour_14, 7);
        lv_obj_set_height(hour_14, 2);
        lv_obj_set_flag(hour_14, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_14, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_14, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_14, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_14, 0, 0);
        lv_obj_set_style_radius(hour_14, 2, 0);
        lv_obj_set_style_pad_all(hour_14, 0, 0);

        lv_obj_t * hour_15 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_15, "hour_15");
        lv_obj_set_x(hour_15, 183);
        lv_obj_set_y(hour_15, 108);
        lv_obj_set_width(hour_15, 7);
        lv_obj_set_height(hour_15, 2);
        lv_obj_set_flag(hour_15, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_15, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_15, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_15, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_15, 0, 0);
        lv_obj_set_style_radius(hour_15, 2, 0);
        lv_obj_set_style_pad_all(hour_15, 0, 0);

        lv_obj_t * hour_16 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_16, "hour_16");
        lv_obj_set_x(hour_16, 195);
        lv_obj_set_y(hour_16, 108);
        lv_obj_set_width(hour_16, 7);
        lv_obj_set_height(hour_16, 2);
        lv_obj_set_flag(hour_16, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_16, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_16, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_16, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_16, 0, 0);
        lv_obj_set_style_radius(hour_16, 2, 0);
        lv_obj_set_style_pad_all(hour_16, 0, 0);

        lv_obj_t * hour_17 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_17, "hour_17");
        lv_obj_set_x(hour_17, 207);
        lv_obj_set_y(hour_17, 108);
        lv_obj_set_width(hour_17, 7);
        lv_obj_set_height(hour_17, 2);
        lv_obj_set_flag(hour_17, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_17, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_17, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_17, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_17, 0, 0);
        lv_obj_set_style_radius(hour_17, 2, 0);
        lv_obj_set_style_pad_all(hour_17, 0, 0);

        lv_obj_t * hour_18 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_18, "hour_18");
        lv_obj_set_x(hour_18, 219);
        lv_obj_set_y(hour_18, 108);
        lv_obj_set_width(hour_18, 7);
        lv_obj_set_height(hour_18, 2);
        lv_obj_set_flag(hour_18, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_18, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_18, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_18, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_18, 0, 0);
        lv_obj_set_style_radius(hour_18, 2, 0);
        lv_obj_set_style_pad_all(hour_18, 0, 0);

        lv_obj_t * hour_19 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_19, "hour_19");
        lv_obj_set_x(hour_19, 231);
        lv_obj_set_y(hour_19, 108);
        lv_obj_set_width(hour_19, 7);
        lv_obj_set_height(hour_19, 2);
        lv_obj_set_flag(hour_19, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_19, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_19, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_19, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_19, 0, 0);
        lv_obj_set_style_radius(hour_19, 2, 0);
        lv_obj_set_style_pad_all(hour_19, 0, 0);

        lv_obj_t * hour_20 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_20, "hour_20");
        lv_obj_set_x(hour_20, 243);
        lv_obj_set_y(hour_20, 108);
        lv_obj_set_width(hour_20, 7);
        lv_obj_set_height(hour_20, 2);
        lv_obj_set_flag(hour_20, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_20, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_20, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_20, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_20, 0, 0);
        lv_obj_set_style_radius(hour_20, 2, 0);
        lv_obj_set_style_pad_all(hour_20, 0, 0);

        lv_obj_t * hour_21 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_21, "hour_21");
        lv_obj_set_x(hour_21, 255);
        lv_obj_set_y(hour_21, 108);
        lv_obj_set_width(hour_21, 7);
        lv_obj_set_height(hour_21, 2);
        lv_obj_set_flag(hour_21, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_21, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_21, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_21, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_21, 0, 0);
        lv_obj_set_style_radius(hour_21, 2, 0);
        lv_obj_set_style_pad_all(hour_21, 0, 0);

        lv_obj_t * hour_22 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_22, "hour_22");
        lv_obj_set_x(hour_22, 267);
        lv_obj_set_y(hour_22, 108);
        lv_obj_set_width(hour_22, 7);
        lv_obj_set_height(hour_22, 2);
        lv_obj_set_flag(hour_22, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_22, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_22, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_22, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_22, 0, 0);
        lv_obj_set_style_radius(hour_22, 2, 0);
        lv_obj_set_style_pad_all(hour_22, 0, 0);

        lv_obj_t * hour_23 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(hour_23, "hour_23");
        lv_obj_set_x(hour_23, 279);
        lv_obj_set_y(hour_23, 108);
        lv_obj_set_width(hour_23, 7);
        lv_obj_set_height(hour_23, 2);
        lv_obj_set_flag(hour_23, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(hour_23, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(hour_23, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(hour_23, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(hour_23, 0, 0);
        lv_obj_set_style_radius(hour_23, 2, 0);
        lv_obj_set_style_pad_all(hour_23, 0, 0);

        lv_obj_t * chart_empty = lv_label_create(lv_obj_0);
        lv_obj_set_name(chart_empty, "chart_empty");
        lv_obj_set_align(chart_empty, LV_ALIGN_CENTER);
        lv_label_set_text(chart_empty, "暂无数据");
        lv_obj_set_style_text_font(chart_empty, notification_22, 0);
        lv_obj_set_style_text_color(chart_empty, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

