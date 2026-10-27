#ifndef WF_PRODUCT_RESOURCE_PROBE_H
#define WF_PRODUCT_RESOURCE_PROBE_H
#include "ui_shell.h"

#ifdef WF_RESOURCE_PROBE
/* UI thread only: objects, pointer cleanup and the deadline lease stay here.
 * panel_reserved must outlive this module; all reads/writes use IRQ protection. */
void wf_product_resource_init(wristflow_ui_shell_t *, lv_indev_t *, const bool *panel_reserved);
void wf_product_resource_poll(void);
uint32_t wf_product_resource_events(uint32_t events);
/* Called inside the existing AOD mailbox's IRQ-disabled critical section. */
bool wf_product_resource_busy_locked(void);
#else
static inline void wf_product_resource_init(wristflow_ui_shell_t *s, lv_indev_t *p, const bool *r) { (void)s; (void)p; (void)r; }
static inline void wf_product_resource_poll(void) {}
static inline uint32_t wf_product_resource_events(uint32_t e) { return e; }
static inline bool wf_product_resource_busy_locked(void) { return false; }
#endif
#endif
