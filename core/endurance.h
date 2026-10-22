#ifndef WRISTFLOW_ENDURANCE_H
#define WRISTFLOW_ENDURANCE_H
#include <stdbool.h>
#include <stdint.h>
#include "power_state.h"

typedef struct {
    bool enabled, usb_absent, holding, blocked;
    uint32_t key_id, usb_insertions;
    uint16_t progress;
} wristflow_endurance_t;
void wristflow_endurance_init(wristflow_endurance_t *, bool enabled, const wristflow_input_observation_t *);
/* Unknown PG never invents a removal. A confirmed absence arms the next insertion. */
bool wristflow_endurance_usb(wristflow_endurance_t *, const wristflow_input_observation_t *);
/* Hardware elapsed time, independent of LVGL timers. A release cancels immediately. */
bool wristflow_endurance_hold(wristflow_endurance_t *, bool down, uint32_t key_id, uint32_t elapsed_ms);
void wristflow_endurance_block(wristflow_endurance_t *);
#endif
