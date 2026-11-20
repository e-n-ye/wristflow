/**
 * @file wristflow_ota_ui_gen.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "wristflow_ota_ui_gen.h"

#if defined(LV_USE_XML) && LV_USE_XML
#endif /* LV_USE_XML */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void check_font(lv_font_t ** font, const char * name);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t wristflow_ota_ui_target = WRISTFLOW_OTA_UI_TARGET_ALL;

/*----------------
 * Translations
 *----------------*/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/*--------------------
 *  Permanent screens
 *-------------------*/

/*----------------
 * Fonts
 *----------------*/

lv_font_t * ota_title;
extern lv_font_t ota_title_data;
lv_font_t * ota_hint;
extern lv_font_t ota_hint_data;
lv_font_t * ota_digits;
extern lv_font_t ota_digits_data;
lv_font_t * ota_bluetooth;
extern lv_font_t ota_bluetooth_data;

/*----------------
 * Images
 *----------------*/



/*----------------
 * Global styles
 *----------------*/

/*----------------
 * Subjects
 *----------------*/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void wristflow_ota_ui_init_gen(const char * asset_path)
{
    /* When running from the editor the theme set from the XML should overwrite this */
#if !defined(LV_EDITOR_PREVIEW)
#if LV_USE_THEME_SIMPLE
    lv_display_t * disp = lv_display_get_default();
    lv_theme_t * th = lv_theme_simple_init(disp);
    lv_display_set_theme(disp, th);
#else
    LV_LOG_WARN("Simple theme is selected in project.xml but LV_USE_THEME_SIMPLE is disabled");
#endif
#endif /*LV_EDITOR_PREVIEW*/


    /*----------------
     * Fonts
     *----------------*/

    /* Targets: any */

    #if WRISTFLOW_OTA_UI_CHECK_COMPILE_TARGET(WRISTFLOW_OTA_UI_TARGET_ALL)
    if (wristflow_ota_ui_check_target(WRISTFLOW_OTA_UI_TARGET_ALL)) {
        if (!ota_title) {
            /* ota_title */
            /* get font 'ota_title' from a C array */
            ota_title = &ota_title_data;

        }
        if (!ota_hint) {
            /* ota_hint */
            /* get font 'ota_hint' from a C array */
            ota_hint = &ota_hint_data;

        }
        if (!ota_digits) {
            /* ota_digits */
            /* get font 'ota_digits' from a C array */
            ota_digits = &ota_digits_data;

        }
        if (!ota_bluetooth) {
            /* ota_bluetooth */
            /* get font 'ota_bluetooth' from a C array */
            ota_bluetooth = &ota_bluetooth_data;

        }
    }
    #endif

    /*----------------
     * Images
     *----------------*/



    /*----------------
     * Global styles
     *----------------*/

    /*----------------
     * Subjects
     *----------------*/
    /*----------------
     * Translations
     *----------------*/

#if defined(LV_USE_XML) && LV_USE_XML
    /* Register widgets */

    /* Check all fonts / default if needed. This prevents fonts that are used in one target but
       defined in another from causing assertion failures during rendering of the Preview. */
    check_font(&ota_title, "ota_title");
    check_font(&ota_hint, "ota_hint");
    check_font(&ota_digits, "ota_digits");
    check_font(&ota_bluetooth, "ota_bluetooth");

    /* Register fonts */
    lv_xml_register_font(NULL, "ota_title", ota_title);
    lv_xml_register_font(NULL, "ota_hint", ota_hint);
    lv_xml_register_font(NULL, "ota_digits", ota_digits);
    lv_xml_register_font(NULL, "ota_bluetooth", ota_bluetooth);

    /* Register subjects */

    /* Register callbacks */
#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if defined(LV_USE_XML) && LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    /* Register images */
#endif

#if defined(LV_USE_XML) && LV_USE_XML == 0
    /*--------------------
     *  Permanent screens
     *-------------------*/
    /* If XML is enabled it's assumed that the permanent screens are created
     * manually from XML using lv_xml_create() */
#endif
}

void wristflow_ota_ui_set_target(uint32_t target)
{
    wristflow_ota_ui_target = target;
}

uint32_t wristflow_ota_ui_get_target(void)
{
    return wristflow_ota_ui_target;
}

bool wristflow_ota_ui_check_target(uint32_t target)
{
    return (wristflow_ota_ui_target & target) ? true : false;
}

/* Callbacks */

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void check_font(lv_font_t ** font, const char * name)
{
    if (!(*font)) {
        *font = (lv_font_t *)LV_FONT_DEFAULT;
        LV_LOG_WARN("font `%s` was not set. Using `LV_FONT_DEFAULT` instead", name);
    }
}