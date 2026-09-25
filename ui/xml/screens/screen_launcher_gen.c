/**
 * @file screen_launcher_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_launcher_gen.h"
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

lv_obj_t * screen_launcher_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_launcher_#");
        lv_obj_set_width(lv_obj_0, 390);
        lv_obj_set_height(lv_obj_0, 450);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_color(lv_obj_0, BG_BLACK, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_text_font(lv_obj_0, body_20, 0);
        lv_obj_set_style_text_color(lv_obj_0, FG_PRIMARY, 0);

        lv_obj_t * launcher_scroll = lv_obj_create(lv_obj_0);
        lv_obj_set_name(launcher_scroll, "launcher_scroll");
        lv_obj_set_x(launcher_scroll, 0);
        lv_obj_set_y(launcher_scroll, 0);
        lv_obj_set_width(launcher_scroll, 390);
        lv_obj_set_height(launcher_scroll, 402);
        lv_obj_set_scrollbar_mode(launcher_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(launcher_scroll, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(launcher_scroll, 0, 0);
        lv_obj_set_style_pad_all(launcher_scroll, 0, 0);
        lv_obj_set_style_radius(launcher_scroll, 0, 0);
        lv_obj_t * launcher_canvas = lv_obj_create(launcher_scroll);
        lv_obj_set_name(launcher_canvas, "launcher_canvas");
        lv_obj_set_width(launcher_canvas, 650);
        lv_obj_set_height(launcher_canvas, 650);
        lv_obj_set_flag(launcher_canvas, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(launcher_canvas, (255 * 0 / 100), 0);
        lv_obj_set_style_border_width(launcher_canvas, 0, 0);
        lv_obj_set_style_pad_all(launcher_canvas, 0, 0);
        lv_obj_set_style_radius(launcher_canvas, 0, 0);
        lv_obj_t * launch_stopwatch = launcher_icon_create(launcher_canvas, "", lv_color_hex(0xf2a33b));
        lv_obj_set_name(launch_stopwatch, "launch_stopwatch");
        lv_obj_set_x(launch_stopwatch, 275);
        lv_obj_set_y(launch_stopwatch, 275);

        lv_obj_t * launch_activity = launcher_icon_create(launcher_canvas, "", lv_color_hex(0x16ba82));
        lv_obj_set_name(launch_activity, "launch_activity");
        lv_obj_set_x(launch_activity, 155);
        lv_obj_set_y(launch_activity, 275);

        lv_obj_t * launch_heart = launcher_icon_create(launcher_canvas, "", lv_color_hex(0xef4770));
        lv_obj_set_name(launch_heart, "launch_heart");
        lv_obj_set_x(launch_heart, 395);
        lv_obj_set_y(launch_heart, 275);

        lv_obj_t * launch_faces = launcher_icon_create(launcher_canvas, "", lv_color_hex(0x616c77));
        lv_obj_set_name(launch_faces, "launch_faces");
        lv_obj_set_x(launch_faces, 215);
        lv_obj_set_y(launch_faces, 171);

        lv_obj_t * launch_system = launcher_icon_create(launcher_canvas, "", lv_color_hex(0x21a1bd));
        lv_obj_set_name(launch_system, "launch_system");
        lv_obj_set_x(launch_system, 335);
        lv_obj_set_y(launch_system, 171);

        lv_obj_t * launch_flashlight = launcher_icon_create(launcher_canvas, "", lv_color_hex(0x8b71bf));
        lv_obj_set_name(launch_flashlight, "launch_flashlight");
        lv_obj_set_x(launch_flashlight, 215);
        lv_obj_set_y(launch_flashlight, 379);

        lv_obj_t * launch_settings = launcher_icon_create(launcher_canvas, "", lv_color_hex(0x647783));
        lv_obj_set_name(launch_settings, "launch_settings");
        lv_obj_set_x(launch_settings, 335);
        lv_obj_set_y(launch_settings, 379);

        lv_obj_t * launcher_caption = lv_label_create(lv_obj_0);
        lv_obj_set_name(launcher_caption, "launcher_caption");
        lv_obj_set_x(launcher_caption, 24);
        lv_obj_set_y(launcher_caption, 404);
        lv_obj_set_width(launcher_caption, 342);
        lv_label_set_text(launcher_caption, "应用");
        lv_obj_set_style_text_align(launcher_caption, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(launcher_caption, FG_SECONDARY, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

