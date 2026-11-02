#ifndef WF_PRODUCT_CYCLE_H
#define WF_PRODUCT_CYCLE_H
#include "cycle_record.h"
#ifdef WF_CYCLE_LOG
bool wf_product_cycle_start(void);
void wf_product_cycle_observe(const wristflow_battery_record_t *,bool flashlight);
void wf_product_cycle_context(const wristflow_battery_record_t *,bool flashlight,uint32_t keep_remaining_ms);
/* IRQ-safe bounded signal queue. Rows separate callback tick from worker
 * consumption time. Neither is proof that the physical panel lit. */
void wf_product_cycle_signal(unsigned event);
bool wf_product_cycle_sampling(void);
bool wf_product_cycle_inhibit(void);
#else
static inline bool wf_product_cycle_start(void) {return true;}
static inline void wf_product_cycle_observe(const wristflow_battery_record_t *r,bool f) {(void)r;(void)f;}
static inline void wf_product_cycle_context(const wristflow_battery_record_t *r,bool f,uint32_t k) {(void)r;(void)f;(void)k;}
static inline void wf_product_cycle_signal(unsigned e) {(void)e;}
static inline bool wf_product_cycle_sampling(void) {return false;}
static inline bool wf_product_cycle_inhibit(void) {return false;}
#endif
#endif
