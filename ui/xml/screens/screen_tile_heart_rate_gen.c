/**
 * @file screen_tile_heart_rate_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_tile_heart_rate_gen.h"
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

lv_obj_t * screen_tile_heart_rate_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_tile_heart_rate_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * heart_tile = card_container_create(lv_obj_0, "心率", "--", "", BG_SURFACE, FG_PRIMARY, FG_SECONDARY, lv_color_hex(0xff5678), 165, 174);
        lv_obj_set_name(heart_tile, "heart_tile");
        lv_obj_set_x(heart_tile, 24);
        lv_obj_set_y(heart_tile, 28);

        lv_obj_t * activity_tile = card_container_create(lv_obj_0, "今日步数", "6,280", "", lv_color_hex(0xff648a), lv_color_hex(0x24111b), lv_color_hex(0x4c2435), lv_color_hex(0x24111b), 165, 174);
        lv_obj_set_name(activity_tile, "activity_tile");
        lv_obj_set_x(activity_tile, 201);
        lv_obj_set_y(activity_tile, 28);

        lv_obj_t * battery_tile = card_container_create(lv_obj_0, "电池", "53%", "", lv_color_hex(0x67e1f7), lv_color_hex(0x09272c), lv_color_hex(0x234b54), lv_color_hex(0x09272c), 165, 174);
        lv_obj_set_name(battery_tile, "battery_tile");
        lv_obj_set_x(battery_tile, 24);
        lv_obj_set_y(battery_tile, 216);

        lv_obj_t * connection_tile = card_container_create(lv_obj_0, "蓝牙", "已连接", "", BG_SURFACE, FG_PRIMARY, FG_SECONDARY, lv_color_hex(0x67e1f7), 165, 174);
        lv_obj_set_name(connection_tile, "connection_tile");
        lv_obj_set_x(connection_tile, 201);
        lv_obj_set_y(connection_tile, 216);

        lv_obj_t * page_dots_0 = page_dots_create(lv_obj_0, 0);
        lv_obj_set_x(page_dots_0, 174);
        lv_obj_set_y(page_dots_0, PAGER_BOTTOM_Y);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

