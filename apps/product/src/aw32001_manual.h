#ifndef WRISTFLOW_AW32001_MANUAL_H
#define WRISTFLOW_AW32001_MANUAL_H

#include "aw32001_inhibit.h"

/* Supervised validation profiles; no automatic restart or cell-temperature input. */
enum { WF_CHARGE_MAX_SECONDS = 60, WF_CHARGE_ENTRY_MIN_MV = 3500,
       WF_CHARGE_ENTRY_MAX_MV = 4100,
       WF_CHARGE_AUTO_MA = 200,
       WF_CHARGE_AUTO_CURRENT_CODE = WF_CHARGE_AUTO_MA / 8 - 1,
       WF_CHARGE_INPUT_SETTLE_MS = 150,
       WF_CHARGE_AUTO_SECONDS = 10 * 60 * 60,
       WF_CHARGE_AUTO_MIN_MV = 3000, WF_CHARGE_AUTO_STOP_MV = 4190 };
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
    void (*wait_ms)(void *context, unsigned milliseconds);
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
    bool automatic;
    uint8_t termination;
    uint8_t entry_fault;
    bool input_qualified;
} wristflow_charge_session_t;

/* One worker owns the session and all I/O. Deadline starts at command acceptance,
 * includes preparation, and is checked around every transfer. It is a software
 * cutoff request, not a hard physical deadline for an unresponsive bus. */
bool wristflow_charge_begin(wristflow_charge_session_t *, const wristflow_charge_port_t *,
                           unsigned seconds, uint32_t accepted_ms);
/* USB automatic cycle: nominal 200mA, fixed 3mA termination, 8h chip timer and
 * 10h software backstop. Field validation requires cell20..45C externally;
 * no cell-temperature interlock exists. Voltage limits are policy requests. */
bool wristflow_charge_begin_auto(wristflow_charge_session_t *, const wristflow_charge_port_t *,
                                uint32_t accepted_ms);
void wristflow_charge_poll(wristflow_charge_session_t *, const wristflow_charge_port_t *);
void wristflow_charge_stop(wristflow_charge_session_t *, const wristflow_charge_port_t *,
                          wristflow_charge_reason_t);

#endif
