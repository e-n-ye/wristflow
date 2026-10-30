#ifndef WF_PRODUCT_RUNTIME_INPUT_H
#define WF_PRODUCT_RUNTIME_INPUT_H
#include "battery_trace.h"
/* UI-thread copies only. AOD/flashlight flags describe completed display state. */
void wf_product_runtime_capture(const wristflow_battery_record_t *,bool aod,bool flashlight,bool diagnostic);
#endif
