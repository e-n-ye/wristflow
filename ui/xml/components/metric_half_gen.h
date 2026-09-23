/**
 * @file metric_half_gen.h
 */

#ifndef METRIC_HALF_H
#define METRIC_HALF_H

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

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_obj_t * metric_half_create(lv_obj_t * parent, const char * title, const char * value, const char * detail, const char * icon, lv_color_t surface_color, lv_color_t text_color, lv_color_t accent_color);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*METRIC_HALF_H*/