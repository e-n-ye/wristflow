#ifndef WF_PRODUCT_CYCLE_H
#define WF_PRODUCT_CYCLE_H
#include "battery_trace.h"
#ifdef WF_CYCLE_LOG
bool wf_product_cycle_start(void);
void wf_product_cycle_observe(const wristflow_battery_record_t *,bool flashlight);
bool wf_product_cycle_inhibit(void);
#else
static inline bool wf_product_cycle_start(void) {return true;}
static inline void wf_product_cycle_observe(const wristflow_battery_record_t *r,bool f) {(void)r;(void)f;}
static inline bool wf_product_cycle_inhibit(void) {return false;}
#endif
#endif
