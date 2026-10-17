#ifndef WRISTFLOW_PRODUCT_CHARGER_H
#define WRISTFLOW_PRODUCT_CHARGER_H
#include <stdbool.h>
#include "power_state.h"

void wristflow_product_charger_report(void);
/* Startup only; early board hook inhibits, worker then owns automatic USB cycles. */
bool wristflow_product_charger_service_start(void);
/* Cached display fields only: no I/O or charging control from UI. */
void wristflow_product_charger_display(wristflow_power_snapshot_t *power);

#endif
