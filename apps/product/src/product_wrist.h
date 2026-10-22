#ifndef WRISTFLOW_PRODUCT_WRIST_H
#define WRISTFLOW_PRODUCT_WRIST_H
#include <stdbool.h>

/* Enqueue a preference change; the existing IMU worker owns all hardware. */
void wristflow_product_wrist_enable(bool enabled);
/* Consume a coalesced, source-validated wake on the UI thread. Disable clears
 * pending wakes immediately, before asynchronous register cleanup. */
bool wristflow_product_wrist_take_wake(void);
/* Cached worker publication; no device IO on the caller. */
bool wristflow_product_wrist_suspended(void);
bool wristflow_product_wrist_failed(void);
#endif
