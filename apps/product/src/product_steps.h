#ifndef WRISTFLOW_PRODUCT_STEPS_H
#define WRISTFLOW_PRODUCT_STEPS_H
#include "daily_steps.h"
/* The existing IMU worker remains the only bus/device owner. */
bool wristflow_product_steps_start(void);
wristflow_steps_snapshot_t wristflow_product_steps_snapshot(void);
void wristflow_product_steps_screen(bool visible);
void wristflow_product_steps_clock_changed(void);
#endif
