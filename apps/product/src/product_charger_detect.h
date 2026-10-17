#ifndef WRISTFLOW_PRODUCT_CHARGER_DETECT_H
#define WRISTFLOW_PRODUCT_CHARGER_DETECT_H
#include <stdbool.h>
/* Board IRQ only signals the charger worker; no register or ADC access here. */
bool wristflow_product_charger_detect_start(void (*changed)(void));
void wristflow_product_charger_detect_stop(void);
#endif
