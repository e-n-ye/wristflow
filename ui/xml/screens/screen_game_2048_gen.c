/**
 * @file screen_game_2048_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_game_2048_gen.h"
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

lv_obj_t * screen_game_2048_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * game_page = lv_obj_create(NULL);
        lv_obj_set_name_static(game_page, "screen_game_2048_#");
        lv_obj_set_name(game_page, "game_page");
        lv_obj_set_width(game_page, 390);
        lv_obj_set_height(game_page, 450);
        lv_obj_set_flag(game_page, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(game_page, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(game_page, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(game_page, 0, 0);
        lv_obj_set_style_pad_all(game_page, 0, 0);
        lv_obj_set_style_radius(game_page, 0, 0);
        lv_obj_set_style_text_color(game_page, FG_PRIMARY, 0);
        lv_obj_set_style_text_font(game_page, notification_22, 0);

        app_header_create(game_page, "2048");

        lv_obj_t * game_restart = lv_button_create(game_page);
        lv_obj_set_name(game_restart, "game_restart");
        lv_obj_set_x(game_restart, 286);
        lv_obj_set_y(game_restart, 22);
        lv_obj_set_width(game_restart, 80);
        lv_obj_set_height(game_restart, 42);
        lv_obj_set_style_bg_color(game_restart, lv_color_hex(0x29251f), 0);
        lv_obj_set_style_bg_opa(game_restart, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(game_restart, 0, 0);
        lv_obj_set_style_shadow_width(game_restart, 0, 0);
        lv_obj_set_style_radius(game_restart, 14, 0);
        lv_obj_set_style_pad_all(game_restart, 0, 0);
        lv_obj_t * lv_label_0 = lv_label_create(game_restart);
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
        lv_label_set_text(lv_label_0, "重来");
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0xf2c56b), 0);

        lv_obj_t * game_score = lv_label_create(game_page);
        lv_obj_set_name(game_score, "game_score");
        lv_obj_set_x(game_score, 35);
        lv_obj_set_y(game_score, 73);
        lv_obj_set_width(game_score, 158);
        lv_obj_set_height(game_score, 34);
        lv_label_set_text(game_score, "得分 0");
        lv_obj_set_style_text_color(game_score, lv_color_hex(0xf2c56b), 0);

        lv_obj_t * game_best = lv_label_create(game_page);
        lv_obj_set_name(game_best, "game_best");
        lv_obj_set_x(game_best, 199);
        lv_obj_set_y(game_best, 73);
        lv_obj_set_width(game_best, 156);
        lv_obj_set_height(game_best, 34);
        lv_label_set_text(game_best, "最高 0");
        lv_obj_set_style_text_align(game_best, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(game_best, FG_SECONDARY, 0);

        lv_obj_t * game_board = lv_obj_create(game_page);
        lv_obj_set_name(game_board, "game_board");
        lv_obj_set_x(game_board, 35);
        lv_obj_set_y(game_board, 112);
        lv_obj_set_width(game_board, 320);
        lv_obj_set_height(game_board, 320);
        lv_obj_set_flag(game_board, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(game_board, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_style_bg_opa(game_board, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(game_board, 0, 0);
        lv_obj_set_style_pad_all(game_board, 0, 0);
        lv_obj_set_style_radius(game_board, 0, 0);
        lv_obj_t * game_tile_0 = game_tile_create(game_board, "2", lv_color_hex(0xede4d6));
        lv_obj_set_name(game_tile_0, "game_tile_0");
        lv_obj_set_x(game_tile_0, 0);
        lv_obj_set_y(game_tile_0, 0);

        lv_obj_t * game_tile_1 = game_tile_create(game_board, "", lv_color_hex(0x191c20));
        lv_obj_set_name(game_tile_1, "game_tile_1");
        lv_obj_set_x(game_tile_1, 82);
        lv_obj_set_y(game_tile_1, 0);

        lv_obj_t * game_tile_2 = game_tile_create(game_board, "4", lv_color_hex(0xead6ac));
        lv_obj_set_name(game_tile_2, "game_tile_2");
        lv_obj_set_x(game_tile_2, 164);
        lv_obj_set_y(game_tile_2, 0);

        lv_obj_t * game_tile_3 = game_tile_create(game_board, "8", lv_color_hex(0xeea365));
        lv_obj_set_name(game_tile_3, "game_tile_3");
        lv_obj_set_x(game_tile_3, 246);
        lv_obj_set_y(game_tile_3, 0);

        lv_obj_t * game_tile_4 = game_tile_create(game_board, "16", lv_color_hex(0xec8354));
        lv_obj_set_name(game_tile_4, "game_tile_4");
        lv_obj_set_x(game_tile_4, 0);
        lv_obj_set_y(game_tile_4, 82);

        lv_obj_t * game_tile_5 = game_tile_create(game_board, "32", lv_color_hex(0xe8664d));
        lv_obj_set_name(game_tile_5, "game_tile_5");
        lv_obj_set_x(game_tile_5, 82);
        lv_obj_set_y(game_tile_5, 82);

        lv_obj_t * game_tile_6 = game_tile_create(game_board, "64", lv_color_hex(0xdf4c42));
        lv_obj_set_name(game_tile_6, "game_tile_6");
        lv_obj_set_x(game_tile_6, 164);
        lv_obj_set_y(game_tile_6, 82);

        lv_obj_t * game_tile_7 = game_tile_create(game_board, "128", lv_color_hex(0xe3bf58));
        lv_obj_set_name(game_tile_7, "game_tile_7");
        lv_obj_set_x(game_tile_7, 246);
        lv_obj_set_y(game_tile_7, 82);

        lv_obj_t * game_tile_8 = game_tile_create(game_board, "256", lv_color_hex(0xd8ad3e));
        lv_obj_set_name(game_tile_8, "game_tile_8");
        lv_obj_set_x(game_tile_8, 0);
        lv_obj_set_y(game_tile_8, 164);

        lv_obj_t * game_tile_9 = game_tile_create(game_board, "512", lv_color_hex(0xc99b2b));
        lv_obj_set_name(game_tile_9, "game_tile_9");
        lv_obj_set_x(game_tile_9, 82);
        lv_obj_set_y(game_tile_9, 164);

        lv_obj_t * game_tile_10 = game_tile_create(game_board, "1024", lv_color_hex(0xbc8820));
        lv_obj_set_name(game_tile_10, "game_tile_10");
        lv_obj_set_x(game_tile_10, 164);
        lv_obj_set_y(game_tile_10, 164);

        lv_obj_t * game_tile_11 = game_tile_create(game_board, "2048", lv_color_hex(0xefd068));
        lv_obj_set_name(game_tile_11, "game_tile_11");
        lv_obj_set_x(game_tile_11, 246);
        lv_obj_set_y(game_tile_11, 164);

        lv_obj_t * game_tile_12 = game_tile_create(game_board, "", lv_color_hex(0x191c20));
        lv_obj_set_name(game_tile_12, "game_tile_12");
        lv_obj_set_x(game_tile_12, 0);
        lv_obj_set_y(game_tile_12, 246);

        lv_obj_t * game_tile_13 = game_tile_create(game_board, "2", lv_color_hex(0xede4d6));
        lv_obj_set_name(game_tile_13, "game_tile_13");
        lv_obj_set_x(game_tile_13, 82);
        lv_obj_set_y(game_tile_13, 246);

        lv_obj_t * game_tile_14 = game_tile_create(game_board, "", lv_color_hex(0x191c20));
        lv_obj_set_name(game_tile_14, "game_tile_14");
        lv_obj_set_x(game_tile_14, 164);
        lv_obj_set_y(game_tile_14, 246);

        lv_obj_t * game_tile_15 = game_tile_create(game_board, "4", lv_color_hex(0xead6ac));
        lv_obj_set_name(game_tile_15, "game_tile_15");
        lv_obj_set_x(game_tile_15, 246);
        lv_obj_set_y(game_tile_15, 246);

        lv_obj_t * game_overlay = lv_obj_create(game_page);
        lv_obj_set_name(game_overlay, "game_overlay");
        lv_obj_set_width(game_overlay, 390);
        lv_obj_set_height(game_overlay, 450);
        lv_obj_set_flag(game_overlay, LV_OBJ_FLAG_HIDDEN, true);
        lv_obj_set_flag(game_overlay, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(game_overlay, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_style_bg_color(game_overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(game_overlay, (255 * 80 / 100), 0);
        lv_obj_set_style_border_width(game_overlay, 0, 0);
        lv_obj_set_style_radius(game_overlay, 0, 0);
        lv_obj_set_style_pad_all(game_overlay, 0, 0);
        lv_obj_t * lv_obj_0 = lv_obj_create(game_overlay);
        lv_obj_set_x(lv_obj_0, 35);
        lv_obj_set_y(lv_obj_0, 123);
        lv_obj_set_width(lv_obj_0, 320);
        lv_obj_set_height(lv_obj_0, 230);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, lv_color_hex(0x24211c), 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 1, 0);
        lv_obj_set_style_border_color(lv_obj_0, lv_color_hex(0x645333), 0);
        lv_obj_set_style_radius(lv_obj_0, 24, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_t * game_message = lv_label_create(lv_obj_0);
        lv_obj_set_name(game_message, "game_message");
        lv_obj_set_x(game_message, 16);
        lv_obj_set_y(game_message, 25);
        lv_obj_set_width(game_message, 288);
        lv_label_set_text(game_message, "重新开始？");
        lv_obj_set_style_text_align(game_message, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * game_detail = lv_label_create(lv_obj_0);
        lv_obj_set_name(game_detail, "game_detail");
        lv_obj_set_x(game_detail, 16);
        lv_obj_set_y(game_detail, 75);
        lv_obj_set_width(game_detail, 288);
        lv_label_set_text(game_detail, "本局进度将清空");
        lv_obj_set_style_text_align(game_detail, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(game_detail, FG_SECONDARY, 0);

        lv_obj_t * game_cancel = lv_button_create(lv_obj_0);
        lv_obj_set_name(game_cancel, "game_cancel");
        lv_obj_set_x(game_cancel, 16);
        lv_obj_set_y(game_cancel, 145);
        lv_obj_set_width(game_cancel, 136);
        lv_obj_set_height(game_cancel, 58);
        lv_obj_set_style_bg_color(game_cancel, lv_color_hex(0x3b3831), 0);
        lv_obj_set_style_bg_opa(game_cancel, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(game_cancel, 0, 0);
        lv_obj_set_style_shadow_width(game_cancel, 0, 0);
        lv_obj_set_style_radius(game_cancel, 16, 0);
        lv_obj_t * game_cancel_text = lv_label_create(game_cancel);
        lv_obj_set_name(game_cancel_text, "game_cancel_text");
        lv_obj_set_align(game_cancel_text, LV_ALIGN_CENTER);
        lv_label_set_text(game_cancel_text, "继续");
        lv_obj_set_style_text_color(game_cancel_text, FG_PRIMARY, 0);

        lv_obj_t * game_confirm = lv_button_create(lv_obj_0);
        lv_obj_set_name(game_confirm, "game_confirm");
        lv_obj_set_x(game_confirm, 168);
        lv_obj_set_y(game_confirm, 145);
        lv_obj_set_width(game_confirm, 136);
        lv_obj_set_height(game_confirm, 58);
        lv_obj_set_style_bg_color(game_confirm, lv_color_hex(0xf2c56b), 0);
        lv_obj_set_style_bg_opa(game_confirm, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(game_confirm, 0, 0);
        lv_obj_set_style_shadow_width(game_confirm, 0, 0);
        lv_obj_set_style_radius(game_confirm, 16, 0);
        lv_obj_t * game_confirm_text = lv_label_create(game_confirm);
        lv_obj_set_name(game_confirm_text, "game_confirm_text");
        lv_obj_set_align(game_confirm_text, LV_ALIGN_CENTER);
        lv_label_set_text(game_confirm_text, "重来");
        lv_obj_set_style_text_color(game_confirm_text, lv_color_hex(0x262018), 0);

        the_root = game_page;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

