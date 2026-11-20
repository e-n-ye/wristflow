/**
 * @file wristflow_ota_ui_gen.h
 */

#ifndef WRISTFLOW_OTA_UI_GEN_H
#define WRISTFLOW_OTA_UI_GEN_H

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

void wristflow_ota_ui_set_target(uint32_t target);
uint32_t wristflow_ota_ui_get_target(void);
bool wristflow_ota_ui_check_target(uint32_t target);

/*********************
 *      DEFINES
 *********************/

#define WRISTFLOW_OTA_UI_TARGET_UNDEFINED          (0 << 1)
#define WRISTFLOW_OTA_UI_TARGET_HUANGSHAN_390X450  (1 << 1)
#define WRISTFLOW_OTA_UI_TARGET_ALL                0x0FFFFFFF

/* By default compile for all targets, allowing to switch to any targets at runtime */
#ifndef WRISTFLOW_OTA_UI_COMPILE_TARGET
#define WRISTFLOW_OTA_UI_COMPILE_TARGET WRISTFLOW_OTA_UI_TARGET_ALL
#endif

#define WRISTFLOW_OTA_UI_CHECK_COMPILE_TARGET(target) (WRISTFLOW_OTA_UI_COMPILE_TARGET & (target) ? 1 : 0)

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

/*----------------
 * Fonts
 *----------------*/

/* Targets: any */
extern lv_font_t * ota_title;
extern lv_font_t * ota_hint;
extern lv_font_t * ota_digits;
extern lv_font_t * ota_bluetooth;


/*----------------
 * Images
 *----------------*/



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

void wristflow_ota_ui_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widgets, components and screens of this library*/
#include "screens/screen_ota_receiver_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WRISTFLOW_OTA_UI_GEN_H*/