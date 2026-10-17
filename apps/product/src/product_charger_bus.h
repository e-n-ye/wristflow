#ifndef WRISTFLOW_PRODUCT_CHARGER_BUS_H
#define WRISTFLOW_PRODUCT_CHARGER_BUS_H
#include <stdbool.h>
/* Caller owns i2c2->lock and IDLE. Keep shared rail/mux through the session. */
bool wristflow_product_charger_bus_prepare_locked(void);
/* After verified stop, return the lease under the same bus lock. */
bool wristflow_product_charger_bus_release(void);
#endif
