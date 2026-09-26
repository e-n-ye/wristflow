#include "product_services.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <flashdb.h>
#include <stdlib.h>
#include <errno.h>

static struct fdb_kvdb settings_db;
static struct rt_mutex settings_lock, database_lock, rtc_lock;
static wristflow_settings_t requested, boot_settings;
static wristflow_layout_t requested_layout;
static wristflow_layout_store_t layout_store;
static uint32_t layout_requested, layout_completed, layout_failed;
static uint32_t requested_at;
static bool storage_ready, service_ready;
static rt_device_t rtc;

static uint32_t milliseconds(void)
{
    return (uint32_t)((uint64_t)rt_tick_get() * 1000 / RT_TICK_PER_SECOND);
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

static void storage_thread(void *context)
{
    (void)context;
    wristflow_settings_t saved = boot_settings;
    uint32_t last_attempt = 0;
    bool retry = false;
    uint32_t layout_attempt = 0, layout_attempt_at = 0;
    for (;;) {
        rt_thread_mdelay(100);
        rt_mutex_take(&settings_lock, RT_WAITING_FOREVER);
        wristflow_settings_t next = requested;
        uint32_t changed = requested_at;
        wristflow_layout_t next_layout = requested_layout;
        uint32_t layout_id = layout_requested, completed = layout_completed;
        rt_mutex_release(&settings_lock);
        uint32_t now = milliseconds();
        if (layout_id != completed && (layout_id != layout_attempt || now - layout_attempt_at >= 5000)) {
            layout_attempt = layout_id; layout_attempt_at = now;
            uint32_t started = milliseconds();
            bool success = wristflow_layout_commit(&layout_store, &next_layout, read_layout, write_layout, NULL);
            rt_mutex_take(&settings_lock, RT_WAITING_FOREVER);
            if (success) layout_completed = layout_id;
            else layout_failed = layout_id;
            rt_mutex_release(&settings_lock);
            rt_kprintf("[product] layout %s request=%u generation=%u pages=%u write_ms=%u\n",
                success ? "saved" : "save failed", layout_id, layout_store.generation, next_layout.count,
                milliseconds() - started);
        }
        if (wristflow_settings_equal(&saved, &next) || now - changed < 1500 ||
            (retry && now - last_attempt < 5000)) continue;
        uint8_t data[WRISTFLOW_SETTINGS_BYTES];
        struct fdb_blob blob;
        if (!wristflow_settings_encode(&next, data)) continue;
        last_attempt = now;
        fdb_err_t result = fdb_kv_set_blob(&settings_db, "preferences", fdb_blob_make(&blob, data, sizeof data));
        retry = result != FDB_NO_ERR;
        if (!retry) saved = next;
        rt_kprintf("[product] settings %s brightness=%u face=%s result=%d\n",
                   retry ? "save failed" : "saved", next.brightness, next.face_id, result);
    }
}

void wristflow_product_services_start(wristflow_settings_t *settings, wristflow_layout_t *layout)
{
    *settings = wristflow_settings_default();
    *layout = wristflow_layout_default();
    layout_store.active = -1;
    RT_ASSERT(rt_mutex_init(&settings_lock, "wf_cfg", RT_IPC_FLAG_PRIO) == RT_EOK);
    RT_ASSERT(rt_mutex_init(&database_lock, "wf_db", RT_IPC_FLAG_PRIO) == RT_EOK);
    RT_ASSERT(rt_mutex_init(&rtc_lock, "wf_rtc", RT_IPC_FLAG_PRIO) == RT_EOK);
    rtc = rt_device_find("rtc");
    fdb_kvdb_control(&settings_db, FDB_KVDB_CTRL_SET_LOCK, lock_database);
    fdb_kvdb_control(&settings_db, FDB_KVDB_CTRL_SET_UNLOCK, unlock_database);
    fdb_err_t result = fdb_kvdb_init(&settings_db, "preferences", "settings", NULL, NULL);
    storage_ready = result == FDB_NO_ERR;
    bool restored = false;
    if (storage_ready) {
        uint8_t data[WRISTFLOW_SETTINGS_BYTES];
        struct fdb_blob blob;
        size_t length = fdb_kv_get_blob(&settings_db, "preferences", fdb_blob_make(&blob, data, sizeof data));
        if (blob.saved.len == sizeof data)
            restored = wristflow_settings_decode(settings, data, length);
        bool layout_restored = wristflow_layout_restore(&layout_store, layout, read_layout, NULL);
        rt_kprintf("[product] layout restore=%s generation=%u pages=%u\n",
            layout_restored ? "yes" : "defaults", layout_store.generation, layout->count);
    }
    requested = *settings;
    boot_settings = *settings;
    requested_layout = *layout;
    if (storage_ready) {
        rt_thread_t worker = rt_thread_create("wf_store", storage_thread, NULL, 6144, 25, 10);
        if (!worker || rt_thread_startup(worker) != RT_EOK) storage_ready = false;
    }
    service_ready = true;
    rt_kprintf("[product] storage=%s restore=%s brightness=%u face=%s rtc=%s\n",
        storage_ready ? "ready" : "unavailable", restored ? "yes" : "defaults",
        settings->brightness, settings->face_id, rtc ? "available" : "missing");
}

void wristflow_product_services_settings(const wristflow_settings_t *settings)
{
    if (!storage_ready || !wristflow_settings_valid(settings)) return;
    rt_mutex_take(&settings_lock, RT_WAITING_FOREVER);
    if (!wristflow_settings_equal(settings, &requested)) {
        requested = *settings;
        requested_at = milliseconds();
    }
    rt_mutex_release(&settings_lock);
}

uint32_t wristflow_product_services_layout(const wristflow_layout_t *layout, void *context)
{
    (void)context;
    if (!storage_ready || !wristflow_layout_valid(layout)) return 0;
    rt_mutex_take(&settings_lock, RT_WAITING_FOREVER);
    requested_layout = *layout;
    if (++layout_requested == 0) ++layout_requested;
    uint32_t id = layout_requested;
    rt_mutex_release(&settings_lock);
    return id;
}

wristflow_save_state_t wristflow_product_services_layout_status(uint32_t request, void *context)
{
    (void)context;
    if (!storage_ready || !request) return WRISTFLOW_SAVE_FAILED;
    rt_mutex_take(&settings_lock, RT_WAITING_FOREVER);
    wristflow_save_state_t state = layout_completed == request ? WRISTFLOW_SAVE_DONE :
        layout_failed == request ? WRISTFLOW_SAVE_FAILED : WRISTFLOW_SAVE_PENDING;
    rt_mutex_release(&settings_lock);
    return state;
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

/* Temporary USB bring-up interface. Phone time sync will use the same UTC boundary. */
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
    uint32_t seconds = (uint32_t)value, readback = 0;
    rt_mutex_take(&rtc_lock, RT_WAITING_FOREVER);
    rt_err_t result = rt_device_control(rtc, RT_DEVICE_CTRL_RTC_SET_TIME, &seconds);
    if (result == RT_EOK) result = rt_device_control(rtc, RT_DEVICE_CTRL_RTC_GET_TIME, &readback);
    rt_mutex_release(&rtc_lock);
    if (result != RT_EOK || readback < seconds || readback - seconds > 1) {
        rt_kprintf("[product] RTC set/readback failed: %d\n", result);
        return -RT_ERROR;
    }
    rt_kprintf("[product] RTC synchronized: UTC=%u; display UTC+8\n", readback);
    return RT_EOK;
}
MSH_CMD_EXPORT(wf_time, Set product RTC with UTC Unix seconds);
