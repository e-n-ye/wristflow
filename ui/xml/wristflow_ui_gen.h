/**
 * @file wristflow_ui_gen.h
 */

#ifndef WRISTFLOW_UI_GEN_H
#define WRISTFLOW_UI_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
    #include "lvgl_private.h"
#else
    #include "lvgl/lvgl.h"
    #include "lvgl/lvgl_private.h"
#endif



/* Prototypes for target functions, needed by responsive const definitions */

void wristflow_ui_set_target(uint32_t target);
uint32_t wristflow_ui_get_target(void);
bool wristflow_ui_check_target(uint32_t target);

/*********************
 *      DEFINES
 *********************/

#define WRISTFLOW_UI_TARGET_UNDEFINED          (0 << 1)
#define WRISTFLOW_UI_TARGET_HUANGSHAN_390X450  (1 << 1)
#define WRISTFLOW_UI_TARGET_ALL                0x0FFFFFFF

/* By default compile for all targets, allowing to switch to any targets at runtime */
#ifndef WRISTFLOW_UI_COMPILE_TARGET
#define WRISTFLOW_UI_COMPILE_TARGET WRISTFLOW_UI_TARGET_ALL
#endif

#define WRISTFLOW_UI_CHECK_COMPILE_TARGET(target) (WRISTFLOW_UI_COMPILE_TARGET & (target) ? 1 : 0)

/**
 * Target AMOLED width
 */
#define SCREEN_WIDTH 390
/**
 * Target AMOLED height
 */
#define SCREEN_HEIGHT 450
/**
 * Top status bar safe inset
 */
#define SAFE_TOP 24
/**
 * Bottom indicator safe inset
 */
#define SAFE_BOTTOM 20
/**
 * Left/right screen edge safe inset
 */
#define SAFE_SIDE 24
/**
 * Minimal spacing
 */
#define SPACE_2XS 4
/**
 * Compact element spacing
 */
#define SPACE_XS 8
/**
 * Default element spacing
 */
#define SPACE_SM 12
/**
 * Component spacing
 */
#define SPACE_MD 16
/**
 * Section separation
 */
#define SPACE_LG 24
/**
 * Small widget radius
 */
#define RADIUS_SM 10
/**
 * Primary card radius
 */
#define RADIUS_MD 20
/**
 * Full screen sheet radius
 */
#define RADIUS_LG 28
/**
 * Pill shape full radius
 */
#define RADIUS_PILL 999
#define TILE_QUARTER_W 165
#define TILE_ROW_H 174
#define TILE_AREA_W 342
#define TILE_AREA_H 362
#define TILE_AREA_X 24
#define TILE_AREA_Y 28
/**
 * Pager indicator dot size
 */
#define PAGER_DOT_SIZE 6
/**
 * Gap between indicator dots
 */
#define PAGER_DOT_GAP 8
/**
 * Vertical position of pager dots
 */
#define PAGER_BOTTOM_Y 423
/**
 * AMOLED pitch black background
 */
#define BG_BLACK lv_color_hex(0x000000)
/**
 * Neutral card background
 */
#define BG_SURFACE lv_color_hex(0x191c20)
/**
 * Secondary surface background
 */
#define BG_SURFACE_SUBTLE lv_color_hex(0x25292e)
/**
 * High contrast white text
 */
#define FG_PRIMARY lv_color_hex(0xffffff)
/**
 * Secondary text
 */
#define FG_SECONDARY lv_color_hex(0xa7adb5)
/**
 * Darker muted text for inactive elements
 */
#define FG_MUTED lv_color_hex(0x64748b)
/**
 * Sky blue accent for battery and system
 */
#define ACCENT_BLUE lv_color_hex(0x38bdf8)
/**
 * Rose red accent for heart rate
 */
#define ACCENT_RED lv_color_hex(0xf43f5e)
/**
 * Emerald green accent for activity/steps
 */
#define ACCENT_GREEN lv_color_hex(0x10b981)
/**
 * Orange accent for calories and warnings
 */
#define ACCENT_ORANGE lv_color_hex(0xf97316)
/**
 * Active page dot highlight
 */
#define PAGER_ACTIVE lv_color_hex(0xffffff)
/**
 * Inactive page dot
 */
#define PAGER_INACTIVE lv_color_hex(0x334155)


#ifndef LV_XML_EVAL_STRING_BUF_SIZE
    #define LV_XML_EVAL_STRING_BUF_SIZE 256
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

/*----------------
 * Global styles
 *----------------*/

extern lv_style_t screen_base;
extern lv_style_t card_base;
extern lv_style_t text_title;
extern lv_style_t text_secondary;
extern lv_style_t text_muted;

/*----------------
 * Fonts
 *----------------*/

/* Targets: any */
extern lv_font_t * body_20;
extern lv_font_t * title_24;
extern lv_font_t * metric_56;
extern lv_font_t * clock_80;
extern lv_font_t * value_36;
extern lv_font_t * icons_44;
extern lv_font_t * icons_20;


/*----------------
 * Images
 *----------------*/

/* Targets: any */
extern const void * hour_unknown;
extern const void * hour_1;
extern const void * hour_2;
extern const void * hour_3;
extern const void * hour_4;
extern const void * hour_5;
extern const void * hour_6;
extern const void * hour_7;
extern const void * hour_8;
extern const void * hour_9;
extern const void * hour_10;
extern const void * hour_11;
extern const void * hour_12;

/*----------------
 * Subjects
 *----------------*/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

/**
 * Initialize the component library
 */

void wristflow_ui_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widgets, components and screens of this library*/
#include "components/activity_summary_gen.h"
#include "components/app_header_gen.h"
#include "components/app_list_item_gen.h"
#include "components/card_container_gen.h"
#include "components/component_confirm_gen.h"
#include "components/component_group_gen.h"
#include "components/editor_card_gen.h"
#include "components/face_thumbnail_gen.h"
#include "components/launcher_icon_gen.h"
#include "components/layout_block_gen.h"
#include "components/layout_option_gen.h"
#include "components/layout_save_status_gen.h"
#include "components/metric_full_gen.h"
#include "components/metric_half_gen.h"
#include "components/page_dot_gen.h"
#include "components/page_dots_gen.h"
#include "components/picker_header_gen.h"
#include "components/picker_list_gen.h"
#include "components/product_metric_full_gen.h"
#include "components/settings_choice_gen.h"
#include "components/settings_entry_gen.h"
#include "components/settings_note_gen.h"
#include "components/settings_row_gen.h"
#include "components/tile_full_gen.h"
#include "components/tile_grid_gen.h"
#include "components/tile_half_gen.h"
#include "components/tile_quarter_gen.h"
#include "screens/screen_app_placeholder_gen.h"
#include "screens/screen_brightness_adjust_gen.h"
#include "screens/screen_component_editor_gen.h"
#include "screens/screen_component_page_gen.h"
#include "screens/screen_component_picker_gen.h"
#include "screens/screen_component_templates_gen.h"
#include "screens/screen_control_center_gen.h"
#include "screens/screen_face_picker_gen.h"
#include "screens/screen_flashlight_gen.h"
#include "screens/screen_launcher_gen.h"
#include "screens/screen_layout_picker_gen.h"
#include "screens/screen_menu_layout_gen.h"
#include "screens/screen_picker_full_gen.h"
#include "screens/screen_picker_half_gen.h"
#include "screens/screen_picker_quarter_gen.h"
#include "screens/screen_product_activity_gen.h"
#include "screens/screen_product_daily_gen.h"
#include "screens/screen_product_health_gen.h"
#include "screens/screen_product_launcher_gen.h"
#include "screens/screen_product_placeholder_gen.h"
#include "screens/screen_product_settings_gen.h"
#include "screens/screen_settings_gen.h"
#include "screens/screen_settings_list_gen.h"
#include "screens/screen_stopwatch_gen.h"
#include "screens/screen_tile_activity_gen.h"
#include "screens/screen_tile_full_gen.h"
#include "screens/screen_tile_heart_rate_gen.h"
#include "screens/screen_tile_system_gen.h"
#include "screens/screen_watchface_gen.h"
#include "screens/screen_watchface_simple_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WRISTFLOW_UI_GEN_H*/