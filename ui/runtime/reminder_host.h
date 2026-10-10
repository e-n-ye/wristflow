#ifndef WRISTFLOW_REMINDER_HOST_H
#define WRISTFLOW_REMINDER_HOST_H
#include "lvgl.h"
#include "display_policy.h"
typedef struct wristflow_reminder_host wristflow_reminder_host_t;
/* One exclusive high-priority presentation slot. Content/business is owned by
 * its adapter. No navigation, DND, task scheduling or application knowledge. */
wristflow_reminder_host_t *wristflow_reminder_host_create(void);
void wristflow_reminder_host_destroy(wristflow_reminder_host_t *host);
bool wristflow_reminder_host_present(wristflow_reminder_host_t *host, lv_obj_t *content, uint32_t now);
void wristflow_reminder_host_close(wristflow_reminder_host_t *host);
bool wristflow_reminder_host_active(const wristflow_reminder_host_t *host);
wristflow_display_phase_t wristflow_reminder_host_phase(const wristflow_reminder_host_t *host, uint32_t now);
void wristflow_reminder_host_wake(wristflow_reminder_host_t *host, uint32_t now);
bool wristflow_reminder_host_touch(wristflow_reminder_host_t *host, uint32_t now, bool pressed);
#endif
