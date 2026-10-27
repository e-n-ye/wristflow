#ifndef WF_RESOURCE_PROBE_H
#define WF_RESOURCE_PROBE_H
#include "ui_shell.h"

/* Explicit diagnostic build only. Every function runs on the UI thread.
 * No clock, notification store, settings, sensor or hardware ownership. */
typedef struct {
    wristflow_ui_shell_t *shell;
    lv_obj_t *root;
    uint32_t tag, started, elapsed;
    unsigned mode, reason;
    bool active;
} wf_resource_probe_t;
bool wf_resource_probe_start(wf_resource_probe_t *, wristflow_ui_shell_t *, unsigned mode, uint32_t tag);
void wf_resource_probe_stop(wf_resource_probe_t *, unsigned reason);
void wf_resource_probe_tick(wf_resource_probe_t *, bool eligible);
#endif
