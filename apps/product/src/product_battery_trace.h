#ifndef WRISTFLOW_PRODUCT_BATTERY_TRACE_H
#define WRISTFLOW_PRODUCT_BATTERY_TRACE_H
#include "battery_trace.h"

/* UI-thread producer. Shell pointers and estimator state never cross threads. */
void wristflow_product_battery_trace_observe(const wristflow_battery_record_t *record);
#endif
