#include "product_services.h"
#include "product_alarm.h"
#include "product_settings_store.h"
#include "product_steps.h"
#include "product_pm.h"
#include "steps_store.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <flashdb.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

static struct fdb_kvdb settings_db;
static struct rt_mutex database_lock, rtc_lock, weather_lock;
static bool storage_ready, service_ready;
#ifdef WF_STORAGE_PROBE
#if FDB_WRITE_GRAN != 1 || RT_TICK_PER_SECOND != 1000
#error Storage probe requires the audited FlashDB format and 1ms ticks
#endif
static bool probe_lock(void *context)
{ (void)context; return rt_mutex_take(&database_lock,rt_tick_from_millisecond(100))==RT_EOK; }
static bool probe_unlock(void *context)
{ (void)context; return rt_mutex_release(&database_lock)==RT_EOK; }
static uint32_t probe_now(void *context)
{ (void)context; return (uint32_t)((uint64_t)rt_tick_get()*1000U/RT_TICK_PER_SECOND); }
static int probe_read(void *context, uint32_t off, uint8_t *buf, size_t n)
{
    const struct fal_partition *part=context;
    if (!part || off>part->len || n>part->len-off || n>128) return -1;
    return fal_partition_read(part,off,buf,n);
}
unsigned wristflow_product_services_storage_snapshot(uint8_t *out, size_t size, wf_storage_probe_info_t *info)
{
    /* Read the existing partition pointer; never init/check/iterate FlashDB. */
    if (!service_ready) {
        const wf_storage_source_t unavailable={0};
        return wf_storage_source_copy(&unavailable,NULL,out,size,info);
    }
    const struct fal_partition *part=settings_db.parent.file_mode ? NULL : settings_db.parent.storage.part;
    const wf_storage_source_t source={service_ready,settings_db.parent.init_ok,settings_db.parent.file_mode,
        part!=NULL,part ? part->len : 0,settings_db.parent.sec_size};
    const wf_storage_copy_io_t io={(void *)part,probe_lock,probe_unlock,probe_read,probe_now};
    return wf_storage_source_copy(&source,&io,out,size,info);
}
#endif
static rt_device_t rtc;
static uint32_t rtc_revision, previous_ticks;
static uint64_t extended_ticks;

/* Under rtc_lock. Alarm's five-minute timer also observes ticks when every
 * view is asleep, so the extended clock cannot miss a whole 32-bit cycle. */
static uint64_t monotonic_locked(void)
{
    uint32_t now = rt_tick_get();
    extended_ticks += (uint32_t)(now - previous_ticks); previous_ticks = now;
    return extended_ticks * 1000U / RT_TICK_PER_SECOND;
}
uint64_t wristflow_product_services_monotonic(void)
{
    rt_mutex_take(&rtc_lock, RT_WAITING_FOREVER);
    uint64_t now = monotonic_locked();
    rt_mutex_release(&rtc_lock); return now;
}
wristflow_alarm_clock_t wristflow_product_services_time(void)
{
    wristflow_alarm_clock_t clock = {0};
    rt_mutex_take(&rtc_lock, RT_WAITING_FOREVER);
    struct timeval value = {0};
    clock.valid = rtc && rt_device_control(rtc, RT_DEVICE_CTRL_RTC_GET_TIMEVAL, &value) == RT_EOK &&
        value.tv_sec >= WRISTFLOW_TIME_MIN && value.tv_sec <= WRISTFLOW_TIME_MAX && value.tv_usec >= 0 && value.tv_usec < 1000000;
    clock.utc = (uint32_t)value.tv_sec; clock.subsecond_ms = (uint16_t)(value.tv_usec / 1000);
    clock.valid = clock.valid && clock.utc >= WRISTFLOW_TIME_MIN && clock.utc <= WRISTFLOW_TIME_MAX;
    clock.revision = rtc_revision; clock.monotonic_ms = monotonic_locked();
    rt_mutex_release(&rtc_lock); return clock;
}
bool wristflow_product_services_storage_ready(void) { return storage_ready; }
static const char *const steps_keys[] = {"steps_a", "steps_b"};
size_t wristflow_product_services_steps_read(unsigned key, uint8_t *record, size_t capacity, void *context)
{
    (void)context;
    if (!storage_ready || key >= 2 || !record || !capacity) return 0;
    struct fdb_blob blob;
    size_t size = fdb_kv_get_blob(&settings_db, steps_keys[key],
        fdb_blob_make(&blob, record, capacity));
    return size && size == blob.saved.len && size <= capacity ? size : 0;
}
bool wristflow_product_services_steps_write(unsigned key, const uint8_t *record, void *context)
{
    (void)context;
    if (!storage_ready || key >= 2) return false;
    struct fdb_blob blob;
    return fdb_kv_set_blob(&settings_db, steps_keys[key],
        fdb_blob_make(&blob, record, WRISTFLOW_STEPS_RECORD_BYTES)) == FDB_NO_ERR;
}
static const char *const alarm_keys[] = {"alarm_a", "alarm_b", "alarm_ca", "alarm_cb"};
bool wristflow_product_services_alarm_read(unsigned key, uint8_t *record, void *context)
{
    (void)context;
    if (!storage_ready || key >= 4) return false;
    struct fdb_blob blob;
    size_t size = fdb_kv_get_blob(&settings_db, alarm_keys[key], fdb_blob_make(&blob, record, WRISTFLOW_ALARM_BYTES));
    return size == WRISTFLOW_ALARM_BYTES && blob.saved.len == WRISTFLOW_ALARM_BYTES;
}
bool wristflow_product_services_alarm_write(unsigned key, const uint8_t *record, void *context)
{
    (void)context;
    if (!storage_ready || key >= 4) return false;
    struct fdb_blob blob;
    fdb_err_t result = fdb_kv_set_blob(&settings_db, alarm_keys[key], fdb_blob_make(&blob, record, WRISTFLOW_ALARM_BYTES));
    if (result != FDB_NO_ERR) rt_kprintf("[alarm] FlashDB key=%u result=%d\n", key, result);
    return result == FDB_NO_ERR;
}

static void lock_weather(void *context)
{
    rt_mutex_take((rt_mutex_t)context, RT_WAITING_FOREVER);
}
static void unlock_weather(void *context)
{
    rt_mutex_release((rt_mutex_t)context);
}

static wristflow_weather_time_t weather_time(void *context)
{
    (void)context;
    /* Called only under weather_lock. Extend raw ticks before scaling. */
    static uint32_t previous;
    static uint64_t elapsed_ticks;
    uint32_t tick = rt_tick_get();
    elapsed_ticks += (uint32_t)(tick - previous);
    previous = tick;
    wristflow_weather_time_t time = {.monotonic_ms = elapsed_ticks * 1000U / RT_TICK_PER_SECOND};
    uint32_t utc = 0;
    rt_mutex_take(&rtc_lock, RT_WAITING_FOREVER);
    bool valid = rtc && rt_device_control(rtc, RT_DEVICE_CTRL_RTC_GET_TIME, &utc) == RT_EOK;
    rt_mutex_release(&rtc_lock);
    if (valid && utc >= WRISTFLOW_TIME_MIN && utc <= WRISTFLOW_TIME_MAX) time.utc_seconds = utc;
    return time;
}

/* Keep flash operations off the lock used to collect UI setting changes. */
static void lock_database(fdb_db_t db)
{
    (void)db;
    rt_mutex_take(&database_lock, RT_WAITING_FOREVER);
}

static void unlock_database(fdb_db_t db)
{
    (void)db;
    rt_mutex_release(&database_lock);
}

static const char *const layout_keys[] = {"layout_a", "layout_b"};
static bool read_layout(unsigned key, uint8_t *record, void *context)
{
    (void)context;
    struct fdb_blob blob;
    size_t size = fdb_kv_get_blob(&settings_db, layout_keys[key], fdb_blob_make(&blob, record, WRISTFLOW_LAYOUT_BYTES));
    return size == WRISTFLOW_LAYOUT_BYTES && blob.saved.len == WRISTFLOW_LAYOUT_BYTES;
}

static bool write_layout(unsigned key, const uint8_t *record, void *context)
{
    (void)context;
    struct fdb_blob blob;
    fdb_err_t result = fdb_kv_set_blob(&settings_db, layout_keys[key], fdb_blob_make(&blob, record, WRISTFLOW_LAYOUT_BYTES));
    if (result != FDB_NO_ERR) rt_kprintf("[product] layout FlashDB result=%d\n", result);
    return result == FDB_NO_ERR;
}

static bool read_settings(void *context, uint8_t record[WRISTFLOW_SETTINGS_BYTES])
{
    (void)context;
    struct fdb_blob blob;
    size_t size = fdb_kv_get_blob(&settings_db, "preferences", fdb_blob_make(&blob, record, WRISTFLOW_SETTINGS_BYTES));
    return size == WRISTFLOW_SETTINGS_BYTES && blob.saved.len == WRISTFLOW_SETTINGS_BYTES;
}
static bool write_settings(void *context, const uint8_t record[WRISTFLOW_SETTINGS_BYTES])
{
    (void)context;
    struct fdb_blob blob;
    fdb_err_t result = fdb_kv_set_blob(&settings_db, "preferences",
        fdb_blob_make(&blob, record, WRISTFLOW_SETTINGS_BYTES));
    if (result != FDB_NO_ERR) rt_kprintf("[product] preferences FlashDB result=%d\n", result);
    return result == FDB_NO_ERR;
}

void wristflow_product_services_start(wristflow_settings_t *settings, wristflow_layout_t *layout)
{
    *settings = wristflow_settings_default();
    *layout = wristflow_layout_default();
    wristflow_layout_store_t layout_store = {.active = -1};
    RT_ASSERT(rt_mutex_init(&database_lock, "wf_db", RT_IPC_FLAG_PRIO) == RT_EOK);
    RT_ASSERT(rt_mutex_init(&rtc_lock, "wf_rtc", RT_IPC_FLAG_PRIO) == RT_EOK);
    RT_ASSERT(rt_mutex_init(&weather_lock, "wf_weather", RT_IPC_FLAG_PRIO) == RT_EOK);
    wristflow_weather_set_lock(lock_weather, unlock_weather, &weather_lock);
    rtc = rt_device_find("rtc");
    wristflow_weather_set_clock(weather_time, NULL);
    fdb_kvdb_control(&settings_db, FDB_KVDB_CTRL_SET_LOCK, lock_database);
    fdb_kvdb_control(&settings_db, FDB_KVDB_CTRL_SET_UNLOCK, unlock_database);
    fdb_err_t result = fdb_kvdb_init(&settings_db, "preferences", "settings", NULL, NULL);
    storage_ready = result == FDB_NO_ERR;
    bool restored = false;
    if (storage_ready) {
        uint8_t data[WRISTFLOW_SETTINGS_BYTES];
        if (read_settings(NULL, data)) restored = wristflow_settings_decode(settings, data, sizeof data);
        bool layout_restored = wristflow_layout_restore(&layout_store, layout, read_layout, NULL);
        rt_kprintf("[product] layout restore=%s generation=%u pages=%u\n",
            layout_restored ? "yes" : "defaults", layout_store.generation, layout->count);
    }
    if (storage_ready) {
        const wristflow_settings_store_io_t backend = {NULL, read_settings, write_settings, read_layout, write_layout};
        storage_ready = wristflow_product_settings_store_start(settings, restored, &layout_store, &backend);
    }
    service_ready = true;
    rt_kprintf("[product] storage=%s restore=%s brightness=%u face=%s breathing=%u rtc=%s\n",
        storage_ready ? "ready" : "unavailable", restored ? "yes" : "defaults",
        settings->brightness, settings->face_id, settings->breathing_minutes, rtc ? "available" : "missing");
}

uint32_t wristflow_product_services_settings(const wristflow_settings_t *settings, void *context)
{
    return storage_ready ? wristflow_product_settings_request(settings, context) : 0;
}

wristflow_save_state_t wristflow_product_services_settings_status(uint32_t request, void *context)
{ return wristflow_product_settings_status(request, context); }

uint32_t wristflow_product_services_layout(const wristflow_layout_t *layout, void *context)
{
    return storage_ready ? wristflow_product_layout_request(layout, context) : 0;
}

wristflow_save_state_t wristflow_product_services_layout_status(uint32_t request, void *context)
{
    return wristflow_product_layout_status(request, context);
}

wristflow_watch_snapshot_t wristflow_product_services_snapshot(void)
{
    uint32_t seconds = 0;
    rt_mutex_take(&rtc_lock, RT_WAITING_FOREVER);
    bool valid = rtc && rt_device_control(rtc, RT_DEVICE_CTRL_RTC_GET_TIME, &seconds) == RT_EOK;
    rt_mutex_release(&rtc_lock);
    wristflow_watch_snapshot_t snapshot = wristflow_product_snapshot(valid, seconds);
    snapshot.uptime_seconds = rt_tick_get() / RT_TICK_PER_SECOND;
    return snapshot;
}

int wristflow_product_services_set_time(uint32_t seconds)
{
    if (!service_ready || !rtc) return -RT_ERROR;
    if (seconds < WRISTFLOW_TIME_MIN || seconds > WRISTFLOW_TIME_MAX) return -RT_EINVAL;
    uint32_t readback = 0;
    rt_mutex_take(&rtc_lock, RT_WAITING_FOREVER);
    rt_err_t result = rt_device_control(rtc, RT_DEVICE_CTRL_RTC_SET_TIME, &seconds);
    ++rtc_revision; /* Also invalidates scheduling if SET/readback is uncertain. */
    if (result == RT_EOK) result = rt_device_control(rtc, RT_DEVICE_CTRL_RTC_GET_TIME, &readback);
    rt_mutex_release(&rtc_lock);
    wristflow_product_alarm_clock_changed();
    wristflow_product_steps_clock_changed();
    wristflow_product_event_send(WF_EVENT_TIME);
    if (result != RT_EOK || readback < seconds || readback - seconds > 1) {
        rt_kprintf("[product] RTC set/readback failed: %d\n", result);
        return -RT_ERROR;
    }
    rt_kprintf("[product] RTC synchronized: UTC=%u; display UTC+8\n", readback);
    return RT_EOK;
}

void wristflow_product_services_update_weather(const wristflow_phone_weather_t *w, unsigned updates)
{
    wristflow_weather_update(w, updates);
    if (w) {
        rt_kprintf("[product] weather updated: %s %d C, condition=%s code=%d hum=%u%%\n",
                   w->city, w->temp, w->condition, w->code, w->humidity);
        unsigned hours = 0, days = 0;
        for (unsigned i = 0; i < WRISTFLOW_WEATHER_HOURLY_COUNT; ++i) hours += w->extra.hourly[i].valid;
        for (unsigned i = 0; i < WRISTFLOW_WEATHER_DAILY_COUNT; ++i) days += w->extra.daily[i].valid;
        rt_kprintf("[product] weather forecast: hours=%u days=%u sunrise=%u sunset=%u\n",
                   hours, days, w->extra.sunrise, w->extra.sunset);
    }
}


/* USB and phone synchronization share the same checked UTC boundary. */
static int wf_time(int argc, char **argv)
{
    if (!service_ready || !rtc) return -RT_ERROR;
    if (argc != 2) {
        rt_kprintf("Usage: wf_time <UTC Unix seconds, 2026..2099>; display UTC+8\n");
        return -RT_EINVAL;
    }
    char *end;
    errno = 0;
    unsigned long value = strtoul(argv[1], &end, 10);
    if (errno || *end || end == argv[1] || argv[1][0] == '-' ||
        value < WRISTFLOW_TIME_MIN || value > WRISTFLOW_TIME_MAX) return -RT_EINVAL;
    return wristflow_product_services_set_time((uint32_t)value);
}
MSH_CMD_EXPORT(wf_time, Set product RTC with UTC Unix seconds);
