#ifndef WRISTFLOW_PRODUCT_CHARGER_H
#define WRISTFLOW_PRODUCT_CHARGER_H
#include <stdbool.h>

void wristflow_product_charger_report(void);
/* Startup only, after ADC synchronization is initialized. Never enables charge. */
bool wristflow_product_charger_service_start(void);

#endif
