/**
 * @file card_container_gen.h
 */

#ifndef CARD_CONTAINER_H
#define CARD_CONTAINER_H

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

lv_obj_t * card_container_create(lv_obj_t * parent, const char * card_title, const char * metric_val, const char * icon, lv_color_t surface_color, lv_color_t value_color, lv_color_t caption_color, lv_color_t accent_color, lv_coord_t card_width, lv_coord_t card_height);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CARD_CONTAINER_H*/