#include "alarm.h"
#include <string.h>

void wristflow_alarms_init(wristflow_alarms_t *alarms)
{
    if (!alarms) return;
    memset(alarms, 0, sizeof *alarms);
    alarms->next_id = 1;
}

bool wristflow_alarm_valid(const wristflow_alarm_t *alarm)
{
    return alarm && alarm->hour < 24 && alarm->minute < 60 &&
        (alarm->weekdays & ~WRISTFLOW_ALARM_DAILY) == 0;
}

static int index_of(const wristflow_alarms_t *alarms, uint32_t id)
{
    if (!alarms || !id || alarms->count > WRISTFLOW_ALARM_CAPACITY) return -1;
    for (unsigned i = 0; i < alarms->count; ++i)
        if (alarms->entries[i].id == id) return (int)i;
    return -1;
}

const wristflow_alarm_t *wristflow_alarm_find(const wristflow_alarms_t *alarms, uint32_t id)
{
    int index = index_of(alarms, id);
    return index < 0 ? NULL : &alarms->entries[index];
}

bool wristflow_alarm_add(wristflow_alarms_t *alarms, const wristflow_alarm_t *draft, uint32_t *id)
{
    if (!alarms || !wristflow_alarm_valid(draft) ||
        alarms->count >= WRISTFLOW_ALARM_CAPACITY || !alarms->next_id ||
        wristflow_alarm_find(alarms, alarms->next_id)) return false;
    wristflow_alarm_t created = *draft;
    created.id = alarms->next_id++;
    created.enabled = true;
    alarms->entries[alarms->count++] = created;
    if (id) *id = created.id;
    return true;
}

bool wristflow_alarm_edit(wristflow_alarms_t *alarms, uint32_t id, const wristflow_alarm_t *draft)
{
    int index = index_of(alarms, id);
    if (index < 0 || !wristflow_alarm_valid(draft)) return false;
    wristflow_alarm_t changed = *draft;
    changed.id = id;
    changed.enabled = alarms->entries[index].enabled;
    alarms->entries[index] = changed;
    return true;
}

bool wristflow_alarm_enable(wristflow_alarms_t *alarms, uint32_t id, bool enabled)
{
    int index = index_of(alarms, id);
    if (index < 0) return false;
    alarms->entries[index].enabled = enabled;
    return true;
}

bool wristflow_alarm_remove(wristflow_alarms_t *alarms, uint32_t id)
{
    int index = index_of(alarms, id);
    if (index < 0) return false;
    for (unsigned i = (unsigned)index; i + 1 < alarms->count; ++i)
        alarms->entries[i] = alarms->entries[i + 1];
    memset(&alarms->entries[--alarms->count], 0, sizeof alarms->entries[0]);
    return true;
}
