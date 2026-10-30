#include "product_settings_store.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>

enum { MAX_ATTEMPTS = 3, DEBOUNCE_MS = 1500, RETRY_MS = 5000 };
typedef struct {
    uint32_t id, due;
    unsigned attempts;
    wristflow_save_state_t state;
} save_job_t;
static struct rt_mutex lock;
static struct rt_event changed;
static bool ready, writing_settings, saved_valid;
static wristflow_settings_t requested, saved;
static wristflow_layout_t requested_layout;
static wristflow_layout_store_t layout_store;
static wristflow_settings_store_io_t io;
static save_job_t preferences, layout;

static uint32_t next_id(uint32_t id) { return ++id ? id : 1; }
static int32_t remaining(const save_job_t *job, uint32_t now)
{
    if (job->state != WRISTFLOW_SAVE_PENDING) return -1;
    int32_t delta = (int32_t)(job->due - now);
    return delta > 0 ? delta : 0;
}
static void new_job(save_job_t *job, unsigned delay_ms)
{
    *job = (save_job_t){ .id = next_id(job->id), .state = WRISTFLOW_SAVE_PENDING,
        .due = rt_tick_get() + rt_tick_from_millisecond(delay_ms) };
}
static void finish(save_job_t *job, uint32_t id, bool success)
{
    if (job->id != id) return; /* A late old write cannot complete a new request. */
    if (success) job->state = WRISTFLOW_SAVE_DONE;
    else if (job->attempts >= MAX_ATTEMPTS) job->state = WRISTFLOW_SAVE_FAILED;
    else job->due = rt_tick_get() + rt_tick_from_millisecond(RETRY_MS);
}
static void hold(void)
{
#ifdef RT_USING_PM
    rt_pm_request(PM_SLEEP_MODE_IDLE);
#endif
}
static void release(void)
{
#ifdef RT_USING_PM
    rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
}

static void process_layout(void)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    if (remaining(&layout, rt_tick_get()) != 0) { rt_mutex_release(&lock); return; }
    wristflow_layout_t value = requested_layout;
    uint32_t id = layout.id;
    ++layout.attempts;
    rt_mutex_release(&lock);
    hold();
    bool success = wristflow_layout_commit(&layout_store, &value, io.read_layout, io.write_layout, io.context);
    release();
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    finish(&layout, id, success);
    rt_mutex_release(&lock);
    rt_kprintf("[store] layout request=%lu verified=%u generation=%lu\n",
        (unsigned long)id, success, (unsigned long)layout_store.generation);
}

static void process_settings(void)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    if (remaining(&preferences, rt_tick_get()) != 0) { rt_mutex_release(&lock); return; }
    if (saved_valid && wristflow_settings_equal(&requested, &saved)) {
        preferences.state = WRISTFLOW_SAVE_DONE;
        rt_mutex_release(&lock);
        return;
    }
    wristflow_settings_t value = requested;
    uint32_t id = preferences.id;
    ++preferences.attempts;
    writing_settings = true;
    rt_mutex_release(&lock);
    uint8_t record[WRISTFLOW_SETTINGS_BYTES], readback[WRISTFLOW_SETTINGS_BYTES];
    wristflow_settings_t decoded;
    hold();
    bool success = wristflow_settings_encode(&value, record) &&
        io.write_settings(io.context, record) && io.read_settings(io.context, readback) &&
        !memcmp(record, readback, sizeof record) &&
        wristflow_settings_decode(&decoded, readback, sizeof readback) &&
        wristflow_settings_equal(&decoded, &value);
    release();
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    writing_settings = false;
    /* An error can follow a partial write. Do not reuse the old saved shadow as
     * proof of persistence, even if this request was superseded during I/O. */
    saved_valid = success;
    if (success) saved = value;
    finish(&preferences, id, success);
    rt_mutex_release(&lock);
    rt_kprintf("[store] preferences request=%lu verified=%u\n", (unsigned long)id, success);
}

static rt_int32_t wait_ticks(void)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    uint32_t now = rt_tick_get();
    int32_t a = remaining(&preferences, now), b = remaining(&layout, now);
    rt_mutex_release(&lock);
    return a < 0 ? b : b < 0 ? a : a < b ? a : b;
}
static void process_record(void)
{
    rt_mutex_take(&lock,RT_WAITING_FOREVER);
    bool busy=preferences.state==WRISTFLOW_SAVE_PENDING || layout.state==WRISTFLOW_SAVE_PENDING;
    rt_mutex_release(&lock);
    if(!busy && io.process_record)io.process_record(io.context);
}
static void worker(void *context)
{
    (void)context;
    for (;;) {
        process_layout();
        process_settings(); /* Recheck time after the potentially slow layout write. */
        process_record();
        rt_uint32_t bits;
        rt_event_recv(&changed, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, wait_ticks(), &bits);
    }
}
bool wristflow_product_settings_store_notify(void)
{ return ready && rt_event_send(&changed,1)==RT_EOK; }

uint32_t wristflow_product_settings_request(const wristflow_settings_t *value, void *context)
{
    (void)context;
    if (!ready || !wristflow_settings_valid(value)) return 0;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool same = wristflow_settings_equal(value, &requested);
    if (preferences.id && same && preferences.state != WRISTFLOW_SAVE_FAILED) {
        uint32_t id = preferences.id;
        rt_mutex_release(&lock);
        return id;
    }
    requested = *value;
    new_job(&preferences, same ? 0 : DEBOUNCE_MS);
    if (!writing_settings && saved_valid && wristflow_settings_equal(value, &saved))
        preferences.state = WRISTFLOW_SAVE_DONE;
    uint32_t id = preferences.id;
    if (rt_event_send(&changed, 1) != RT_EOK) preferences.state = WRISTFLOW_SAVE_FAILED;
    rt_mutex_release(&lock);
    return id;
}

uint32_t wristflow_product_layout_request(const wristflow_layout_t *value, void *context)
{
    (void)context;
    if (!ready || !wristflow_layout_valid(value)) return 0;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    requested_layout = *value;
    new_job(&layout, 0); /* Editor commits and explicit retries are immediate. */
    uint32_t id = layout.id;
    if (rt_event_send(&changed, 1) != RT_EOK) layout.state = WRISTFLOW_SAVE_FAILED;
    rt_mutex_release(&lock);
    return id;
}
static wristflow_save_state_t status(save_job_t *job, uint32_t id)
{
    if (!ready || !id) return WRISTFLOW_SAVE_FAILED;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    wristflow_save_state_t value = job->id == id ? job->state : WRISTFLOW_SAVE_FAILED;
    rt_mutex_release(&lock);
    return value;
}
wristflow_save_state_t wristflow_product_settings_status(uint32_t id, void *context)
{ (void)context; return status(&preferences, id); }
wristflow_save_state_t wristflow_product_layout_status(uint32_t id, void *context)
{ (void)context; return status(&layout, id); }

bool wristflow_product_settings_store_start(const wristflow_settings_t *boot, bool restored,
    const wristflow_layout_store_t *publication, const wristflow_settings_store_io_t *backend)
{
    if (ready) return true;
    if (!wristflow_settings_valid(boot) || !publication || !backend || !backend->read_settings ||
        !backend->write_settings || !backend->read_layout || !backend->write_layout) return false;
    if (rt_mutex_init(&lock, "wf_cfg", RT_IPC_FLAG_PRIO) != RT_EOK) return false;
    if (rt_event_init(&changed, "wf_save", RT_IPC_FLAG_PRIO) != RT_EOK) {
        rt_mutex_detach(&lock); return false;
    }
    io = *backend;
    layout_store = *publication;
    requested = saved = *boot;
    saved_valid = restored;
    rt_thread_t thread = rt_thread_create("wf_store", worker, RT_NULL, 6144, 25, 10);
    if (!thread || rt_thread_startup(thread) != RT_EOK) {
        if (thread) rt_thread_delete(thread);
        rt_event_detach(&changed);
        rt_mutex_detach(&lock);
        return false;
    }
    ready = true;
    return true;
}
