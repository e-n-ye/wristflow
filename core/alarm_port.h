#ifndef WRISTFLOW_ALARM_PORT_H
#define WRISTFLOW_ALARM_PORT_H
#include "alarm_schedule.h"
#include "component_layout.h"
typedef struct {
    wristflow_alarms_t alarms;
    wristflow_alarm_pending_t pending[WRISTFLOW_ALARM_CAPACITY];
    bool time_valid, storage_failed;
} wristflow_alarm_snapshot_t;
/* UI-thread command copies a draft. Product commits only after background
 * storage verification; read is a synchronized copy, never an expiry driver. */
typedef struct {
    void *context;
    uint32_t (*request)(void *, const wristflow_alarm_command_t *);
    wristflow_save_state_t (*status)(void *, uint32_t request);
    void (*read)(void *, wristflow_alarm_snapshot_t *);
    bool (*ack)(void *, uint32_t id, uint32_t sequence);
    /* Explicit reconciliation before collecting a batch across producers.
     * Independent RT deadlines remain authoritative while LVGL is paused. */
    void (*sync)(void *);
} wristflow_alarm_port_t;
#endif
