#ifndef WRISTFLOW_PRODUCT_LIGHT_H
#define WRISTFLOW_PRODUCT_LIGHT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool enabled, active, valid, fault, cleanup_pending;
    uint8_t brightness;
    uint32_t millilux, sampled_ms, session, sequence;
} wristflow_product_light_snapshot_t;
/* UI commands/cache only; all I2C stays on the existing sensor worker.
 * Failure closes the RAM mode. Explicit re-enable is the only retry.
 * ACTIVE qualification is separate from the saved user mode. */
bool wristflow_product_light_enable(bool enabled);
void wristflow_product_light_active(bool active);
wristflow_product_light_snapshot_t wristflow_product_light_snapshot(void);
#endif
