#ifndef WRISTFLOW_AW32001_MANUAL_H
#define WRISTFLOW_AW32001_MANUAL_H

#include "aw32001_inhibit.h"

/* Supervised short experiment only; no automatic restart or temperature claim. */
enum { WF_CHARGE_MAX_SECONDS = 60, WF_CHARGE_ENTRY_MIN_MV = 3500,
       WF_CHARGE_ENTRY_MAX_MV = 4100 };
typedef enum { WF_CHARGE_IDLE, WF_CHARGE_ENABLED, WF_CHARGE_STOPPED,
               WF_CHARGE_UNCONFIRMED } wristflow_charge_state_t;
typedef enum {
    WF_CHARGE_NONE, WF_CHARGE_MANUAL, WF_CHARGE_TIMEOUT, WF_CHARGE_COMPLETE,
    WF_CHARGE_IO, WF_CHARGE_PROFILE, WF_CHARGE_INPUT, WF_CHARGE_FAULT,
    WF_CHARGE_VOLTAGE, WF_CHARGE_REGULATION
} wristflow_charge_reason_t;
typedef struct {
    wristflow_aw32001_io_t io;
    uint32_t (*now_ms)(void *context);
    bool (*cancelled)(void *context);
    bool (*voltage)(void *context, uint16_t *millivolts);
} wristflow_charge_port_t;
typedef struct {
    wristflow_charge_state_t state;
    wristflow_charge_reason_t reason;
    wristflow_aw32001_error_t cause_error, stop_error;
    uint8_t cause_reg, stop_reg;
    uint32_t deadline_ms, observed_ms;
    uint16_t millivolts;
    uint8_t power, timer, current, voltage, individual, status, fault, id;
    bool sample_valid;
} wristflow_charge_session_t;

/* One worker owns the session and all I/O. Deadline starts at command acceptance,
 * includes preparation, and is checked around every transfer. It is a software
 * cutoff request, not a hard physical deadline for an unresponsive bus. */
bool wristflow_charge_begin(wristflow_charge_session_t *, const wristflow_charge_port_t *,
                           unsigned seconds, uint32_t accepted_ms);
void wristflow_charge_poll(wristflow_charge_session_t *, const wristflow_charge_port_t *);
void wristflow_charge_stop(wristflow_charge_session_t *, const wristflow_charge_port_t *,
                          wristflow_charge_reason_t);

#endif
