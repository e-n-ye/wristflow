#ifndef WRISTFLOW_AOD_DISPLAY_H
#define WRISTFLOW_AOD_DISPLAY_H
#include "aod_panel_probe.h"

/* Physical panel state, separate from the sleeping navigation policy. AOD
 * retains panel power and the normal IDLE guard until retention is measured. */
typedef enum { WF_AOD_PANEL_NORMAL, WF_AOD_PANEL_RESIDENT, WF_AOD_PANEL_FAULT } wf_aod_panel_phase_t;
typedef struct {
    wf_aod_panel_phase_t phase;
    uint32_t frame_minute;
    bool owned, failed;
} wf_aod_display_t;
typedef struct {
    bool (*observe)(wf_panel_observation_t *, void *);
    bool (*brightness)(uint8_t, void *);
    bool (*idle)(bool, void *);
    bool (*draw)(void *); /* One static frame and completed transfer. */
    void (*hide)(void *);
} wf_aod_display_io_t;

/* UI thread only. No periodic redraw when minute identity is unchanged. */
bool wf_aod_display_frame(wf_aod_display_t *, const wf_aod_display_io_t *, void *, uint32_t minute);
/* All recovery operations are attempted. Failure retains exclusive LCD ownership. */
bool wf_aod_display_restore(wf_aod_display_t *, const wf_aod_display_io_t *, void *);
#endif
