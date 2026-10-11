#include "reminder_queue.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    wristflow_reminder_queue_t q={0};
    wristflow_reminder_entry_t cd={{WF_REMINDER_COUNTDOWN,0},1000,0};
    assert(wristflow_reminder_enqueue(&q,cd));
    for(unsigned id=5;id;--id) assert(wristflow_reminder_enqueue(&q,(wristflow_reminder_entry_t){{WF_REMINDER_ALARM,id},1000,id}));
    assert(q.count==6 && wristflow_reminder_front(&q)->key.id==1);
    assert(wristflow_reminder_enqueue(&q,cd) && q.count==6 && !q.overflow);
    assert(!wristflow_reminder_enqueue(&q,(wristflow_reminder_entry_t){{WF_REMINDER_ALARM,6},999,6}) && q.overflow==1);
    q.active=true;
    assert(wristflow_reminder_remove(&q,(wristflow_reminder_key_t){WF_REMINDER_ALARM,3}));
    assert(wristflow_reminder_enqueue(&q,(wristflow_reminder_entry_t){{WF_REMINDER_ALARM,6},1,6}));
    assert(q.entries[0].key.id==1 && q.entries[1].key.id==6); /* No preemption. */
    assert(wristflow_reminder_remove(&q,q.entries[0].key));
    assert(q.entries[0].key.id==6 && !q.active);
    assert(wristflow_reminder_enqueue(&q,(wristflow_reminder_entry_t){{WF_REMINDER_ALARM,7},0,7}));
    assert(q.entries[0].key.id==7); /* Batch removal must not pin an unseen next item. */
    while(q.count) assert(wristflow_reminder_remove(&q,q.entries[0].key));
    assert(!q.active && !wristflow_reminder_front(&q));
    /* Extended timestamps preserve order across a 32-bit wrap. */
    cd.due_ms=UINT64_C(0x100000001); assert(wristflow_reminder_enqueue(&q,cd));
    assert(wristflow_reminder_enqueue(&q,(wristflow_reminder_entry_t){{WF_REMINDER_ALARM,9},UINT32_MAX,9}));
    assert(q.entries[0].key.id==9);
    puts("reminder FIFO/ties, capacity/coalescing, no preemption and clock wrap passed");
}
