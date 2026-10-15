#ifndef WRISTFLOW_PRODUCT_POWER_H
#define WRISTFLOW_PRODUCT_POWER_H

#include "power_state.h"

/* Worker-only device I/O. No timer or charge writes. Publish voltage before PG
 * so a slow I2C observation cannot delay the independent ADC result. */
wristflow_power_snapshot_t wristflow_product_power_voltage(void);
void wristflow_product_power_input(wristflow_power_snapshot_t *sample);

/* UI-facing operations only enqueue/copy. No device access or waiting. */
bool wristflow_product_power_start(void);
void wristflow_product_power_screen(bool lit);
void wristflow_product_power_request(void);
wristflow_power_snapshot_t wristflow_product_power_snapshot(void);

#endif
