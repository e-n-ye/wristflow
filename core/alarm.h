#ifndef WRISTFLOW_ALARM_H
#define WRISTFLOW_ALARM_H

#include <stdbool.h>
#include <stdint.h>

#define WRISTFLOW_ALARM_CAPACITY 5U
#define WRISTFLOW_ALARM_DAILY 0x7fU
#define WRISTFLOW_ALARM_WORKDAYS 0x1fU
#define WRISTFLOW_ALARM_WEEKEND 0x60U

/* Bit 0 is Monday, bit 6 Sunday; zero means once. This configuration model
 * has one caller/owner, no clock, scheduler, storage, LVGL or heap allocation.
 * A scheduler must resolve a once alarm to a date separately. */
typedef struct {
    uint32_t id;
    uint8_t hour, minute, weekdays;
    bool enabled;
} wristflow_alarm_t;

typedef struct {
    wristflow_alarm_t entries[WRISTFLOW_ALARM_CAPACITY];
    uint32_t next_id;
    uint8_t count;
} wristflow_alarms_t;

void wristflow_alarms_init(wristflow_alarms_t *alarms);
bool wristflow_alarm_valid(const wristflow_alarm_t *alarm);
const wristflow_alarm_t *wristflow_alarm_find(const wristflow_alarms_t *alarms, uint32_t id);
/* Creation order is retained. IDs are never recycled after deletion; zero is
 * invalid and ID exhaustion rejects creation without changing the model. */
bool wristflow_alarm_add(wristflow_alarms_t *alarms, const wristflow_alarm_t *draft, uint32_t *id);
bool wristflow_alarm_edit(wristflow_alarms_t *alarms, uint32_t id, const wristflow_alarm_t *draft);
bool wristflow_alarm_enable(wristflow_alarms_t *alarms, uint32_t id, bool enabled);
bool wristflow_alarm_remove(wristflow_alarms_t *alarms, uint32_t id);

#endif
