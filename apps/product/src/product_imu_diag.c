#include "product_imu_diag.h"
#include "imu_pedometer.h"
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <board.h>
#include <stdlib.h>
#include <string.h>

#ifndef BSP_USING_I2C3
#error "Product native IMU diagnostic requires BSP_USING_I2C3"
#endif

enum { EV_COMMAND = 1, EV_STOP = 2 };
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
    gate(active || unresolved);
    rt_base_t level = rt_hw_interrupt_disable();
    cached = device.snapshot;
    occupied = active || unresolved || pending != CMD_NONE;
    rt_hw_interrupt_enable(level);
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
            uint32_t elapsed = now_ms() - device.snapshot.started_ms;
            uint32_t remaining = elapsed >= device.snapshot.duration_ms ? 1 : device.snapshot.duration_ms - elapsed;
            wait = rt_tick_from_millisecond(remaining < 1000 ? remaining : 1000);
            if (!wait) wait = 1;
        }
        rt_uint32_t events = 0;
        rt_event_recv(&work, EV_COMMAND | EV_STOP, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, wait, &events);
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

bool wristflow_product_imu_diag_start(void)
{
    if (ready) return true;
    if (rt_event_init(&work, "wf_imu", RT_IPC_FLAG_PRIO) != RT_EOK) return false;
    wf_imu_pedometer_init(&device, (wf_imu_io_t){NULL, read_reg, write_reg, io_clock, io_cancelled});
    rt_thread_t thread = rt_thread_create("wf_imu", worker, NULL, 2048, 24, 10);
    if (!thread) { rt_event_detach(&work); return false; }
    if (rt_thread_startup(thread) != RT_EOK) {
        rt_thread_delete(thread); rt_event_detach(&work); return false;
    }
    ready = true;
    return true;
}
