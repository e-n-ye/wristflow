#ifndef WRISTFLOW_PRODUCT_COMPASS_H
#define WRISTFLOW_PRODUCT_COMPASS_H
#include "compass.h"

/* UI-thread qualification only; the existing sensor worker owns every I2C.
 * Leaving/sleeping invalidates the cache immediately. A fault latches until a
 * new visible-page session; failed cleanup retains ownership/IDLE responsibility. */
bool wristflow_product_compass_active(bool active);
wf_compass_snapshot_t wristflow_product_compass_snapshot(void);
#endif
