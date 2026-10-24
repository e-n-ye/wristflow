#ifndef WRISTFLOW_LTR303_DIAG_H
#define WRISTFLOW_LTR303_DIAG_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *context;
    bool (*read)(void *, uint8_t reg, uint8_t *value);
    bool (*write)(void *, uint8_t reg, uint8_t value);
    uint32_t (*now_ms)(void *);
    bool (*cancelled)(void *);
} wf_ltr303_io_t;
typedef enum { WF_LIGHT_IDLE, WF_LIGHT_RUNNING, WF_LIGHT_DONE, WF_LIGHT_FAULT } wf_light_phase_t;
typedef enum {
    WF_LIGHT_NO_END, WF_LIGHT_DEADLINE, WF_LIGHT_CANCELLED, WF_LIGHT_IO_FAILURE,
    WF_LIGHT_ID_MISMATCH, WF_LIGHT_IN_USE, WF_LIGHT_CONFIG_CHANGED
} wf_light_end_t;
typedef struct {
    wf_light_phase_t phase;
    wf_light_end_t ended;
    uint32_t session, started_ms, duration_ms, sampled_ms, samples, not_ready, io_errors;
    uint16_t ch0, ch1;
    uint8_t part, manufacturer, before_ctrl, before_rate, entered_ctrl, entered_rate;
    uint8_t after_ctrl, after_rate, status;
    bool identity_valid, before_valid, entered_valid, after_valid;
    bool owned, restored, sample_valid, saturated;
} wf_ltr303_snapshot_t;
typedef struct {
    wf_ltr303_io_t io;
    wf_ltr303_snapshot_t snapshot;
    uint32_t due_ms;
    bool changed, data_locked, continuous;
} wf_ltr303_t;

/* Single sensor worker only. Explicit bounded run; no reset, IRQ or own timer.
 * Fixed 1x / 100ms integration / 500ms repeat, observations about once a second.
 * Values are raw channels, never labelled lux without a verified conversion. */
void wf_ltr303_init(wf_ltr303_t *, wf_ltr303_io_t);
bool wf_ltr303_begin(wf_ltr303_t *, uint32_t started_ms, uint32_t duration_ms);
/* Same registers/ownership/restore path; product worker stops on policy exit.
 * No rolling diagnostic sessions or implicit background restart. */
bool wf_ltr303_begin_continuous(wf_ltr303_t *, uint32_t started_ms);
void wf_ltr303_poll(wf_ltr303_t *);
bool wf_ltr303_stop(wf_ltr303_t *, wf_light_end_t);
void wf_ltr303_bus_fault(wf_ltr303_t *);
/* -1 means no autonomous retry, including unresolved cleanup. */
int32_t wf_ltr303_wait_ms(const wf_ltr303_t *, uint32_t now_ms);
#endif
