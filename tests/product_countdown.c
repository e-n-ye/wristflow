#include "product_countdown.h"
#include "product_pm.h"
#include <assert.h>
#include <stdio.h>
static wristflow_countdown_t read_task(const wristflow_countdown_port_t *p)
{ wristflow_countdown_t task; p->read(p->context, &task); return task; }
static bool command(const wristflow_countdown_port_t *p, wristflow_countdown_command_t op, unsigned sec)
{ return p->command(p->context, op, sec); }
int main(void)
{
    const wristflow_countdown_port_t *p=wristflow_product_countdown_start();
    assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_IDLE && !test_countdown_wait());
    assert(!command(p, WF_COUNTDOWN_START, 0));
    assert(!command(p, WF_COUNTDOWN_START, 86400));
    assert(command(p, WF_COUNTDOWN_START, 1));
    test_countdown_advance(1001, false);
    assert(command(p, WF_COUNTDOWN_CANCEL, 0)); /* Deadline passed, callback selected late. */
    test_countdown_selected_callback();
    assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_IDLE && !test_countdown_sends());
    assert(command(p, WF_COUNTDOWN_START, 3));
    test_countdown_advance(3001, false);
    /* Getter alone must not drive expiry or fake a background transition. */
    assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_RUNNING && !test_countdown_sends());
    test_countdown_selected_callback();
    assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_EXPIRED && read_task(p).expiry_pending);
    assert(test_countdown_events() == WF_EVENT_COUNTDOWN && test_countdown_sends() == 1);
    test_countdown_selected_callback(); assert(test_countdown_sends() == 1);
    assert(command(p, WF_COUNTDOWN_ACK, 0) && !command(p, WF_COUNTDOWN_ACK, 0));
    assert(command(p, WF_COUNTDOWN_REPEAT, 0) && test_countdown_wait() == 3000);
    test_countdown_advance(701, true);
    assert(command(p, WF_COUNTDOWN_PAUSE, 0));
    assert(read_task(p).remaining_ms == 2299 && !test_countdown_wait());
    test_countdown_advance(8000, true);
    test_countdown_selected_callback(); assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_PAUSED);
    assert(command(p, WF_COUNTDOWN_RESUME, 0));
    test_countdown_selected_callback(); /* Old selected callback after Resume: no early expiry. */
    assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_RUNNING && test_countdown_wait() == 2299);
    assert(command(p, WF_COUNTDOWN_CANCEL, 0));
    assert(command(p, WF_COUNTDOWN_START, 60));
    test_countdown_selected_callback(); assert(test_countdown_wait() == 60000);
    test_countdown_advance(60000, true);
    assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_EXPIRED && test_countdown_sends() == 2);
    assert(command(p, WF_COUNTDOWN_REPEAT, 0) && read_task(p).duration_ms == 60000);
    assert(command(p, WF_COUNTDOWN_CANCEL, 0));
    /* Maximum duration, bounded LPTIM horizon, including 32-bit clock wrap. */
    test_countdown_advance(UINT32_MAX - p->now(p->context) - 1000U, false);
    assert(command(p, WF_COUNTDOWN_START, 86399));
    unsigned rounds=0;
    while (read_task(p).phase == WRISTFLOW_COUNTDOWN_RUNNING) {
        uint32_t wait=test_countdown_wait(); assert(wait && wait <= 300000);
        test_countdown_advance(wait, true); ++rounds;
    }
    assert(rounds == 288 && test_countdown_sends() == 3);
    assert(command(p, WF_COUNTDOWN_DISMISS, 0));
    assert(command(p, WF_COUNTDOWN_START, 1));
    test_countdown_advance(1000, true); /* Callback won first, UI Cancel still clears it. */
    assert(command(p, WF_COUNTDOWN_CANCEL, 0));
    test_countdown_selected_callback();
    assert(read_task(p).phase == WRISTFLOW_COUNTDOWN_IDLE && !test_countdown_wait());
    /* Mailbox coalescing/stale bits do not carry business state. */
    assert(test_countdown_events() == WF_EVENT_COUNTDOWN && !read_task(p).expiry_pending);
    puts("Product countdown: independent expiry, bounded waits, stale callbacks, pause/repeat/wrap passed");
    return 0;
}
