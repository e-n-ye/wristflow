#ifndef WRISTFLOW_PRODUCT_ALARM_H
#define WRISTFLOW_PRODUCT_ALARM_H
#include "alarm_port.h"
/* Boot after services/PM, before BLE. No LVGL, owns one mutex, one bounded RT
 * soft deadline and one background storage worker. Port outlives all views. */
const wristflow_alarm_port_t *wristflow_product_alarm_start(void);
void wristflow_product_alarm_clock_changed(void);
#endif
