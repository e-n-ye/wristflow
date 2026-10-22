#ifndef WRISTFLOW_AOD_PANEL_PROBE_H
#define WRISTFLOW_AOD_PANEL_PROBE_H
#include <stdbool.h>
#include <stdint.h>

/* A0 diagnostic only. Observations are SDK caches, not panel register reads. */
typedef enum { WF_PANEL_OTHER, WF_PANEL_NORMAL, WF_PANEL_IDLE } wf_panel_mode_t;
typedef struct { wf_panel_mode_t mode; uint8_t brightness; } wf_panel_observation_t;
typedef struct { bool active, held, cancel; } wf_aod_probe_input_t;
/* Consume an entire KEY press begun in the diagnostic, including a click or
 * release arriving after restoration. Caller serializes access. */
bool wf_aod_probe_filter_key(wf_aod_probe_input_t *, bool pressed, bool released);
typedef enum {
    WF_AOD_PROBE_UNUSED, WF_AOD_PROBE_RUNNING, WF_AOD_PROBE_DONE,
    WF_AOD_PROBE_CANCELLED, WF_AOD_PROBE_REJECTED, WF_AOD_PROBE_FAULT
} wf_aod_probe_phase_t;
typedef struct {
    wf_aod_probe_phase_t phase;
    uint32_t started_ms, elapsed_ms, duration_ms, interrupted_events;
    wf_panel_observation_t before, entered, after;
    uint8_t valid; /* before=1, entered=2, after=4 */
    bool restore_pending, restored;
} wf_aod_probe_t;
typedef struct {
    uint32_t (*now)(void *);
    bool (*observe)(wf_panel_observation_t *, void *);
    bool (*brightness)(uint8_t, void *);
    bool (*idle)(bool, void *);
    /* Positive: wait at most remaining_ms; any event ends the probe.
     * Zero: recovery quarantine, wait for events/explicit retry indefinitely. */
    uint32_t (*wait)(uint32_t remaining_ms, bool *cancelled, void *);
    void (*publish)(const wf_aod_probe_t *, void *);
} wf_aod_probe_io_t;

/* UI-thread owner only, with normal LVGL dispatch paused and no competing LCD
 * writer. Caller owns eligibility, first-frame drain, input and event replay.
 * SDK calls may block internally: this is a logical deadline, not an I/O watchdog. */
bool wf_aod_probe_run(wf_aod_probe_t *, const wf_aod_probe_io_t *, void *, unsigned seconds);
/* Retry only the original borrowed NORMAL/brightness state after a failed restore. */
bool wf_aod_probe_restore(wf_aod_probe_t *, const wf_aod_probe_io_t *, void *);
/* Retain exclusive LCD ownership until restoration is confirmed. Service
 * events accumulate for replay; only a fresh explicit cancellation retries I/O. */
bool wf_aod_probe_await_restore(wf_aod_probe_t *, const wf_aod_probe_io_t *, void *);
#endif
