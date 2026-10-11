#include "product_alarm.h"
#include "product_services.h"
#include "product_pm.h"
#include <rtthread.h>
#include <string.h>

#if RT_TICK_PER_SECOND != 1000
#error "The locked RTC/LPTIM compensation requires 1000 Hz"
#endif
static struct rt_mutex lock;
static struct rt_timer deadline;
static struct rt_semaphore storage_work;
static wristflow_alarm_schedule_t state;
static wristflow_alarm_store_t store;
static wristflow_alarm_command_t requested;
static uint32_t request_id;
static uint64_t saved_version, retry_at;
static wristflow_save_state_t save_status;
static bool ready, worker_ready, pending_request, storage_failed, work_queued;

static void wake_store_locked(uint64_t now, bool manual)
{
    if (worker_ready && !work_queued && (pending_request || state.version != saved_version) &&
        (manual || !storage_failed || now - retry_at >= 5000U)) {
        work_queued = true; rt_sem_release(&storage_work);
    }
}

static void schedule_locked(wristflow_alarm_clock_t clock)
{
    rt_timer_stop(&deadline);
    rt_tick_t wait = wristflow_alarm_schedule_wait(&state, clock);
    if (worker_ready && storage_failed && state.version != saved_version) {
        uint64_t elapsed = clock.monotonic_ms - retry_at;
        uint32_t retry = elapsed >= 5000U ? 1U : (uint32_t)(5000U - elapsed);
        if (retry < wait) wait = retry;
    }
    RT_ASSERT(rt_timer_control(&deadline, RT_TIMER_CTRL_SET_TIME, &wait) == RT_EOK);
    RT_ASSERT(rt_timer_start(&deadline) == RT_EOK);
}
static bool update_locked(wristflow_alarm_clock_t *clock)
{
    *clock = wristflow_product_services_time();
    uint64_t before = state.version;
    wristflow_alarm_schedule_update(&state, *clock);
    schedule_locked(*clock);
    return state.version != before;
}
static void reconcile(void *context)
{
    (void)context;
    if (!ready) return;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    wristflow_alarm_clock_t clock;
    bool changed = update_locked(&clock);
    wake_store_locked(clock.monotonic_ms, false);
    rt_mutex_release(&lock);
    if (changed) wristflow_product_event_send(WF_EVENT_ALARM);
}
void wristflow_product_alarm_clock_changed(void) { reconcile(NULL); }

static void deadline_fired(void *context)
{
    (void)context;
    /* Flash publication may hold the model lock. Never block the shared soft
     * timer thread; a short one-shot retry still reconstructs from live state. */
    if (rt_mutex_take(&lock, 0) != RT_EOK) {
        rt_tick_t retry = 20;
        rt_timer_control(&deadline, RT_TIMER_CTRL_SET_TIME, &retry);
        rt_timer_start(&deadline); return;
    }
    wristflow_alarm_clock_t clock;
    bool changed = update_locked(&clock);
    wake_store_locked(clock.monotonic_ms, false);
    rt_mutex_release(&lock);
    if (changed) wristflow_product_event_send(WF_EVENT_ALARM);
}

static uint32_t request(void *context, const wristflow_alarm_command_t *command)
{
    (void)context;
    if (!worker_ready || !command || !wristflow_product_services_storage_ready()) return 0;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    wristflow_alarm_clock_t clock;
    bool changed = update_locked(&clock);
    wristflow_alarm_schedule_t candidate = state;
    uint32_t result = 0;
    if (!pending_request && wristflow_alarm_schedule_command(&candidate, command, clock)) {
        requested = *command; pending_request = true; save_status = WRISTFLOW_SAVE_PENDING;
        if (++request_id == 0) ++request_id;
        result = request_id;
        wake_store_locked(clock.monotonic_ms, true);
    }
    rt_mutex_release(&lock);
    if (changed) wristflow_product_event_send(WF_EVENT_ALARM);
    return result;
}
static wristflow_save_state_t status(void *context, uint32_t id)
{
    (void)context;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    wristflow_save_state_t result = id && id == request_id ? save_status : WRISTFLOW_SAVE_FAILED;
    rt_mutex_release(&lock); return result;
}
static void read_state(void *context, wristflow_alarm_snapshot_t *copy)
{
    (void)context;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    copy->alarms = state.data.alarms;
    memcpy(copy->pending, state.pending, sizeof copy->pending);
    copy->time_valid = state.time_valid; copy->storage_failed = storage_failed;
    rt_mutex_release(&lock);
}
static bool ack(void *context, uint32_t id, uint32_t sequence)
{
    (void)context;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool accepted = wristflow_alarm_schedule_ack(&state, id, sequence);
    wake_store_locked(wristflow_product_services_monotonic(), false);
    rt_mutex_release(&lock); return accepted;
}

/* Preparation runs outside the model lock. A clock/expiry/ACK racing the write
 * invalidates the candidate; no publication marker is written for a stale draft.
 * Only final marker publication serializes with model mutation. */
static void storage_step(void)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    work_queued = false;
    if (!pending_request && saved_version == state.version) { rt_mutex_release(&lock); return; }
    wristflow_alarm_clock_t clock;
    bool changed = update_locked(&clock);
    bool manual = pending_request;
    uint32_t id = request_id;
    uint64_t base = state.version;
    if (!manual && (saved_version == base || (storage_failed && clock.monotonic_ms - retry_at < 5000U))) {
        rt_mutex_release(&lock);
        if (changed) wristflow_product_event_send(WF_EVENT_ALARM);
        return;
    }
    wristflow_alarm_schedule_t candidate = state;
    bool accepted = !manual || wristflow_alarm_schedule_command(&candidate, &requested, clock);
    rt_mutex_release(&lock);
    if (changed) wristflow_product_event_send(WF_EVENT_ALARM);
    wristflow_alarm_prepared_t prepared;
    bool verified = accepted && wristflow_alarm_prepare(&store, &candidate.data,
        wristflow_product_services_alarm_read, wristflow_product_services_alarm_write, NULL, &prepared);
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    changed = update_locked(&clock);
    bool same = state.version == base && (!manual || (pending_request && request_id == id));
    bool replan = false;
    if (manual && verified && same) {
        wristflow_alarm_schedule_t fresh = state;
        if (!wristflow_alarm_schedule_command(&fresh, &requested, clock)) verified = false;
        else replan = memcmp(&fresh.data, &candidate.data, sizeof fresh.data) ||
            memcmp(fresh.next_utc, candidate.next_utc, sizeof fresh.next_utc);
    }
    if (verified && (!same || replan)) {
        wake_store_locked(clock.monotonic_ms, true);
        rt_mutex_release(&lock);
        if (changed) wristflow_product_event_send(WF_EVENT_ALARM);
        return;
    }
    bool success = verified && same && wristflow_alarm_publish(&store, &prepared, wristflow_product_services_alarm_write, NULL);
    storage_failed = !success; retry_at = clock.monotonic_ms;
    if (success) {
        if (manual) { candidate.previous = clock; state = candidate; }
        saved_version = state.version;
        schedule_locked(clock);
    }
    if (manual) { pending_request = false; save_status = success ? WRISTFLOW_SAVE_DONE : WRISTFLOW_SAVE_FAILED; }
    schedule_locked(clock);
    rt_mutex_release(&lock);
    rt_kprintf("[alarm] %s request=%u generation=%u\n", success ? "saved" : "save failed", manual ? id : 0, store.generation);
    wristflow_product_event_send(WF_EVENT_ALARM);
}
static void storage_thread(void *context)
{
    (void)context;
    for (;;) { rt_sem_take(&storage_work, RT_WAITING_FOREVER); storage_step(); }
}
static uint64_t now64(void *context)
{ (void)context; return wristflow_product_services_monotonic(); }
const wristflow_alarm_port_t *wristflow_product_alarm_start(void)
{
    static const wristflow_alarm_port_t port = {.request=request,.status=status,.read=read_state,.ack=ack,.sync=reconcile,.now64=now64};
    if (!ready) {
        rt_mutex_init(&lock, "wf_alarm", RT_IPC_FLAG_PRIO);
        rt_sem_init(&storage_work, "wf_alarm_save", 0, RT_IPC_FLAG_PRIO);
        wristflow_alarm_schedule_init(&state);
        bool restored = wristflow_alarm_restore(&store, &state.data, wristflow_product_services_alarm_read, NULL);
        rt_timer_init(&deadline, "wf_alarm_due", deadline_fired, NULL, 1, RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);
        ready = true;
        reconcile(NULL); /* Boot replan never replays expired dates. */
        if (wristflow_product_services_storage_ready()) {
            rt_thread_t worker = rt_thread_create("wf_alarm_save", storage_thread, NULL, 4096, 25, 10);
            worker_ready = worker && rt_thread_startup(worker) == RT_EOK;
        }
        storage_failed = !worker_ready;
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        wake_store_locked(wristflow_product_services_monotonic(), false);
        rt_mutex_release(&lock);
        rt_kprintf("[alarm] restore=%s count=%u storage=%s generation=%u\n", restored ? "yes" : "empty",
            state.data.alarms.count, worker_ready ? "ready" : "unavailable", store.generation);
    }
    return &port;
}
