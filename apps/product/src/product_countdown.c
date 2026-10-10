#include "product_countdown.h"
#include "product_pm.h"

#if RT_TICK_PER_SECOND != 1000
#error "The locked SiFli PM compensation and countdown clock require 1000 Hz"
#endif

/* Bound the SDK's timeout * LPTIM frequency multiplication to uint32_t.
 * 300000 ticks * 8192 Hz = 2457600000; no LVGL or periodic UI polling. */
#define MAX_WAIT_MS 300000U
static struct rt_mutex lock;
static struct rt_timer deadline;
static wristflow_countdown_t task;
static bool ready;

static uint32_t monotonic(void *context)
{ (void)context; return (uint32_t)rt_tick_get(); }

/* All timer combinations and state access share the same thread mutex.
 * stop cannot retract a callback already selected by the RT timer thread. */
static void schedule(uint32_t now)
{
    rt_timer_stop(&deadline);
    if (task.phase != WRISTFLOW_COUNTDOWN_RUNNING) return;
    rt_tick_t wait = wristflow_countdown_remaining(&task, now);
    if (wait > MAX_WAIT_MS) wait = MAX_WAIT_MS;
    if (!wait) wait = 1;
    RT_ASSERT(rt_timer_control(&deadline, RT_TIMER_CTRL_SET_TIME, &wait) == RT_EOK);
    RT_ASSERT(rt_timer_start(&deadline) == RT_EOK);
}

static void elapsed(void *context)
{
    (void)context;
    RT_ASSERT(rt_mutex_take(&lock, RT_WAITING_FOREVER) == RT_EOK);
    uint32_t now = monotonic(NULL);
    /* Reconcile the CURRENT state/deadline, even for a selected old callback.
     * An early old callback re-arms; a late one may deliver current expiry. */
    bool expired = wristflow_countdown_update(&task, now);
    schedule(now);
    RT_ASSERT(rt_mutex_release(&lock) == RT_EOK);
    if (expired) wristflow_product_event_send(WF_EVENT_COUNTDOWN);
}

static bool command(void *context, wristflow_countdown_command_t operation, uint32_t seconds)
{
    (void)context;
    RT_ASSERT(rt_mutex_take(&lock, RT_WAITING_FOREVER) == RT_EOK);
    uint32_t now = monotonic(NULL);
    bool expired = wristflow_countdown_update(&task, now), accepted = false;
    switch (operation) {
    case WF_COUNTDOWN_START: accepted = wristflow_countdown_start(&task, seconds, now); break;
    case WF_COUNTDOWN_PAUSE: accepted = wristflow_countdown_pause(&task, now); break;
    case WF_COUNTDOWN_RESUME: accepted = wristflow_countdown_resume(&task, now); break;
    case WF_COUNTDOWN_CANCEL:
        /* Explicit immediate cancellation wins even if the deadline callback
         * just ran but the running page's click has not yet been dispatched. */
        wristflow_countdown_cancel(&task); accepted = true; break;
    case WF_COUNTDOWN_REPEAT: accepted = wristflow_countdown_repeat(&task, now); break;
    case WF_COUNTDOWN_ACK: accepted = wristflow_countdown_take_expiry(&task); break;
    case WF_COUNTDOWN_DISMISS:
        if (task.phase == WRISTFLOW_COUNTDOWN_EXPIRED) {
            wristflow_countdown_cancel(&task); accepted = true;
        }
        break;
    }
    schedule(now);
    bool pending = task.expiry_pending;
    RT_ASSERT(rt_mutex_release(&lock) == RT_EOK);
    if (expired && pending) wristflow_product_event_send(WF_EVENT_COUNTDOWN);
    return accepted;
}

static void read_task(void *context, wristflow_countdown_t *copy)
{
    (void)context;
    RT_ASSERT(rt_mutex_take(&lock, RT_WAITING_FOREVER) == RT_EOK);
    *copy = task;
    RT_ASSERT(rt_mutex_release(&lock) == RT_EOK);
}

const wristflow_countdown_port_t *wristflow_product_countdown_start(void)
{
    static const wristflow_countdown_port_t port = {
        .command = command, .read = read_task, .now = monotonic
    };
    if (!ready) {
        RT_ASSERT(rt_mutex_init(&lock, "wf_count", RT_IPC_FLAG_PRIO) == RT_EOK);
        wristflow_countdown_init(&task);
        rt_timer_init(&deadline, "wf_dead", elapsed, NULL, 1,
                      RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);
        ready = true;
    }
    return &port;
}
