#include "alarm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void same(const wristflow_alarms_t *a, const wristflow_alarms_t *b)
{
    assert(a->count == b->count && a->next_id == b->next_id);
    for (unsigned i = 0; i < WRISTFLOW_ALARM_CAPACITY; ++i) {
        assert(a->entries[i].id == b->entries[i].id);
        assert(a->entries[i].hour == b->entries[i].hour);
        assert(a->entries[i].minute == b->entries[i].minute);
        assert(a->entries[i].weekdays == b->entries[i].weekdays);
        assert(a->entries[i].enabled == b->entries[i].enabled);
    }
}

int main(void)
{
    wristflow_alarms_t bank; wristflow_alarms_init(&bank);
    assert(bank.count == 0 && !wristflow_alarm_find(&bank, 0));
    wristflow_alarm_t draft = {.hour=0, .minute=0, .weekdays=0, .enabled=false};
    uint32_t ids[5];
    for (unsigned i = 0; i < 5; ++i) {
        assert(wristflow_alarm_add(&bank, &draft, &ids[i]));
        assert(ids[i] == i + 1 && wristflow_alarm_find(&bank, ids[i])->enabled);
    }
    wristflow_alarms_t before = bank;
    assert(!wristflow_alarm_add(&bank, &draft, NULL)); same(&bank, &before);
    assert(wristflow_alarm_enable(&bank, ids[1], false));
    draft = (wristflow_alarm_t){.id=999, .hour=23, .minute=59,
        .weekdays=WRISTFLOW_ALARM_WEEKEND, .enabled=true};
    assert(wristflow_alarm_edit(&bank, ids[1], &draft));
    const wristflow_alarm_t *edited = wristflow_alarm_find(&bank, ids[1]);
    assert(edited->id == ids[1] && !edited->enabled && edited->hour == 23 && edited->minute == 59);
    assert(edited->weekdays == 0x60 && WRISTFLOW_ALARM_WORKDAYS == 0x1f);
    before = bank;
    const wristflow_alarm_t invalid[] = {{.hour=24}, {.minute=60}, {.weekdays=0x80}};
    for (unsigned i = 0; i < 3; ++i) {
        assert(!wristflow_alarm_valid(&invalid[i]));
        assert(!wristflow_alarm_edit(&bank, ids[0], &invalid[i])); same(&bank, &before);
    }
    assert(!wristflow_alarm_edit(&bank, 0, &draft));
    assert(!wristflow_alarm_enable(&bank, 99, true));
    assert(!wristflow_alarm_remove(&bank, 99)); same(&bank, &before);
    assert(wristflow_alarm_remove(&bank, ids[1]));
    assert(bank.count == 4 && bank.entries[1].id == ids[2]);
    assert(!wristflow_alarm_find(&bank, ids[1]));
    uint32_t new_id;
    assert(wristflow_alarm_add(&bank, &draft, &new_id) && new_id == 6);
    assert(wristflow_alarm_find(&bank, ids[2])->hour == 0);
    for (unsigned i = 0; i < 5; ++i) assert(wristflow_alarm_remove(&bank, bank.entries[0].id));
    assert(bank.count == 0);
    for (unsigned mask = 0; mask <= WRISTFLOW_ALARM_DAILY; ++mask) {
        draft.weekdays = (uint8_t)mask;
        assert(wristflow_alarm_add(&bank, &draft, &new_id));
        assert(wristflow_alarm_find(&bank, new_id)->weekdays == mask);
        assert(wristflow_alarm_remove(&bank, new_id));
    }
    bank.next_id = UINT32_MAX;
    assert(wristflow_alarm_add(&bank, &draft, &new_id) && new_id == UINT32_MAX);
    before = bank;
    assert(!wristflow_alarm_add(&bank, &draft, NULL)); same(&bank, &before);
    assert(!wristflow_alarm_add(NULL, &draft, NULL));
    assert(!wristflow_alarm_valid(NULL));
    puts("alarm model: capacity, boundaries, masks, stable IDs and enabled-state preservation passed");
    return 0;
}
