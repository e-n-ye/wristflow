#ifndef LV_CONF_H
#define LV_CONF_H

/* SiFli enables both draw units by default. The launcher uses software
 * blending for its RGB565 arc and text and keeps the board's LCD driver. */
#undef LV_USE_DRAW_EPIC
#define LV_USE_DRAW_EPIC 0

#endif
