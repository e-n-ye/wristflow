#include "product_imu_diag.h"
#include "imu_pedometer.h"
#include "imu_wrist_tilt.h"
#include "product_steps.h"
#include "product_wrist.h"
#include "product_wrist_irq.h"
#include "product_pm.h"
#include "product_services.h"
#include "steps_store.h"
#include "ltr303_diag.h"
#include "product_light.h"
#include "auto_brightness.h"
#include "mmc5603_diag.h"
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <board.h>
#include "drv_gpio.h"
#include <stdlib.h>
#include <string.h>

#ifndef BSP_USING_I2C3
#error "Product native IMU diagnostic requires BSP_USING_I2C3"
#endif

enum { EV_COMMAND = 1, EV_STOP = 2, EV_DAILY_START = 4, EV_SCREEN = 8, EV_SAVE = 16,
    EV_WRIST = 32, EV_WRIST_STOP = 64, EV_WRIST_POLICY = 128, EV_WRIST_IRQ = 256,
    EV_LIGHT = 512, EV_LIGHT_STOP = 1024, EV_AUTO_LIGHT = 2048,
    EV_MAG = 4096, EV_MAG_STOP = 8192 };
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
static wf_imu_wrist_tilt_t wrist;
static wf_imu_wrist_snapshot_t wrist_cached;
static bool wrist_pending, wrist_busy;
static bool wrist_route;
static bool wrist_requested, wrist_runtime, wrist_wake_pending;
static bool wrist_suspended_cached, wrist_failed_cached;
static uint32_t wrist_wakes;
static volatile bool wrist_cancel_requested;
static unsigned wrist_seconds;
static uint8_t wrist_mask, wrist_latency;
static wf_ltr303_t light;
static wf_ltr303_snapshot_t light_cached;
static bool light_pending, light_busy;
static volatile bool light_cancel_requested;
static unsigned light_seconds;
static bool auto_requested, auto_active, auto_owner, auto_runtime;
static bool auto_retry, auto_fault;
static volatile bool auto_cancel;
static uint32_t auto_generation, auto_applied_generation, auto_cached_generation, auto_started_ms, auto_sequence;
static wf_auto_brightness_t auto_filter;
static wristflow_product_light_snapshot_t auto_value, auto_cached;
static wf_mmc5603_t mag;
static wf_mmc5603_snapshot_t mag_cached;
static bool mag_pending, mag_busy;
static volatile bool mag_cancel_requested;
static unsigned mag_seconds;
static bool mag_selftest_only;
static wf_mag_threshold_t mag_threshold_mode;
static bool prepare_bus(void);
static bool probe(void);
static wristflow_steps_clock_t daily_clock(void);
static void light_work(rt_uint32_t events);
static void mag_work(rt_uint32_t events);
static uint32_t now_ms(void);

bool wristflow_product_light_enable(bool enabled)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool accepted = ready && daily_mode && (!enabled ||
        (!light_pending && (!light_busy || auto_cached.cleanup_pending || auto_requested) &&
         !wrist_pending && (!wrist_busy || (wrist_runtime && wrist_cached.continuous))));
    if (accepted && (auto_requested != enabled || (enabled && auto_fault) || (!enabled && auto_cached.cleanup_pending))) {
        auto_requested = enabled; auto_fault = false; auto_retry = true;
        ++auto_generation; auto_cancel = true;
        rt_event_send(&work, EV_AUTO_LIGHT);
    }
    rt_hw_interrupt_enable(level);
    return accepted;
}
void wristflow_product_light_active(bool active)
{
    rt_base_t level = rt_hw_interrupt_disable();
    if (auto_active != active) {
        auto_active = active; ++auto_generation; auto_cancel = true;
        if (ready && daily_mode && (auto_requested || auto_cached.cleanup_pending)) rt_event_send(&work, EV_AUTO_LIGHT);
    }
    rt_hw_interrupt_enable(level);
}
wristflow_product_light_snapshot_t wristflow_product_light_snapshot(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wristflow_product_light_snapshot_t value = auto_cached;
    value.enabled = auto_requested; value.active = auto_requested && auto_active;
    value.fault = auto_fault;
    value.valid = value.valid && value.active && auto_cached_generation == auto_generation && !auto_cancel;
    rt_hw_interrupt_enable(level);
    if (value.valid && now_ms() - value.sampled_ms >= 5000U) value.valid = false;
    return value;
}

static void wrist_irq_signal(void)
{ if (ready) rt_event_send(&work, EV_WRIST_IRQ); }

void wristflow_product_wrist_enable(bool enabled)
{
    rt_base_t level = rt_hw_interrupt_disable();
    if (ready && daily_mode && wrist_requested != enabled) {
        wrist_requested = enabled;
        wrist_wake_pending = false;
        if (!enabled && wrist_runtime) wrist_cancel_requested = true;
        rt_event_send(&work, EV_WRIST_POLICY);
    }
    rt_hw_interrupt_enable(level);
}

bool wristflow_product_wrist_take_wake(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool wake = wrist_requested && wrist_wake_pending;
    wrist_wake_pending = false;
    rt_hw_interrupt_enable(level);
    return wake;
}
bool wristflow_product_wrist_suspended(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool value = ready && !wrist_requested && wrist_suspended_cached;
    rt_hw_interrupt_enable(level); return value;
}
bool wristflow_product_wrist_failed(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool value = wrist_failed_cached;
    rt_hw_interrupt_enable(level); return value;
}

static uint32_t now_ms(void)
{ return (uint32_t)((uint64_t)rt_tick_get() * 1000U / RT_TICK_PER_SECOND); }
static uint32_t io_clock(void *context) { (void)context; return now_ms(); }
static bool io_cancelled(void *context) { (void)context; return cancel_requested; }
static bool wrist_cancelled(void *context)
{ (void)context; return wrist_cancel_requested || cancel_requested; }
static void wrist_delay(void *context, uint32_t ms)
{ (void)context; rt_thread_mdelay((rt_int32_t)ms); }
static int wrist_int1_read(void *context)
{ (void)context; return rt_pin_read(GET_PIN(1, 31)); }
static bool wrist_int1_prepare(void)
{
    if (!rt_device_find("pin")) return false;
    /* Board V1.2 schematic: IMU INT1 -> PA31. Passive input only; this
     * diagnostic never attaches an IRQ/AON source or drives the line. */
    HAL_PIN_Set(PAD_PA31, GPIO_A31, PIN_NOPULL, 1);
    rt_pin_mode(GET_PIN(1, 31), PIN_MODE_INPUT);
    return true;
}
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
static bool light_read(void *context, uint8_t reg, uint8_t *value)
{ (void)context; return rt_i2c_mem_read(bus, 0x29, reg, 8, value, 1) == 1; }
static bool light_write(void *context, uint8_t reg, uint8_t value)
{ (void)context; return rt_i2c_mem_write(bus, 0x29, reg, 8, &value, 1) == 1; }
static bool light_cancelled(void *context)
{ (void)context; return light_cancel_requested || cancel_requested || (auto_owner && auto_cancel); }
static bool mag_read(void *context, uint8_t reg, uint8_t *data, size_t size)
{ (void)context; return rt_i2c_mem_read(bus, 0x30, reg, 8, data, size) == size; }
static bool mag_write(void *context, uint8_t reg, uint8_t value)
{ (void)context; return rt_i2c_mem_write(bus, 0x30, reg, 8, &value, 1) == 1; }
static bool mag_cancelled(void *context)
{ (void)context; return mag_cancel_requested || cancel_requested; }
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
    bool wrist_unresolved = wrist.changed && !wrist.snapshot.stop_verified;
    wristflow_wrist_irq_snapshot_t irq_state = wristflow_product_wrist_irq_snapshot();
    gate((active && !daily_mode) || unresolved ||
        (wrist.snapshot.phase == WF_IMU_RUNNING && !wrist.snapshot.continuous) ||
        (wrist_unresolved && wrist.snapshot.phase != WF_IMU_RUNNING) || (irq_state.owned && !irq_state.armed) ||
        light.snapshot.phase == WF_LIGHT_RUNNING || light.snapshot.owned ||
        mag.snapshot.phase == WF_MAG_RUNNING || mag.snapshot.owned);
    rt_base_t level = rt_hw_interrupt_disable();
    cached = device.snapshot;
    wrist_cached = wrist.snapshot;
    light_cached = light.snapshot;
    mag_cached = mag.snapshot;
    mag_busy = mag_pending || mag.snapshot.phase == WF_MAG_RUNNING || mag.snapshot.owned;
    auto_cached = auto_value;
    auto_cached_generation = auto_applied_generation;
    auto_cached.cleanup_pending = auto_owner && light.snapshot.owned && !auto_runtime;
    light_busy = light_pending || light.snapshot.phase == WF_LIGHT_RUNNING || light.snapshot.owned;
    wrist_failed_cached = wrist.snapshot.phase == WF_IMU_FAULT;
    wrist_suspended_cached = !wrist_runtime && !wrist.changed && !wrist_unresolved &&
        !irq_state.owned && !irq_state.armed && !wrist_failed_cached;
    wrist_busy = wrist_pending || wrist.snapshot.phase == WF_IMU_RUNNING || wrist_unresolved;
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
        {
            value.metric.total_valid = false;
            value.metric.hours_valid = 0;
        }
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

static void wrist_frame_report(const char *stage, const wf_imu_wrist_frame_t *f)
{
    rt_kprintf("[imu-wrist-frame] stage=%s attempted=%u valid=0x%03x begin=%lu end=%lu\n",
        stage, f->attempted, f->valid, (unsigned long)f->started_ms, (unsigned long)f->completed_ms);
    rt_kprintf("[imu-wrist-regs] stage=%s bank=%02x who=%02x xl=%02x ctrl3=%02x ctrl10=%02x pulse=%02x tap=%02x\n",
        stage, f->bank, f->who, f->xl, f->ctrl3, f->ctrl10, f->pulse, f->tap);
    rt_kprintf("[imu-wrist-source] stage=%s status=%02x axes=%02x source=%02x ready=%u xyz_valid=%u\n",
        stage, f->status, f->axes, f->source, f->data_ready, f->xyz_valid);
    rt_kprintf("[imu-wrist-xyz] stage=%s raw=%02x,%02x,%02x,%02x,%02x,%02x x=%d y=%d z=%d; counts, not mg\n",
        stage, f->raw[0], f->raw[1], f->raw[2], f->raw[3], f->raw[4], f->raw[5],
        f->xyz[0], f->xyz[1], f->xyz[2]);
    rt_kprintf("[imu-wrist-pin] stage=%s ctrl4=%02x valid=%u pre=%u post=%u pre_ms=%lu post_ms=%lu; PA31 raw levels\n",
        stage, f->ctrl4, f->pin_valid, f->pin_before, f->pin_after,
        (unsigned long)f->pin_before_ms, (unsigned long)f->pin_after_ms);
    rt_kprintf("[imu-wrist-other-routes] stage=%s int1=%02x int2=%02x md1=%02x md2=%02x\n",
        stage, f->int1, f->int2, f->md1, f->md2);
}
static void wrist_report(bool frames)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wf_imu_wrist_snapshot_t s = wrist_cached;
    bool requested = wrist_requested;
    uint32_t wakes = wrist_wakes;
    rt_hw_interrupt_enable(level);
    wristflow_wrist_irq_snapshot_t irq_state = wristflow_product_wrist_irq_snapshot();
    rt_kprintf("[imu-wrist] phase=%u end=%u configured=%u source_valid=%u stop_verified=%u\n",
        s.phase, s.ended, s.configured, s.source_valid, s.stop_verified);
    rt_kprintf("[imu-wrist] mask=0x%02x latency=%u ths=%u source=0x%02x axes=0x%02x observations=%lu errors=%lu\n",
        s.mask, s.latency, s.threshold, s.source, s.axes,
        (unsigned long)s.observations, (unsigned long)s.io_errors);
    rt_kprintf("[imu-wrist] start=%lu duration=%lu sample=%lu; latched source observations, not gestures\n",
        (unsigned long)s.started_ms, (unsigned long)s.duration_ms, (unsigned long)s.sampled_ms);
    if (frames) {
        rt_kprintf("[imu-wrist-route] int1=%u; event is last accepted source bit0 frame\n", s.route_int1);
        rt_kprintf("[wrist-wake] requested=%u continuous=%u armed=%u owned=%u irqs=%lu wakes=%lu; software counts, not physical DEEP proof\n",
            requested, s.continuous, irq_state.armed, irq_state.owned,
            (unsigned long)irq_state.irqs, (unsigned long)wakes);
        wrist_frame_report("before", &s.before);
        wrist_frame_report("running", &s.running);
        wrist_frame_report("after", &s.after);
        wrist_frame_report("event", &s.event);
    }
}
static void wrist_stop(bool keep_steps)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wrist_wake_pending = false;
    rt_hw_interrupt_enable(level);
    bool irq_stopped = wristflow_product_wrist_irq_stop();
    if (wrist.changed && !prepare_bus()) {
        wrist.snapshot.phase = WF_IMU_FAULT;
        wrist.snapshot.ended = WF_IMU_IO_FAILURE;
        wrist.snapshot.source_valid = wrist.snapshot.configured = wrist.snapshot.stop_verified = false;
    } else (void)wf_imu_wrist_stop(&wrist, now_ms(), WF_IMU_USER_STOP, keep_steps);
    device.wrist_enabled = false;
    wrist_runtime = false;
    if (!irq_stopped) {
        wrist.snapshot.phase = WF_IMU_FAULT;
        wrist.snapshot.ended = WF_IMU_IO_FAILURE;
        wrist.snapshot.stop_verified = false;
    }
}
static void wrist_work(rt_uint32_t events)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool begin = wrist_pending;
    unsigned span = wrist_seconds;
    uint8_t mask = wrist_mask, latency = wrist_latency;
    bool route = wrist_route;
    bool requested = wrist_requested;
    wrist_pending = false;
    rt_hw_interrupt_enable(level);
    if (events & EV_STOP) return; /* Shared shutdown was handled first. */
    if (requested && (events & EV_WRIST_POLICY) && !wrist_runtime && wrist.snapshot.phase == WF_IMU_RUNNING)
        wrist_stop(device.snapshot.phase == WF_IMU_RUNNING);
    if ((events & EV_WRIST_STOP) || (!requested && wrist_runtime) ||
        ((events & EV_WRIST_POLICY) && !requested)) {
        wrist_stop(device.snapshot.phase == WF_IMU_RUNNING);
        wrist_cancel_requested = false;
        /* A prior uncertain wrist transaction faults the shared device. Explicit
         * cleanup may stop it safely; only wf_steps retry restarts its baseline. */
        if (!wrist.changed && device.snapshot.phase == WF_IMU_FAULT && !device.snapshot.stop_verified)
            (void)wf_imu_pedometer_stop(&device, now_ms(), WF_IMU_IO_FAILURE);
    } else if (requested && !wrist_runtime && !wrist.changed &&
        wrist.snapshot.phase != WF_IMU_FAULT && device.snapshot.phase == WF_IMU_RUNNING) {
        wrist_cancel_requested = false;
        bool ok = prepare_bus() && wristflow_product_wrist_irq_start(wrist_irq_signal);
        if (ok) ok = wf_imu_wrist_wake_begin(&wrist, 0x40, 15);
        if (ok) {
            wrist_runtime = true;
            /* A high latched before IRQ enable cannot produce another edge. */
            if (wrist_int1_read(NULL) == 1) rt_event_send(&work, EV_WRIST_IRQ);
        } else {
            wrist_stop(true);
            wrist.snapshot.phase = WF_IMU_FAULT;
            wrist.snapshot.ended = WF_IMU_IO_FAILURE;
        }
        rt_kprintf("[wrist-wake] enable=%u; native xneg600ms/30deg\n", ok);
    } else if (begin && !requested && device.snapshot.phase == WF_IMU_RUNNING) {
        bool ok = prepare_bus() && (!route || wrist_int1_prepare());
        if (ok) ok = route ? wf_imu_wrist_route_begin(&wrist, span * 1000U, mask, latency) :
            wf_imu_wrist_begin(&wrist, span * 1000U, mask, latency);
        rt_kprintf("[imu-wrist] accepted=%u\n", ok);
    } else if (wrist.snapshot.phase == WF_IMU_RUNNING) {
        if (device.snapshot.phase != WF_IMU_RUNNING) wrist_stop(false);
        else if (prepare_bus()) {
            uint32_t observed = wrist.snapshot.observations;
            wf_imu_wrist_poll(&wrist, now_ms());
            if (wrist_runtime && wrist.snapshot.phase == WF_IMU_RUNNING &&
                wrist.snapshot.observations != observed && wrist.snapshot.event.pin_after == 0) {
                rt_base_t wake_level = rt_hw_interrupt_disable();
                if (wrist_requested && !wrist_cancel_requested && !cancel_requested) {
                    wrist_wake_pending = true;
                    ++wrist_wakes;
                    wristflow_product_event_send(WF_EVENT_WRIST);
                }
                rt_hw_interrupt_enable(wake_level);
            }
        }
        else {
            wrist.snapshot.phase = WF_IMU_FAULT;
            wrist.snapshot.ended = WF_IMU_IO_FAILURE;
            wrist.snapshot.source_valid = wrist.snapshot.configured = wrist.snapshot.stop_verified = false;
        }
    }
    if (wrist_runtime && wrist.snapshot.phase != WF_IMU_RUNNING) {
        bool fault = wrist.snapshot.phase == WF_IMU_FAULT;
        wrist_stop(device.snapshot.phase == WF_IMU_RUNNING);
        if (fault) {
            wrist.snapshot.phase = WF_IMU_FAULT;
            wrist.snapshot.ended = WF_IMU_IO_FAILURE;
        }
    }
    /* Latched high after a successful clear is unsafe to leave armed: retain a
     * visible fault instead of a tight retry loop or an unverified wake. */
    if (wrist_runtime && wrist.snapshot.running.attempted && wrist.snapshot.running.pin_after) {
        wrist_stop(true);
        wrist.snapshot.phase = WF_IMU_FAULT;
        wrist.snapshot.ended = WF_IMU_IO_FAILURE;
    }
    device.wrist_enabled = wrist.snapshot.phase == WF_IMU_RUNNING && wrist.snapshot.configured;
    if (wrist.changed && !wrist.snapshot.stop_verified && wrist.snapshot.phase == WF_IMU_FAULT) {
        device.snapshot.phase = WF_IMU_FAULT;
        device.snapshot.counter_valid = device.snapshot.stop_verified = false;
    }
}
static void daily_work(rt_uint32_t events)
{
    gate(true);
    if (events & (EV_STOP | EV_DAILY_START)) { light_work(events); mag_work(events); }
    wf_imu_wrist_snapshot_t before_wrist = wrist.snapshot;
    if (events & (EV_STOP | EV_DAILY_START)) {
        if (wrist.changed || wrist.snapshot.phase == WF_IMU_RUNNING ||
            wristflow_product_wrist_irq_snapshot().owned) wrist_stop(false);
        wrist_cancel_requested = false;
    }
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
        bool prepared = !light.snapshot.owned && !mag.snapshot.owned && prepare_bus();
        if (prepared && device.identified && device.snapshot.phase == WF_IMU_FAULT)
            prepared = wf_imu_pedometer_stop(&device, now_ms(), WF_IMU_USER_STOP);
        bool ok = prepared && !wrist.changed && !wristflow_product_wrist_irq_snapshot().owned &&
            probe() && wf_imu_pedometer_begin_continuous(&device);
        if (!ok) wristflow_daily_steps_invalidate(&daily);
    } else if (device.snapshot.phase == WF_IMU_RUNNING) {
        if (prepare_bus()) wf_imu_pedometer_poll(&device, now_ms());
        else {
            device.snapshot.phase = WF_IMU_FAULT;
            device.snapshot.counter_valid = device.snapshot.stop_verified = false;
        }
    }
    wrist_work(events);
    if (device.snapshot.phase == WF_IMU_RUNNING && device.snapshot.counter_valid) {
        if (!wristflow_daily_steps_observe(&daily, device.snapshot.steps, daily_clock()))
            (void)wf_imu_pedometer_stop(&device, now_ms(), WF_IMU_IO_FAILURE);
    } else if (daily.snapshot.metric.total_valid) wristflow_daily_steps_invalidate(&daily);
    if (device.snapshot.phase != WF_IMU_RUNNING &&
        (wrist.snapshot.phase == WF_IMU_RUNNING || wristflow_product_wrist_irq_snapshot().owned))
        wrist_stop(false);
    if (checkpoint_enabled) {
        uint64_t now = daily_clock().monotonic_ms;
        wristflow_steps_store_offer(&checkpoint, &daily.snapshot, now);
        if (events & EV_SAVE) wristflow_steps_store_flush(&checkpoint, now);
    }
    if (!(events & EV_STOP)) {
        light_work(events & ~EV_DAILY_START);
        mag_work(events & ~EV_DAILY_START);
    }
    publish(); /* Releases this owner's IDLE unless stopping remains unconfirmed. */
    if (wrist.snapshot.phase != before_wrist.phase || wrist.snapshot.observations != before_wrist.observations ||
        (events & (EV_WRIST | EV_WRIST_STOP | EV_WRIST_POLICY | EV_WRIST_IRQ))) wrist_report(false);
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
static void light_report(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wf_ltr303_snapshot_t s = light_cached;
    bool queued = light_pending;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[light] session=%lu phase=%u end=%u queued=%u owned=%u restored=%u errors=%lu\n",
        (unsigned long)s.session, s.phase, s.ended, queued, s.owned, s.restored, (unsigned long)s.io_errors);
    rt_kprintf("[light-config] id_valid=%u part=%02x manufacturer=%02x before_valid=%u ctrl=%02x rate=%02x\n",
        s.identity_valid, s.part, s.manufacturer, s.before_valid, s.before_ctrl, s.before_rate);
    rt_kprintf("[light-config] entered_valid=%u ctrl=%02x rate=%02x after_valid=%u ctrl=%02x rate=%02x\n",
        s.entered_valid, s.entered_ctrl, s.entered_rate, s.after_valid, s.after_ctrl, s.after_rate);
    rt_kprintf("[light-sample] session=%lu seq=%lu ms=%lu age=%lu status=%02x ch0=%u ch1=%u valid=%u saturated=%u; raw counts, not lux\n",
        (unsigned long)s.session, (unsigned long)s.samples, (unsigned long)s.sampled_ms,
        (unsigned long)(s.samples ? now_ms() - s.sampled_ms : 0), s.status, s.ch0, s.ch1, s.sample_valid, s.saturated);
    rt_kprintf("[light-time] start=%lu duration=%lu not_ready=%lu; 1x/100ms integration/500ms repeat, raw channels\n",
        (unsigned long)s.started_ms, (unsigned long)s.duration_ms, (unsigned long)s.not_ready);
    wristflow_product_light_snapshot_t a = wristflow_product_light_snapshot();
    rt_kprintf("[auto-light] enabled=%u active=%u valid=%u fault=%u cleanup=%u brightness=%u mlux=%lu ms=%lu session=%lu seq=%lu; uncalibrated\n",
        a.enabled, a.active, a.valid, a.fault, a.cleanup_pending, a.brightness,
        (unsigned long)a.millilux, (unsigned long)a.sampled_ms, (unsigned long)a.session, (unsigned long)a.sequence);
}
static bool auto_stop(void)
{
    auto_value.valid = false;
    if (light.snapshot.phase == WF_LIGHT_RUNNING || light.snapshot.owned) {
        if (!light.snapshot.owned || prepare_bus()) (void)wf_ltr303_stop(&light, WF_LIGHT_CANCELLED);
        else wf_ltr303_bus_fault(&light);
    }
    auto_runtime = false;
    auto_owner = light.snapshot.owned;
    return !auto_owner;
}
static void auto_fail(uint32_t generation)
{
    auto_value.valid = false;
    rt_base_t level = rt_hw_interrupt_disable();
    if (auto_generation == generation) {
        auto_requested = false; auto_fault = true; auto_cancel = true; ++auto_generation;
    }
    rt_hw_interrupt_enable(level);
    rt_kprintf("[auto-light] fault; manual fallback, explicit retry required\n");
}
static void auto_light_work(rt_uint32_t events)
{
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t generation = auto_generation;
    bool enabled = auto_requested, active = auto_active, retry = auto_retry;
    auto_retry = false;
    rt_hw_interrupt_enable(level);
    bool stop = events & (EV_STOP | EV_DAILY_START | EV_LIGHT_STOP);
    if (stop && (auto_runtime || auto_owner || (events & (EV_STOP | EV_LIGHT_STOP)))) {
        rt_base_t lock = rt_hw_interrupt_disable();
        auto_requested = false; auto_cancel = true; ++auto_generation;
        rt_hw_interrupt_enable(lock);
        (void)auto_stop();
        light_cancel_requested = false;
        return;
    }
    if (events & EV_DAILY_START) return; /* New daily owner is not prepared yet. */
    if (auto_runtime && (!enabled || !active || generation != auto_applied_generation)) {
        if (!auto_stop()) { auto_fail(generation); return; }
    }
    if (!enabled || !active) {
        auto_value.valid = false;
        if (auto_owner && retry && !auto_stop()) auto_fail(generation);
        return;
    }
    if (!auto_runtime) {
        /* Failed cleanup is only retried by a user command, never by a timer
         * or an OFF/ACTIVE transition. Never steal a diagnostic ALS owner. */
        if (auto_owner && (!retry || !auto_stop())) { auto_fail(generation); return; }
        if (light.snapshot.phase == WF_LIGHT_RUNNING || light.snapshot.owned ||
            device.snapshot.phase != WF_IMU_RUNNING || wrist.snapshot.phase == WF_IMU_FAULT ||
            (wrist.snapshot.phase == WF_IMU_RUNNING && !wrist.snapshot.continuous)) {
            auto_fail(generation); return;
        }
        auto_applied_generation = generation;
        auto_started_ms = now_ms(); auto_sequence = 0;
        auto_filter = (wf_auto_brightness_t){0}; auto_value = (wristflow_product_light_snapshot_t){0};
        rt_base_t lock = rt_hw_interrupt_disable();
        if (auto_generation == generation) auto_cancel = false;
        rt_hw_interrupt_enable(lock);
        auto_owner = auto_runtime = wf_ltr303_begin_continuous(&light, auto_started_ms);
        if (!auto_runtime) { auto_fail(generation); return; }
    }
    if (device.snapshot.phase != WF_IMU_RUNNING || wrist.snapshot.phase == WF_IMU_FAULT) {
        (void)auto_stop(); auto_fail(generation); return;
    }
    if (wf_ltr303_wait_ms(&light, now_ms()) == 0 || auto_cancel) {
        if (!light.snapshot.owned && auto_cancel) wf_ltr303_poll(&light);
        else if (prepare_bus()) wf_ltr303_poll(&light);
        else wf_ltr303_bus_fault(&light);
    }
    /* Recheck the generation after I/O. A cancelled old frame must never be
     * published as the result of a newer enable/ACTIVE request. */
    rt_base_t lock = rt_hw_interrupt_disable();
    bool current = auto_generation == generation;
    rt_hw_interrupt_enable(lock);
    if (!current) {
        if (light.snapshot.phase == WF_LIGHT_RUNNING) (void)auto_stop();
        else {
            auto_value.valid = false;
            auto_runtime = false; auto_owner = light.snapshot.owned;
        }
        if (light.snapshot.phase == WF_LIGHT_FAULT || auto_owner) {
            /* poll may already have attempted cancellation/restore. Never
             * repeat that uncertain transaction on this generation change. */
            rt_base_t state = rt_hw_interrupt_disable();
            uint32_t failed_generation = auto_generation;
            auto_retry = false;
            rt_hw_interrupt_enable(state);
            auto_fail(failed_generation);
        }
        return;
    }
    if (light.snapshot.phase != WF_LIGHT_RUNNING) {
        /* poll already attempted restoration once. Keep unresolved ownership
         * and IDLE; don't hide the failure by retrying in the same cycle. */
        auto_runtime = false; auto_owner = light.snapshot.owned;
        auto_fail(generation); return;
    }
    if (light.snapshot.samples != auto_sequence) {
        auto_sequence = light.snapshot.samples;
        uint32_t mlux;
        if (light.snapshot.sample_valid && wf_light_millilux(light.snapshot.ch0, light.snapshot.ch1, &mlux)) {
            auto_value = (wristflow_product_light_snapshot_t){.valid=true,
                .brightness=wf_auto_brightness_observe(&auto_filter, mlux), .millilux=mlux,
                .sampled_ms=light.snapshot.sampled_ms, .session=light.snapshot.session, .sequence=auto_sequence};
        }
    }
    uint32_t last = auto_value.valid ? auto_value.sampled_ms : auto_started_ms;
    if (now_ms() - last >= 5000U) {
        (void)auto_stop(); auto_fail(generation);
    }
}
static void light_work(rt_uint32_t events)
{
    if (auto_requested || auto_runtime || auto_owner) { auto_light_work(events); return; }
    rt_base_t level = rt_hw_interrupt_disable();
    bool begin = light_pending;
    unsigned span = light_seconds;
    light_pending = false;
    rt_hw_interrupt_enable(level);
    wf_ltr303_snapshot_t before = light.snapshot;
    bool stopping = (events & (EV_STOP | EV_DAILY_START | EV_LIGHT_STOP)) || light_cancel_requested;
    if (stopping && (light.snapshot.phase == WF_LIGHT_RUNNING || light.snapshot.owned ||
        begin || light_cancel_requested || (events & EV_LIGHT_STOP))) {
        if (!light.snapshot.owned || prepare_bus()) (void)wf_ltr303_stop(&light, WF_LIGHT_CANCELLED);
        else wf_ltr303_bus_fault(&light);
        light_cancel_requested = false;
    } else if (!stopping && begin) {
        /* One I2C3 owner. A finite wrist diagnostic and LTR diagnostic never
         * overlap; the persistent IRQ-driven wrist preference is preserved. */
        bool available = device.snapshot.phase == WF_IMU_RUNNING && !cancel_requested &&
            wrist.snapshot.phase != WF_IMU_FAULT &&
            (wrist.snapshot.phase != WF_IMU_RUNNING || wrist.snapshot.continuous);
        uint32_t started = now_ms();
        if (available) {
            bool prepared = prepare_bus();
            if (wf_ltr303_begin(&light, started, span * 1000U) && !prepared) wf_ltr303_bus_fault(&light);
        }
        rt_kprintf("[light] accepted=%u\n", light.snapshot.phase == WF_LIGHT_RUNNING);
    } else if (light.snapshot.phase == WF_LIGHT_RUNNING) {
        if (device.snapshot.phase != WF_IMU_RUNNING || wrist.snapshot.phase == WF_IMU_FAULT) {
            if (!light.snapshot.owned || prepare_bus()) (void)wf_ltr303_stop(&light, WF_LIGHT_IO_FAILURE);
            else wf_ltr303_bus_fault(&light);
        } else if (light_cancelled(NULL) || wf_ltr303_wait_ms(&light, now_ms()) == 0) {
            if (!light.snapshot.owned && (light_cancelled(NULL) ||
                now_ms() - light.snapshot.started_ms >= light.snapshot.duration_ms)) wf_ltr303_poll(&light);
            else if (prepare_bus()) wf_ltr303_poll(&light);
            else wf_ltr303_bus_fault(&light);
        }
    }
    if (light.snapshot.samples != before.samples) {
        wf_ltr303_snapshot_t *s = &light.snapshot;
        rt_kprintf("[light-data] session=%lu seq=%lu ms=%lu status=%02x ch0=%u ch1=%u valid=%u saturated=%u\n",
            (unsigned long)s->session, (unsigned long)s->samples, (unsigned long)s->sampled_ms,
            s->status, s->ch0, s->ch1, s->sample_valid, s->saturated);
    }
    if (light.snapshot.phase != before.phase || (events & EV_LIGHT_STOP)) {
        rt_kprintf("[light-state] session=%lu phase=%u end=%u owned=%u restored=%u errors=%lu\n",
            (unsigned long)light.snapshot.session, light.snapshot.phase, light.snapshot.ended,
            light.snapshot.owned, light.snapshot.restored, (unsigned long)light.snapshot.io_errors);
    }
}
static void mag_trace_report(const wf_mmc5603_snapshot_t *s)
{
    rt_kprintf("[mag-mode] session=%lu selftest_only=%u threshold=%s experimental=%u\n",
        (unsigned long)s->session, s->selftest_only,
        s->threshold_mode == WF_MAG_THRESHOLD_RAW80 ? "raw80" : "encoded",
        s->threshold_mode == WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL);
    rt_kprintf("[mag-selftest-time] session=%lu trace=%02x reset=%02x@%lu command=%lu ack=%lu polls=%lu\n",
        (unsigned long)s->session, s->selftest_trace, s->reset_status, (unsigned long)s->reset_observed_ms,
        (unsigned long)s->selftest_command_ms, (unsigned long)s->selftest_ack_ms, (unsigned long)s->selftest_polls);
    rt_kprintf("[mag-selftest-state] session=%lu observed=%02x@%lu post=%02x@%lu data_ms=%lu; software observation\n",
        (unsigned long)s->session, s->selftest_status, (unsigned long)s->selftest_status_ms,
        s->selftest_post_status, (unsigned long)s->selftest_post_ms, (unsigned long)s->selftest_data_ms);
    rt_kprintf("[mag-selftest-raw] session=%lu trace=%02x bytes=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x; response only\n",
        (unsigned long)s->session, s->selftest_trace,
        s->selftest_raw[0], s->selftest_raw[1], s->selftest_raw[2], s->selftest_raw[3], s->selftest_raw[4],
        s->selftest_raw[5], s->selftest_raw[6], s->selftest_raw[7], s->selftest_raw[8]);
}
static void mag_work(rt_uint32_t events)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool begin = mag_pending;
    unsigned span = mag_seconds;
    bool selftest_only = mag_selftest_only;
    wf_mag_threshold_t threshold_mode = mag_threshold_mode;
    mag_pending = false;
    rt_hw_interrupt_enable(level);
    wf_mmc5603_snapshot_t before = mag.snapshot;
    bool stopping = (events & (EV_STOP | EV_DAILY_START | EV_MAG_STOP)) || mag_cancel_requested;
    if (stopping && (mag.snapshot.phase == WF_MAG_RUNNING || mag.snapshot.owned ||
        begin || mag_cancel_requested || (events & EV_MAG_STOP))) {
        if (!mag.snapshot.owned || prepare_bus()) (void)wf_mmc5603_stop(&mag, WF_MAG_CANCELLED);
        else wf_mmc5603_bus_fault(&mag);
        mag_cancel_requested = false;
    } else if (!stopping && begin) {
        bool available = device.snapshot.phase == WF_IMU_RUNNING && !cancel_requested &&
            wrist.snapshot.phase != WF_IMU_FAULT &&
            (wrist.snapshot.phase != WF_IMU_RUNNING || wrist.snapshot.continuous) &&
            (light.snapshot.phase != WF_LIGHT_RUNNING || auto_owner);
        uint32_t started = now_ms();
        if (available) {
            bool prepared = prepare_bus();
            bool began = selftest_only ? wf_mmc5603_begin_selftest(&mag, started, threshold_mode) :
                wf_mmc5603_begin(&mag, started, span * 1000U);
            if (began && !prepared) wf_mmc5603_bus_fault(&mag);
        }
        rt_kprintf("[mag] accepted=%u\n", mag.snapshot.phase == WF_MAG_RUNNING);
    } else if (mag.snapshot.phase == WF_MAG_RUNNING) {
        if (device.snapshot.phase != WF_IMU_RUNNING || wrist.snapshot.phase == WF_IMU_FAULT) {
            if (!mag.snapshot.owned || prepare_bus()) (void)wf_mmc5603_stop(&mag, WF_MAG_IO_FAILURE);
            else wf_mmc5603_bus_fault(&mag);
        } else if (mag_cancelled(NULL) || wf_mmc5603_wait_ms(&mag, now_ms()) == 0) {
            if (!mag.snapshot.owned && (mag_cancelled(NULL) ||
                now_ms() - mag.snapshot.started_ms >= mag.snapshot.duration_ms)) wf_mmc5603_poll(&mag);
            else if (prepare_bus()) wf_mmc5603_poll(&mag);
            else wf_mmc5603_bus_fault(&mag);
        }
    }
    if (mag.snapshot.selftest_valid && !before.selftest_valid) {
        wf_mmc5603_snapshot_t *s = &mag.snapshot;
        rt_kprintf("[mag-selftest] session=%lu factory=%02x/%02x/%02x threshold=%02x/%02x/%02x status=%02x passed=%u\n",
            (unsigned long)s->session, s->selftest_factory[0], s->selftest_factory[1], s->selftest_factory[2],
            s->selftest_threshold[0], s->selftest_threshold[1], s->selftest_threshold[2],
            s->selftest_status, s->selftest_passed);
    }
    if (mag.snapshot.samples != before.samples) {
        wf_mmc5603_snapshot_t *s = &mag.snapshot;
        rt_kprintf("[mag-data] session=%lu seq=%lu ms=%lu status=%02x xyz=%ld/%ld/%ld valid=%u saturated=%u; raw counts\n",
            (unsigned long)s->session, (unsigned long)s->samples, (unsigned long)s->sampled_ms,
            s->status, (long)s->x, (long)s->y, (long)s->z, s->sample_valid, s->saturated);
    }
    if (mag.snapshot.phase != before.phase || (events & EV_MAG_STOP)) {
        if (!mag.snapshot.selftest_valid && mag.snapshot.selftest_trace &&
            mag.snapshot.phase != WF_MAG_RUNNING && before.phase == WF_MAG_RUNNING) mag_trace_report(&mag.snapshot);
        rt_kprintf("[mag-state] session=%lu phase=%u end=%u owned=%u stopped=%u errors=%lu\n",
            (unsigned long)mag.snapshot.session, mag.snapshot.phase, mag.snapshot.ended,
            mag.snapshot.owned, mag.snapshot.stopped, (unsigned long)mag.snapshot.io_errors);
    }
    if (mag.snapshot.selftest_valid && !before.selftest_valid) mag_trace_report(&mag.snapshot);
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
        if (daily_mode && wrist.snapshot.phase == WF_IMU_RUNNING && !wrist.snapshot.continuous) {
            uint32_t elapsed = now_ms() - wrist.snapshot.started_ms;
            uint32_t left = elapsed >= wrist.snapshot.duration_ms ? 1 : wrist.snapshot.duration_ms - elapsed;
            rt_tick_t wrist_wait = rt_tick_from_millisecond(left < 100U ? left : 100U);
            if (!wrist_wait) wrist_wait = 1;
            if (wait == (rt_tick_t)RT_WAITING_FOREVER || wrist_wait < wait) wait = wrist_wait;
        }
        int32_t light_left = wf_ltr303_wait_ms(&light, now_ms());
        if (light_left >= 0) {
            rt_tick_t light_wait = rt_tick_from_millisecond((unsigned)light_left);
            if (!light_wait) light_wait = 1;
            if (wait == (rt_tick_t)RT_WAITING_FOREVER || light_wait < wait) wait = light_wait;
        }
        int32_t mag_left = wf_mmc5603_wait_ms(&mag, now_ms());
        if (mag_left >= 0) {
            rt_tick_t mag_wait = rt_tick_from_millisecond((unsigned)mag_left);
            if (!mag_wait) mag_wait = 1;
            if (wait == (rt_tick_t)RT_WAITING_FOREVER || mag_wait < wait) wait = mag_wait;
        }
        rt_uint32_t events = 0;
        rt_event_recv(&work, EV_COMMAND | EV_STOP | EV_DAILY_START | EV_SCREEN | EV_SAVE | EV_WRIST | EV_WRIST_STOP |
            EV_WRIST_POLICY | EV_WRIST_IRQ | EV_LIGHT | EV_LIGHT_STOP | EV_AUTO_LIGHT | EV_MAG | EV_MAG_STOP,
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

static void wf_mag(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "status")) {
        rt_base_t level = rt_hw_interrupt_disable();
        wf_mmc5603_snapshot_t s = mag_cached;
        rt_hw_interrupt_enable(level);
        rt_kprintf("[mag] session=%lu phase=%u end=%u id=%02x identity_valid=%u owned=%u stopped=%u errors=%lu\n",
            (unsigned long)s.session, s.phase, s.ended, s.identity, s.identity_valid,
            s.owned, s.stopped, (unsigned long)s.io_errors);
        rt_kprintf("[mag] start=%lu duration=%lu ms=%lu seq=%lu status=%02x after=%02x xyz=%ld/%ld/%ld valid=%u saturated=%u\n",
            (unsigned long)s.started_ms, (unsigned long)s.duration_ms, (unsigned long)s.sampled_ms,
            (unsigned long)s.samples, s.status, s.after_status, (long)s.x, (long)s.y, (long)s.z,
            s.sample_valid, s.saturated);
        rt_kprintf("[mag] selftest_valid=%u passed=%u status=%02x factory=%02x/%02x/%02x threshold=%02x/%02x/%02x; uncalibrated counts, no heading\n",
            s.selftest_valid, s.selftest_passed, s.selftest_status,
            s.selftest_factory[0], s.selftest_factory[1], s.selftest_factory[2],
            s.selftest_threshold[0], s.selftest_threshold[1], s.selftest_threshold[2]);
        mag_trace_report(&s);
        return;
    }
    if (!ready || !daily_mode) { rt_kprintf("[mag] shared sensor worker unavailable\n"); return; }
    if (argc == 2 && !strcmp(argv[1], "stop")) {
        rt_base_t level = rt_hw_interrupt_disable();
        mag_pending = false; mag_cancel_requested = true;
        rt_event_send(&work, EV_MAG_STOP);
        rt_hw_interrupt_enable(level);
        return;
    }
    unsigned span = 0;
    bool selftest_only = false;
    wf_mag_threshold_t threshold_mode = WF_MAG_THRESHOLD_RAW80;
    if (argc == 3 && !strcmp(argv[1], "start")) {
        char *end;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (argv[2][0] >= '0' && argv[2][0] <= '9' && !*end && value >= 1 && value <= 120) span = (unsigned)value;
    } else if (argc == 3 && !strcmp(argv[1], "selftest")) {
        if (!strcmp(argv[2], "raw80") || !strcmp(argv[2], "encoded")) {
            selftest_only = true;
            threshold_mode = !strcmp(argv[2], "encoded") ? WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL : WF_MAG_THRESHOLD_RAW80;
            span = WF_MAG_SELFTEST_TIMEOUT_MS / 1000U;
        }
    }
    if (!span) {
        rt_kprintf("wf_mag start <1..120 seconds> | selftest raw80|encoded | stop | status\n");
        rt_kprintf("selftest: 10s total limit, no XYZ; encoded is an experimental threshold hypothesis, not a validated correction\n");
        return;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    bool accepted = cached.phase == WF_IMU_RUNNING && !daily_retry_pending && !cancel_requested &&
        !mag_busy && !mag_pending && !mag_cancel_requested && !mag_cached.owned &&
        !wrist_cancel_requested && wrist_cached.phase != WF_IMU_FAULT &&
        !wrist_pending && (!wrist_busy || (wrist_runtime && wrist_cached.continuous)) &&
        !light_pending && !light_cancel_requested && !auto_cached.cleanup_pending &&
        (!light_busy || auto_owner);
    if (accepted) {
        mag_pending = mag_busy = true; mag_seconds = span;
        mag_selftest_only = selftest_only; mag_threshold_mode = threshold_mode;
        rt_event_send(&work, EV_MAG);
    }
    rt_hw_interrupt_enable(level);
    rt_kprintf("[mag] queued=%u\n", accepted);
}
MSH_CMD_EXPORT(wf_mag, bounded MMC5603 native selftest and raw XYZ on shared sensor worker);

static void wf_light(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "status")) { light_report(); return; }
    if (!ready || !daily_mode) { rt_kprintf("[light] shared sensor worker unavailable\n"); return; }
    if (argc == 2 && !strcmp(argv[1], "stop")) {
        rt_base_t level = rt_hw_interrupt_disable();
        light_pending = false;
        light_cancel_requested = true;
        rt_event_send(&work, EV_LIGHT_STOP);
        rt_hw_interrupt_enable(level);
        return;
    }
    unsigned span = 0;
    if (argc == 3 && !strcmp(argv[1], "start")) {
        char *end;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (argv[2][0] >= '0' && argv[2][0] <= '9' && !*end && value >= 1 && value <= 120) span = (unsigned)value;
    }
    if (!span) { rt_kprintf("wf_light start <1..120 seconds> | stop | status; raw channels, not lux\n"); return; }
    rt_base_t level = rt_hw_interrupt_disable();
    bool accepted = cached.phase == WF_IMU_RUNNING && !daily_retry_pending && !cancel_requested &&
        !wrist_cancel_requested && wrist_cached.phase != WF_IMU_FAULT &&
        !wrist_pending && (!wrist_busy || (wrist_runtime && wrist_cached.continuous)) &&
        !auto_requested && !mag_busy && !mag_pending && !light_busy && !light_pending && !light_cancel_requested &&
        light_cached.phase != WF_LIGHT_RUNNING && !light_cached.owned;
    if (accepted) {
        light_pending = light_busy = true; light_seconds = span;
        rt_event_send(&work, EV_LIGHT);
    }
    rt_hw_interrupt_enable(level);
    rt_kprintf("[light] queued=%u\n", accepted);
}
MSH_CMD_EXPORT(wf_light, bounded LTR303 raw channel observation on the shared sensor worker);

static void wf_wrist(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "status")) { wrist_report(true); return; }
    if (!ready || !daily_mode) { rt_kprintf("[imu-wrist] daily IMU owner unavailable\n"); return; }
    if (argc == 2 && !strcmp(argv[1], "stop")) {
        rt_base_t level = rt_hw_interrupt_disable();
        wrist_pending = false;
        wrist_requested = false; /* Explicit diagnostic stop suspends the RAM request. */
        wrist_wake_pending = false;
        wrist_cancel_requested = true;
        rt_event_send(&work, EV_WRIST_STOP);
        rt_hw_interrupt_enable(level);
        return;
    }
    unsigned span = 0, latency = 15;
    uint8_t mask = 0;
    static const char *const axes[] = {"xpos", "xneg", "ypos", "yneg", "zpos", "zneg"};
    bool route = argc >= 2 && !strcmp(argv[1], "route");
    if ((argc == 4 || argc == 5) && (!strcmp(argv[1], "start") || route)) {
        char *end;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (argv[2][0] >= '0' && argv[2][0] <= '9' && !*end && value >= 1 && value <= 120) span = (unsigned)value;
        for (unsigned i = 0; i < 6; ++i) if (!strcmp(argv[3], axes[i])) mask = (uint8_t)(0x80U >> i);
        if (argc == 5) {
            value = strtoul(argv[4], &end, 10);
            latency = argv[4][0] >= '0' && argv[4][0] <= '9' && !*end && value >= 1 && value <= 255 ? (unsigned)value : 0;
        }
    }
    if (!span || !mask || !latency) {
        rt_kprintf("wf_wrist start <1..120 seconds> <xpos|xneg|ypos|yneg|zpos|zneg> [latency1..255x40ms]\n");
        rt_kprintf("wf_wrist status | stop; bounded commands do not enable display wake; stop suspends RAM request\n");
        rt_kprintf("wf_wrist route <1..120 seconds> <axis> [latency]; INT2 OR INT1 / PA31 levels\n");
        return;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    bool accepted = !wrist_requested && cached.phase == WF_IMU_RUNNING && !wrist_busy && !cancel_requested &&
        !wrist_cancel_requested && wrist_cached.phase != WF_IMU_FAULT &&
        !auto_requested && !mag_busy && !mag_pending && !light_busy && !light_pending && !light_cached.owned && light_cached.phase != WF_LIGHT_RUNNING;
    if (accepted) {
        wrist_busy = wrist_pending = true;
        wrist_seconds = span; wrist_mask = mask; wrist_latency = (uint8_t)latency;
        wrist_route = route;
        rt_event_send(&work, EV_WRIST);
    }
    rt_hw_interrupt_enable(level);
    if (!accepted) rt_kprintf("[imu-wrist] rejected: daily counter must run; stop active/fault diagnostic first\n");
}
MSH_CMD_EXPORT(wf_wrist, bounded embedded absolute wrist tilt observation alongside native steps);

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
        uint32_t sum = 0;
        for (unsigned h = 0; h < 24; ++h)
            if (s.metric.hours_valid & (1U << h)) sum += s.metric.hourly[h];
        rt_kprintf("[steps-hours] valid=0x%06lx sum=%lu saved_valid=0x%06lx\n",
            (unsigned long)s.metric.hours_valid, (unsigned long)sum, (unsigned long)saved.saved.hours_valid);
        for (unsigned h = 0; h < 24; ++h)
            if (s.metric.hours_valid & (1U << h))
                rt_kprintf("[steps-hour] hour=%u total=%lu\n", h, (unsigned long)s.metric.hourly[h]);
        return;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    if (argc == 2 && !strcmp(argv[1], "save")) {
        rt_event_send(&work, EV_SAVE);
    } else if (argc == 2 && !strcmp(argv[1], "stop")) {
        wrist_requested = false;
        wrist_wake_pending = false;
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
    wf_imu_wrist_init(&wrist, (wf_imu_io_t){NULL, read_reg, write_reg, io_clock, wrist_cancelled}, wrist_delay);
    wf_ltr303_init(&light, (wf_ltr303_io_t){NULL, light_read, light_write, io_clock, light_cancelled});
    wf_mmc5603_init(&mag, (wf_mmc5603_io_t){NULL, mag_read, mag_write, io_clock, mag_cancelled, wrist_delay});
    mag_cached = mag.snapshot;
    mag_pending = mag_busy = mag_cancel_requested = false;
    mag_selftest_only = false; mag_threshold_mode = WF_MAG_THRESHOLD_RAW80; mag_seconds = 0;
    light_cached = light.snapshot;
    light_pending = light_busy = light_cancel_requested = false;
    auto_requested = auto_active = auto_owner = auto_runtime = auto_retry = auto_fault = auto_cancel = false;
    auto_generation = auto_applied_generation = auto_cached_generation = auto_started_ms = auto_sequence = 0;
    auto_value = auto_cached = (wristflow_product_light_snapshot_t){0};
    wrist.read_int1 = wrist_int1_read;
    wrist_cached = wrist.snapshot;
    wrist_pending = wrist_busy = wrist_cancel_requested = false;
    wrist_requested = wrist_runtime = wrist_wake_pending = false;
    wrist_wakes = 0;
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
