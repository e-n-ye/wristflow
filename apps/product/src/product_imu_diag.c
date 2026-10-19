#include "product_imu_diag.h"
#include "imu_pedometer.h"
#include "product_steps.h"
#include "product_services.h"
#include "steps_store.h"
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <board.h>
#include <stdlib.h>
#include <string.h>

#ifndef BSP_USING_I2C3
#error "Product native IMU diagnostic requires BSP_USING_I2C3"
#endif

enum { EV_COMMAND = 1, EV_STOP = 2, EV_DAILY_START = 4, EV_SCREEN = 8, EV_SAVE = 16 };
enum command { CMD_NONE, CMD_PROBE, CMD_STEPS };
static struct rt_event work;
static struct rt_i2c_bus_device *bus;
static wf_imu_pedometer_t device;
static wf_imu_steps_snapshot_t cached;
static enum command pending;
static unsigned seconds;
static uint8_t address;
static bool ready, occupied, hold;
static volatile bool cancel_requested;
static bool daily_mode, daily_retry_pending, daily_lit = true;
static wristflow_daily_steps_t daily;
static wristflow_steps_snapshot_t daily_cached;
static wristflow_steps_store_t checkpoint, checkpoint_cached;
static bool checkpoint_initialized, checkpoint_enabled;
static bool prepare_bus(void);
static bool probe(void);
static wristflow_steps_clock_t daily_clock(void);

static uint32_t now_ms(void)
{ return (uint32_t)((uint64_t)rt_tick_get() * 1000U / RT_TICK_PER_SECOND); }
static uint32_t io_clock(void *context) { (void)context; return now_ms(); }
static bool io_cancelled(void *context) { (void)context; return cancel_requested; }
static bool read_reg(void *context, uint8_t reg, uint8_t *data, size_t size)
{
    (void)context;
    return rt_i2c_mem_read(bus, address, reg, 8, data, size) == size;
}
static bool write_reg(void *context, uint8_t reg, uint8_t value)
{
    (void)context;
    return rt_i2c_mem_write(bus, address, reg, 8, &value, 1) == 1;
}
static void gate(bool take)
{
#ifdef RT_USING_PM
    if (take && !hold) rt_pm_request(PM_SLEEP_MODE_IDLE);
    if (!take && hold) rt_pm_release(PM_SLEEP_MODE_IDLE);
#endif
    hold = take;
}
static void publish(void)
{
    bool active = device.snapshot.phase == WF_IMU_RUNNING;
    /* A failed stop keeps ownership/IDLE until an explicit successful stop. */
    bool unresolved = device.identified && !device.snapshot.stop_verified &&
        device.snapshot.phase == WF_IMU_FAULT;
    gate((active && !daily_mode) || unresolved);
    rt_base_t level = rt_hw_interrupt_disable();
    cached = device.snapshot;
    if (daily_mode) {
        daily_cached = daily.snapshot;
        checkpoint_cached = checkpoint;
        daily_retry_pending = false;
    }
    occupied = active || unresolved || pending != CMD_NONE;
    rt_hw_interrupt_enable(level);
}

wristflow_steps_snapshot_t wristflow_product_steps_snapshot(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wristflow_steps_snapshot_t value = daily_cached;
    rt_hw_interrupt_enable(level);
    if (value.observation.native_source) {
        wristflow_steps_clock_t clock = daily_clock();
        if (!clock.valid || clock.monotonic_ms < value.observation.sampled_ms ||
            clock.monotonic_ms - value.observation.sampled_ms > WRISTFLOW_STEPS_MAX_GAP_MS ||
            clock.revision != value.observation.clock_revision ||
            ((uint64_t)clock.utc + 28800U) / 86400U != value.observation.local_day)
            value.metric.total_valid = false;
    }
    return value;
}

void wristflow_product_steps_screen(bool visible)
{
    rt_base_t level = rt_hw_interrupt_disable();
    daily_lit = visible;
    if (ready && daily_mode) rt_event_send(&work, EV_SCREEN);
    rt_hw_interrupt_enable(level);
}

void wristflow_product_steps_clock_changed(void)
{ if (ready && daily_mode) rt_event_send(&work, EV_SCREEN); }

static wristflow_steps_clock_t daily_clock(void)
{
    wristflow_alarm_clock_t clock = wristflow_product_services_time();
    return (wristflow_steps_clock_t){.monotonic_ms = clock.monotonic_ms,
        .utc = clock.utc, .revision = clock.revision, .valid = clock.valid};
}

static void daily_work(rt_uint32_t events)
{
    gate(true);
    bool boot_seed = false;
    if (!checkpoint_initialized) {
        checkpoint_enabled = wristflow_product_services_storage_ready();
        boot_seed = checkpoint_enabled && wristflow_steps_store_restore(&checkpoint,
            wristflow_product_services_steps_read, NULL);
        checkpoint_initialized = true;
        rt_kprintf("[steps-store] available=%u restore=%u day=%lu total=%lu generation=%lu\n",
            checkpoint_enabled, boot_seed, (unsigned long)checkpoint.saved.local_day,
            (unsigned long)checkpoint.saved.total, (unsigned long)checkpoint.generation);
    }
    if (events & EV_STOP) {
        if (device.identified && (!prepare_bus() ||
            !wf_imu_pedometer_stop(&device, now_ms(), WF_IMU_USER_STOP))) {
            device.snapshot.phase = WF_IMU_FAULT;
            device.snapshot.stop_verified = false;
        }
        wristflow_daily_steps_invalidate(&daily);
        cancel_requested = false;
    } else if (events & EV_DAILY_START) {
        /* Recovery is explicit, serialized, and never power-cycles PA30.
         * Previously known totals are not silently mixed with a reset count. */
        uint32_t version = daily.snapshot.observation.version;
        wristflow_daily_steps_init(&daily);
        daily.snapshot.observation.version = version;
        if (boot_seed) (void)wristflow_daily_steps_seed(&daily, &checkpoint.saved);
        bool prepared = prepare_bus();
        if (prepared && device.identified && device.snapshot.phase == WF_IMU_FAULT)
            prepared = wf_imu_pedometer_stop(&device, now_ms(), WF_IMU_USER_STOP);
        bool ok = prepared && probe() && wf_imu_pedometer_begin_continuous(&device);
        if (!ok) wristflow_daily_steps_invalidate(&daily);
    } else if (device.snapshot.phase == WF_IMU_RUNNING) {
        if (prepare_bus()) wf_imu_pedometer_poll(&device, now_ms());
        else {
            device.snapshot.phase = WF_IMU_FAULT;
            device.snapshot.counter_valid = device.snapshot.stop_verified = false;
        }
    }
    if (device.snapshot.phase == WF_IMU_RUNNING && device.snapshot.counter_valid) {
        if (!wristflow_daily_steps_observe(&daily, device.snapshot.steps, daily_clock()))
            (void)wf_imu_pedometer_stop(&device, now_ms(), WF_IMU_IO_FAILURE);
    } else if (daily.snapshot.metric.total_valid) wristflow_daily_steps_invalidate(&daily);
    if (checkpoint_enabled) {
        uint64_t now = daily_clock().monotonic_ms;
        wristflow_steps_store_offer(&checkpoint, &daily.snapshot, now);
        if (events & EV_SAVE) wristflow_steps_store_flush(&checkpoint, now);
    }
    publish(); /* Releases this owner's IDLE unless stopping remains unconfirmed. */
    if (checkpoint_enabled) {
        uint64_t now = daily_clock().monotonic_ms;
        if (wristflow_steps_store_wait(&checkpoint, now) == 0) {
            gate(true); /* Flash never runs under the snapshot critical section. */
            (void)wristflow_steps_store_process(&checkpoint, now,
                wristflow_product_services_steps_read, wristflow_product_services_steps_write, NULL);
            rt_kprintf("[steps-store] verified=%u dirty=%u failed=%u attempts=%u day=%lu total=%lu generation=%lu\n",
                checkpoint.saved_valid, checkpoint.dirty, checkpoint.failed, checkpoint.attempts,
                (unsigned long)checkpoint.saved.local_day, (unsigned long)checkpoint.saved.total,
                (unsigned long)checkpoint.generation);
            publish();
        }
    }
}
static bool prepare_bus(void)
{
    bus = rt_i2c_bus_device_find("i2c3");
    if (!bus) {
        rt_kprintf("[imu-steps] bus i2c3 unavailable\n");
        return false;
    }
    HAL_PIN_Set(PAD_PA39, I2C3_SDA, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA40, I2C3_SCL, PIN_PULLUP, 1);
    /* Shared LTR/MMC/IMU supply: enable for I/O, never power-cycle on exit. */
    BSP_GPIO_Set(30, 1, 1);
    rt_thread_mdelay(10);
    struct rt_i2c_configuration config = {0, 0, 50, 100000};
    int result = rt_i2c_configure(bus, &config);
    if (result != RT_EOK) rt_kprintf("[imu-steps] bus configure failed=%d\n", result);
    return result == RT_EOK;
}
static bool probe(void)
{
    if (!prepare_bus()) return false;
    unsigned found = 0;
    uint8_t selected = 0;
    for (uint8_t candidate = 0x6a; candidate <= 0x6b; ++candidate) {
        uint8_t who = 0;
        bool match = rt_i2c_mem_read(bus, candidate, 0x0f, 8, &who, 1) == 1 && who == 0x6a;
        rt_kprintf("[imu-steps] probe addr=0x%02x who=0x%02x match=%u\n", candidate, who, match);
        if (match) { ++found; selected = candidate; }
    }
    if (found != 1) {
        device.identified = false;
        device.snapshot.phase = WF_IMU_UNPROBED;
        device.snapshot.counter_valid = device.snapshot.stop_verified = false;
        return false;
    }
    address = selected;
    return wf_imu_pedometer_probe(&device);
}
static void report(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wf_imu_steps_snapshot_t s = cached;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[imu-steps] phase=%u end=%u counter_valid=%u steps=%u stop_verified=%u\n",
        s.phase, s.ended, s.counter_valid, s.steps, s.stop_verified);
    rt_kprintf("[imu-steps] who=0x%02x ths=0x%02x deb=0x%02x start=%lu duration=%lu sample=%lu\n",
        s.who, s.threshold, s.debounce, (unsigned long)s.started_ms,
        (unsigned long)s.duration_ms, (unsigned long)s.sampled_ms);
    rt_kprintf("[imu-steps] io_errors=%lu incoherent=%lu; native counter, not daily steps\n",
        (unsigned long)s.io_errors, (unsigned long)s.incoherent_reads);
}
static void worker(void *context)
{
    (void)context;
    for (;;) {
        rt_tick_t wait = RT_WAITING_FOREVER;
        if (device.snapshot.phase == WF_IMU_RUNNING) {
            if (daily_mode) {
                rt_base_t level = rt_hw_interrupt_disable();
                unsigned interval = daily_lit ? 1000U : 60000U;
                rt_hw_interrupt_enable(level);
                wristflow_steps_clock_t clock = daily_clock();
                if (clock.valid) {
                    uint32_t boundary = (86400U - (clock.utc + 28800U) % 86400U) * 1000U;
                    if (boundary < interval) interval = boundary;
                }
                wait = rt_tick_from_millisecond(interval);
            } else {
                uint32_t elapsed = now_ms() - device.snapshot.started_ms;
                uint32_t remaining = elapsed >= device.snapshot.duration_ms ? 1 : device.snapshot.duration_ms - elapsed;
                wait = rt_tick_from_millisecond(remaining < 1000 ? remaining : 1000);
            }
            if (!wait) wait = 1;
        }
        if (daily_mode && checkpoint_enabled) {
            int32_t left = wristflow_steps_store_wait(&checkpoint, daily_clock().monotonic_ms);
            if (left >= 0) {
                rt_tick_t save_wait = rt_tick_from_millisecond((unsigned)left);
                if (!save_wait) save_wait = 1;
                if (wait == (rt_tick_t)RT_WAITING_FOREVER || save_wait < wait) wait = save_wait;
            }
        }
        rt_uint32_t events = 0;
        rt_event_recv(&work, EV_COMMAND | EV_STOP | EV_DAILY_START | EV_SCREEN | EV_SAVE,
            RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, wait, &events);
        if (daily_mode) {
            daily_work(events);
            continue;
        }
        rt_base_t level = rt_hw_interrupt_disable();
        enum command command = pending;
        unsigned span = seconds;
        pending = CMD_NONE;
        if (events & EV_STOP) cancel_requested = false;
        rt_hw_interrupt_enable(level);
        if (events & EV_STOP) {
            if (device.identified) {
                gate(true);
                if (prepare_bus()) (void)wf_imu_pedometer_stop(&device, now_ms(), WF_IMU_USER_STOP);
                else {
                    device.snapshot.phase = WF_IMU_FAULT;
                    device.snapshot.counter_valid = device.snapshot.stop_verified = false;
                }
            }
        } else if (command != CMD_NONE) {
            gate(true);
            bool ok = command == CMD_PROBE ? probe() : prepare_bus() &&
                wf_imu_pedometer_begin(&device, now_ms(), span * 1000U);
            rt_kprintf("[imu-steps] command=%u accepted=%u\n", command, ok);
        }
        wf_imu_pedometer_poll(&device, now_ms());
        publish();
        if (events || device.snapshot.phase != WF_IMU_RUNNING) report();
    }
}
static void wf_imu(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "status")) { report(); return; }
    if (daily_mode) {
        rt_kprintf("[imu-steps] daily owner active; diagnostics cannot reset or stop it; wf_steps status|stop|retry\n");
        return;
    }
    if (!ready) { rt_kprintf("[imu-steps] worker unavailable\n"); return; }
    if (argc == 2 && !strcmp(argv[1], "stop")) {
        rt_base_t level = rt_hw_interrupt_disable();
        pending = CMD_NONE;
        occupied = true;
        cancel_requested = true;
        rt_event_send(&work, EV_STOP);
        rt_hw_interrupt_enable(level);
        return;
    }
    enum command command = CMD_NONE;
    unsigned span = 0;
    if (argc == 2 && !strcmp(argv[1], "probe")) command = CMD_PROBE;
    if (argc == 3 && !strcmp(argv[1], "steps")) {
        char *end = NULL;
        unsigned long parsed = strtoul(argv[2], &end, 10);
        if (argv[2][0] && end && !*end && parsed >= 1 && parsed <= 120) {
            command = CMD_STEPS; span = (unsigned)parsed;
        }
    }
    if (command == CMD_NONE) { rt_kprintf("wf_imu probe | steps <1..120 seconds> | stop | status\n"); return; }
    rt_base_t level = rt_hw_interrupt_disable();
    bool accepted = !occupied && !cancel_requested && cached.phase != WF_IMU_FAULT &&
        (command == CMD_PROBE || cached.phase == WF_IMU_READY || cached.phase == WF_IMU_DONE);
    if (accepted) {
        occupied = true; pending = command; seconds = span;
        rt_event_send(&work, EV_COMMAND);
    }
    rt_hw_interrupt_enable(level);
    if (!accepted) rt_kprintf("[imu-steps] rejected: probe first, stop active/fault experiment\n");
}
MSH_CMD_EXPORT(wf_imu, bounded native step-counter diagnostic);

static void wf_steps(int argc, char **argv)
{
    if (!ready || !daily_mode) { rt_kprintf("[steps] daily service unavailable\n"); return; }
    if (argc == 2 && !strcmp(argv[1], "status")) {
        wristflow_steps_snapshot_t s = wristflow_product_steps_snapshot();
        report();
        rt_kprintf("[steps] valid=%u total=%lu day=%lu utc=%lu version=%lu native=%u partial=1 restored=%u\n",
            s.metric.total_valid, (unsigned long)s.metric.total, (unsigned long)s.observation.local_day,
            (unsigned long)s.observation.utc, (unsigned long)s.observation.version,
            s.observation.native_count, s.observation.restored);
        rt_base_t level = rt_hw_interrupt_disable();
        wristflow_steps_store_t saved = checkpoint_cached;
        rt_hw_interrupt_enable(level);
        rt_kprintf("[steps-store] available=%u verified=%u dirty=%u failed=%u attempts=%u day=%lu total=%lu generation=%lu\n",
            checkpoint_enabled, saved.saved_valid, saved.dirty, saved.failed, saved.attempts,
            (unsigned long)saved.saved.local_day, (unsigned long)saved.saved.total,
            (unsigned long)saved.generation);
        return;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    if (argc == 2 && !strcmp(argv[1], "save")) {
        rt_event_send(&work, EV_SAVE);
    } else if (argc == 2 && !strcmp(argv[1], "stop")) {
        cancel_requested = true;
        rt_event_send(&work, EV_STOP);
    } else if (argc == 2 && !strcmp(argv[1], "retry") &&
        cached.phase != WF_IMU_RUNNING && !cancel_requested && !daily_retry_pending) {
        daily_retry_pending = true;
        rt_event_send(&work, EV_DAILY_START);
    }
    rt_hw_interrupt_enable(level);
}
MSH_CMD_EXPORT(wf_steps, native daily steps status/save or stop/retry with a new baseline);

static bool start(bool daily_service)
{
    if (ready) return daily_mode == daily_service;
    if (rt_event_init(&work, "wf_imu", RT_IPC_FLAG_PRIO) != RT_EOK) return false;
    wf_imu_pedometer_init(&device, (wf_imu_io_t){NULL, read_reg, write_reg, io_clock, io_cancelled});
    daily_mode = daily_service;
    checkpoint_initialized = checkpoint_enabled = false;
    checkpoint = checkpoint_cached = (wristflow_steps_store_t){.active = -1};
    wristflow_daily_steps_init(&daily);
    rt_thread_t thread = rt_thread_create("wf_imu", worker, NULL, 6144, 24, 10);
    if (!thread) { rt_event_detach(&work); return false; }
    ready = true;
    if (daily_mode) { daily_retry_pending = true; rt_event_send(&work, EV_DAILY_START); }
    if (rt_thread_startup(thread) != RT_EOK) {
        ready = false;
        rt_thread_delete(thread); rt_event_detach(&work); return false;
    }
    return true;
}
bool wristflow_product_imu_diag_start(void) { return start(false); }
bool wristflow_product_steps_start(void) { return start(true); }
