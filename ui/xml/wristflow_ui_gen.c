/**
 * @file wristflow_ui_gen.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "wristflow_ui_gen.h"

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

static uint32_t wristflow_ui_target = WRISTFLOW_UI_TARGET_ALL;

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

lv_font_t * body_20;
extern lv_font_t body_20_data;
lv_font_t * title_24;
extern lv_font_t title_24_data;
lv_font_t * metric_56;
extern lv_font_t metric_56_data;
lv_font_t * value_36;
extern lv_font_t value_36_data;
lv_font_t * icons_44;
extern lv_font_t icons_44_data;
lv_font_t * icons_20;
extern lv_font_t icons_20_data;

/*----------------
 * Images
 *----------------*/

/* Targets: any */
const void * hour_seven = NULL;
extern const void * hour_seven_data;

/*----------------
 * Global styles
 *----------------*/

lv_style_t screen_base;
lv_style_t card_base;
lv_style_t text_title;
lv_style_t text_secondary;
lv_style_t text_muted;

/*----------------
 * Subjects
 *----------------*/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void wristflow_ui_init_gen(const char * asset_path)
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

    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        if (!body_20) {
            /* body_20 */
            /* get font 'body_20' from a C array */
            body_20 = &body_20_data;

        }
        if (!title_24) {
            /* title_24 */
            /* get font 'title_24' from a C array */
            title_24 = &title_24_data;

        }
        if (!metric_56) {
            /* metric_56 */
            /* get font 'metric_56' from a C array */
            metric_56 = &metric_56_data;

        }
        if (!value_36) {
            /* value_36 */
            /* get font 'value_36' from a C array */
            value_36 = &value_36_data;

        }
        if (!icons_44) {
            /* icons_44 */
            /* get font 'icons_44' from a C array */
            icons_44 = &icons_44_data;

        }
        if (!icons_20) {
            /* icons_20 */
            /* get font 'icons_20' from a C array */
            icons_20 = &icons_20_data;

        }
    }
    #endif

    /*----------------
     * Images
     *----------------*/

    /* Targets: any */
    #if WRISTFLOW_UI_CHECK_COMPILE_TARGET(WRISTFLOW_UI_TARGET_ALL)
    if (wristflow_ui_check_target(WRISTFLOW_UI_TARGET_ALL)) {
        /* hour_seven */
        if (!hour_seven) {
            hour_seven = &hour_seven_data;
        }
    }
    #endif

    /*----------------
     * Global styles
     *----------------*/

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&screen_base);
        lv_style_init(&card_base);
        lv_style_init(&text_title);
        lv_style_init(&text_secondary);
        lv_style_init(&text_muted);

        lv_style_set_bg_color(&screen_base, BG_BLACK);
        lv_style_set_text_color(&screen_base, FG_PRIMARY);
        lv_style_set_bg_color(&card_base, BG_SURFACE);
        lv_style_set_radius(&card_base, RADIUS_MD);
        lv_style_set_text_color(&card_base, FG_PRIMARY);
        lv_style_set_text_color(&text_title, FG_PRIMARY);
        lv_style_set_text_color(&text_secondary, FG_SECONDARY);
        lv_style_set_text_color(&text_muted, FG_MUTED);

        style_inited = true;
    }

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
    check_font(&body_20, "body_20");
    check_font(&title_24, "title_24");
    check_font(&metric_56, "metric_56");
    check_font(&value_36, "value_36");
    check_font(&icons_44, "icons_44");
    check_font(&icons_20, "icons_20");

    /* Register fonts */
    lv_xml_register_font(NULL, "body_20", body_20);
    lv_xml_register_font(NULL, "title_24", title_24);
    lv_xml_register_font(NULL, "metric_56", metric_56);
    lv_xml_register_font(NULL, "value_36", value_36);
    lv_xml_register_font(NULL, "icons_44", icons_44);
    lv_xml_register_font(NULL, "icons_20", icons_20);

    /* Register subjects */

    /* Register callbacks */
#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if defined(LV_USE_XML) && LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    /* Register images */
    lv_xml_register_image(NULL, "hour_seven", hour_seven);
#endif

#if defined(LV_USE_XML) && LV_USE_XML == 0
    /*--------------------
     *  Permanent screens
     *-------------------*/
    /* If XML is enabled it's assumed that the permanent screens are created
     * manually from XML using lv_xml_create() */
#endif
}

void wristflow_ui_set_target(uint32_t target)
{
    wristflow_ui_target = target;
}

uint32_t wristflow_ui_get_target(void)
{
    return wristflow_ui_target;
}

bool wristflow_ui_check_target(uint32_t target)
{
    return (wristflow_ui_target & target) ? true : false;
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