#ifndef WRISTFLOW_PRODUCT_MOTOR_H
#define WRISTFLOW_PRODUCT_MOTOR_H
#include <stdbool.h>
#include <stdint.h>

/* Initial diagnostic policy, not a measured motor/driver electrical limit. */
#define WRISTFLOW_MOTOR_MAX_MS 250u
typedef struct {
    void *context;
    bool (*initialize_off)(void *context);
    /* OFF succeeds only after GPIO LOW and backend resource release checks. */
    bool (*set_enabled)(void *context, bool enabled);
    /* May enable a supply with the gate LOW; caller rechecks stop/deadline. */
    bool (*prepare)(void *context);
} wristflow_motor_io_t;

typedef enum {
    WRISTFLOW_MOTOR_UNAVAILABLE, WRISTFLOW_MOTOR_OFF, WRISTFLOW_MOTOR_QUEUED,
    WRISTFLOW_MOTOR_ACTIVE, WRISTFLOW_MOTOR_STOPPING, WRISTFLOW_MOTOR_FAULT,
    WRISTFLOW_MOTOR_GAP
} wristflow_motor_state_t;
typedef enum {
    WRISTFLOW_MOTOR_NONE, WRISTFLOW_MOTOR_EXPIRED, WRISTFLOW_MOTOR_CANCELLED,
    WRISTFLOW_MOTOR_ON_ERROR, WRISTFLOW_MOTOR_OFF_ERROR,
    WRISTFLOW_MOTOR_INIT_ERROR, WRISTFLOW_MOTOR_RESOURCE_ERROR, WRISTFLOW_MOTOR_WAKE_ERROR,
    WRISTFLOW_MOTOR_PREPARE_ERROR
} wristflow_motor_reason_t;
typedef struct {
    wristflow_motor_state_t state;
    wristflow_motor_reason_t reason;
    uint32_t request_id, requested_ms, remaining_ms;
    /* Combined backend OFF check, not an independent GPIO/current measurement. */
    bool off_latched, busy, stop_pending;
    bool preview;
    uint8_t segment, segments;
} wristflow_motor_snapshot_t;

/* Product main calls start before consumers; concurrent constructors are unsupported.
 * Startup owns initial OFF; after startup only the worker calls the backend.
 * Foreground APIs copy scalar state under a short IRQ lock and never do I/O.
 * Concurrent pulses are rejected; stop cancels queued/active work without extending it. */
bool wristflow_product_motor_start(const wristflow_motor_io_t *io);
uint32_t wristflow_product_motor_pulse(uint32_t milliseconds);
/* One settings preview, standard100ms or enhanced100/150gap/100ms. Same
 * exclusive worker; no queue/replacement. Segment deadlines never extend. */
uint32_t wristflow_product_motor_preview(bool enhanced);
/* Only cancels the matching preview; stale handles cannot stop CLI pulses. */
bool wristflow_product_motor_cancel_preview(uint32_t request);
bool wristflow_product_motor_stop(void);
wristflow_motor_snapshot_t wristflow_product_motor_snapshot(void);
const wristflow_motor_io_t *wristflow_product_motor_board_io(void);
#endif
