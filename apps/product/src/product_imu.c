/* C2: bounded IMU diagnostics plus interrupt-driven wrist wake when dim or off. */
#include "product_imu.h"
#include "imu_device.h"
#include "product_pm.h"
#include <rtdevice.h>
#include <board.h>
#include <stdlib.h>
#include <string.h>
#include "bf0_pm.h"
#include "drv_gpio.h"

#define IMU_PIN 31
#define EV_COMMAND 1u
#define EV_IRQ 2u
#define EV_STOP 4u
#define EV_STATUS 8u
#define EV_RUNTIME 16u
#define EV_ALL 31u

enum command { CMD_PROBE, CMD_SAMPLE, CMD_ARM };
static struct rt_event work;
static struct rt_i2c_bus_device *bus;
static wf_imu_device_t device;
static bool ready, pins_ready, configured, pending, busy;
static enum command requested;
static unsigned requested_count;
static uint8_t address;
static int8_t aon_pin = -1;
static volatile uint32_t irq_count;
static uint32_t io_errors, tilt_count;
static uint8_t last_source;
static volatile bool runtime_requested;
static bool runtime_armed, flat_seen;
static int16_t runtime_baseline[3];
static rt_tick_t runtime_scan_started;
static rt_tick_t runtime_candidate_started;

static bool read_register(void *context, uint8_t reg, uint8_t *bytes, size_t count)
{
    (void)context;
    bool ok = rt_i2c_mem_read(bus, address, reg, 8, bytes, count) == count;
    if (!ok) ++io_errors;
    return ok;
}

static bool write_register(void *context, uint8_t reg, uint8_t value)
{
    (void)context;
    bool ok = rt_i2c_mem_write(bus, address, reg, 8, &value, 1) == 1;
    if (!ok) ++io_errors;
    return ok;
}

static void delay_ms(void *context, unsigned ms)
{ (void)context; rt_thread_mdelay(ms); }

static void interrupt(void *context)
{
    (void)context;
    ++irq_count;
    rt_event_send(&work, EV_IRQ); /* No bus traffic, logging or LVGL in IRQ context. */
}

static bool disable_wake(void)
{
    rt_err_t gpio = RT_EOK, aon = RT_EOK;
    if (pins_ready) gpio = rt_pin_irq_enable(IMU_PIN, PIN_IRQ_DISABLE);
    if (aon_pin >= 0) aon = pm_disable_pin_wakeup((uint8_t)aon_pin);
    bool ok = gpio == RT_EOK && aon == RT_EOK;
    if (!ok) rt_kprintf("[imu-c1] wake disable FAILED gpio=%d aon=%d\n", gpio, aon);
    return ok;
}

static void stop_sensor(void)
{
    if (!disable_wake()) configured = false;
    if (device.identified) {
        bool ok = wf_imu_stop(&device);
        rt_kprintf("[imu-c1] stop=%s (MD1/function/accel/gyro readback)\n", ok ? "ok" : "FAILED");
        if (!ok) configured = false;
    }
    /* GS_3V3 is shared with other sensors: never power-cycle that rail here. */
}

static bool configure_bus(void)
{
    if (pins_ready) return true;
    bus = rt_i2c_bus_device_find("i2c3");
    if (!bus) { rt_kprintf("[imu-c1] missing i2c3\n"); return false; }
    HAL_PIN_Set(PAD_PA39, I2C3_SDA, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA40, I2C3_SCL, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA31, GPIO_A31, PIN_PULLDOWN, 1);
    rt_pin_mode(IMU_PIN, PIN_MODE_INPUT_PULLDOWN);
    /* Only enable the existing shared sensor supply; do not cut it on exit. */
    BSP_GPIO_Set(30, 1, 1);
    rt_thread_mdelay(10);
    struct rt_i2c_configuration cfg = {0, 0, 50, 100000};
    if (rt_i2c_configure(bus, &cfg) != RT_EOK) return false;
    if (rt_pin_attach_irq(IMU_PIN, PIN_IRQ_MODE_RISING, interrupt, NULL) != RT_EOK) return false;
    aon_pin = HAL_HPAON_QueryWakeupPin(GET_GPIO_INSTANCE(IMU_PIN), GET_GPIOx_PIN(IMU_PIN));
    pins_ready = true;
    device = (wf_imu_device_t){read_register, write_register, delay_ms, NULL, false};
    rt_kprintf("[imu-c1] i2c3 PA39/40 100kHz timeout=50; INT1=PA31 aon=%d\n", aon_pin);
    return true;
}

static bool probe(void)
{
    configured = false;
    if (!configure_bus()) return false;
    if (!disable_wake()) return false;
    uint8_t selected = 0;
    unsigned found = 0;
    /* SA0 is grounded in V1.2. Also inspect the alternate address without writing. */
    for (address = 0x6a; address <= 0x6b; ++address) {
        uint8_t id;
        bool identified = wf_imu_identify(&device, &id);
        rt_kprintf("[imu-c1] addr=0x%02x WHO_AM_I=0x%02x expected=0x6a match=%u\n", address, id, identified);
        if (identified) { selected = address; ++found; }
    }
    device.identified = false;
    if (found != 1) {
        rt_kprintf("[imu-c1] probe rejected: matching devices=%u; no configuration written\n", found);
        return false;
    }
    address = selected;
    configured = wf_imu_prepare(&device);
    rt_kprintf("[imu-c1] prepare=%s; ID alone does not distinguish every LSM6 variant\n",
        configured ? "ok" : "FAILED");
    return configured;
}

static void report(void)
{
    rt_kprintf("[imu-c1] configured=%u addr=0x%02x irq=%u tilt=%u io_errors=%u src=0x%02x\n",
        configured, address, irq_count, tilt_count, io_errors, last_source);
    if (!pins_ready) return;
    rt_kprintf("[imu-c1] pin31=%d gs_en=%d vsys_en=%d tick=%u\n",
        rt_pin_read(IMU_PIN), rt_pin_read(30), rt_pin_read(38), rt_tick_get());
    if (!device.identified) return;
    const uint8_t regs[] = {WF_IMU_WHO, WF_IMU_XL, WF_IMU_G, WF_IMU_CTRL3,
        WF_IMU_CTRL6, WF_IMU_CTRL10, WF_IMU_TAP, WF_IMU_MD1};
    for (unsigned i = 0; i < sizeof regs; ++i) {
        uint8_t value;
        if (read_register(NULL, regs[i], &value, 1))
            rt_kprintf("[imu-c1] reg[0x%02x]=0x%02x\n", regs[i], value);
        else rt_kprintf("[imu-c1] reg[0x%02x]=READ_FAILED\n", regs[i]);
    }
}

static int sample(void)
{
    int16_t axes[3];
    int result = wf_imu_accel(&device, axes);
    if (result == 1) {
        /* +/-2 g sensitivity is 0.061 mg/LSB. Preserve raw values for axis mapping. */
        rt_kprintf("[imu-c1] tick=%u raw=%d,%d,%d mg=%d,%d,%d\n", rt_tick_get(),
            axes[0], axes[1], axes[2], axes[0]*61/1000, axes[1]*61/1000, axes[2]*61/1000);
    }
    return result;
}

static void experiment(enum command cmd, unsigned count)
{
    rt_pm_request(PM_SLEEP_MODE_IDLE);
    if (!configured || !(cmd == CMD_ARM ? wf_imu_tilt_mode(&device) : wf_imu_sample_mode(&device))) {
        rt_kprintf("[imu-c1] rejected; run wf_imu probe first or inspect I2C error\n");
        stop_sensor();
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        return;
    }
    bool armed = cmd == CMD_ARM;
    if (armed && (aon_pin < 0 ||
        rt_pin_irq_enable(IMU_PIN, PIN_IRQ_ENABLE) != RT_EOK ||
        pm_enable_pin_wakeup((uint8_t)aon_pin, AON_PIN_MODE_POS_EDGE) != RT_EOK)) {
        rt_kprintf("[imu-c1] cannot arm PA31/AON; stopping\n");
        stop_sensor();
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        return;
    }
    rt_tick_t start = rt_tick_get();
    rt_tick_t duration = rt_tick_from_millisecond(armed ? count * 1000 : count * 100 + 1000);
    unsigned samples = 0, events_seen = 0;
    uint32_t first_irq = irq_count;
    rt_kprintf("[imu-c1] %s=%u accel=26Hz gyro=off; no screen wake\n", armed ? "arm_seconds" : "samples", count);
    if (armed && rt_pin_read(IMU_PIN)) rt_event_send(&work, EV_IRQ);
    rt_pm_release(PM_SLEEP_MODE_IDLE);
    for (;;) {
        rt_tick_t elapsed = rt_tick_get() - start;
        if (elapsed >= duration) break;
        rt_int32_t wait = (rt_int32_t)(duration - elapsed);
        if (!armed && wait > (rt_int32_t)rt_tick_from_millisecond(100)) wait = rt_tick_from_millisecond(100);
        rt_uint32_t event = 0;
        rt_event_recv(&work, EV_IRQ | EV_STOP | EV_STATUS,
            RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, wait, &event);
        rt_pm_request(PM_SLEEP_MODE_IDLE);
        bool finish = (event & EV_STOP) != 0;
        if (event & EV_STATUS) report();
        if (!finish && armed && (event & EV_IRQ)) {
            if (!read_register(NULL, WF_IMU_FUNC, &last_source, 1)) finish = true;
            else {
                if (last_source & 0x20) ++tilt_count;
                rt_kprintf("[imu-c1] IRQ src=0x%02x pin=%d count=%u tick=%u\n",
                    last_source, rt_pin_read(IMU_PIN), irq_count, rt_tick_get());
                if (sample() < 0) finish = true;
                wristflow_product_event_send(WF_EVENT_PM_SAMPLE);
                if (++events_seen >= 32 || irq_count - first_irq >= 32) finish = true;
            }
        }
        if (!finish && !armed) {
            int result = sample();
            if (result < 0 || (result == 1 && ++samples >= count)) finish = true;
        }
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        if (finish) break;
    }
    rt_pm_request(PM_SLEEP_MODE_IDLE);
    stop_sensor();
    rt_kprintf("[imu-c1] experiment ended samples=%u irq_delta=%u elapsed_ticks=%u\n",
        samples, irq_count - first_irq, rt_tick_get() - start);
    wristflow_product_event_send(WF_EVENT_PM_SAMPLE);
    rt_pm_release(PM_SLEEP_MODE_IDLE);
}

static bool runtime_start(void)
{
    if (!runtime_requested || runtime_armed) return true;
    if (!configured && !probe()) return false;
    if (!wf_imu_tilt_mode(&device) || aon_pin < 0 ||
        rt_pin_irq_enable(IMU_PIN, PIN_IRQ_ENABLE) != RT_EOK ||
        pm_enable_pin_wakeup((uint8_t)aon_pin, AON_PIN_MODE_POS_EDGE) != RT_EOK) {
        stop_sensor();
        return false;
    }
    int16_t baseline[3];
    int sample_result = 0;
    for (unsigned attempt = 0; attempt < 4 && sample_result == 0; ++attempt) {
        rt_thread_mdelay(40);
        sample_result = wf_imu_accel(&device, baseline);
    }
    if (sample_result != 1) {
        rt_kprintf("[imu-c2] wrist wake baseline unavailable result=%d\n", sample_result);
        stop_sensor();
        return false;
    }
    runtime_baseline[0] = baseline[0];
    runtime_baseline[1] = baseline[1];
    runtime_baseline[2] = baseline[2];
    runtime_armed = true;
    runtime_scan_started = 0;
    runtime_candidate_started = 0;
    /* Establish the screen-up gravity baseline before accepting a lift. */
    flat_seen = baseline[2] < -12000 &&
        abs((int)baseline[0]) < 8000 && abs((int)baseline[1]) < 8000;
    rt_kprintf("[imu-c2] wrist wake armed baseline=%d,%d,%d flat=%u direction=negative-X\n",
        baseline[0], baseline[1], baseline[2], flat_seen);
    return true;
}

static bool runtime_sample(void)
{
    if (!runtime_armed || !runtime_requested) return false;
    rt_tick_t now = rt_tick_get();
    if (now - runtime_scan_started >= rt_tick_from_millisecond(1500)) {
        runtime_scan_started = 0;
        runtime_candidate_started = 0;
        return false;
    }
    int16_t axes[3];
    if (wf_imu_accel(&device, axes) != 1) return false;
    int32_t x = axes[0], y = axes[1], z = axes[2];
    if (z < -12000 && abs((int)x) < 8000 && abs((int)y) < 8000) {
        runtime_baseline[0] = axes[0];
        runtime_baseline[1] = axes[1];
        runtime_baseline[2] = axes[2];
        flat_seen = true;
    }
    bool departed = abs((int)(x - runtime_baseline[0])) > 6000 ||
        abs((int)(y - runtime_baseline[1])) > 6000 ||
        abs((int)(z - runtime_baseline[2])) > 6000;
    /* The confirmed wrist pose is the short edge down, screen facing the user: -X. */
    bool vertical = x < -12000 && abs((int)y) < 8000 && abs((int)z) < 7000;
    if (!flat_seen || !departed) {
        runtime_candidate_started = 0;
        return false;
    }
    if (!vertical) {
        runtime_candidate_started = 0;
        return false;
    }
    if (!runtime_candidate_started) {
        runtime_candidate_started = now;
        rt_kprintf("[imu-c2] wrist wake candidate raw=%d,%d,%d\n", axes[0], axes[1], axes[2]);
        return false;
    }
    if (now - runtime_candidate_started < rt_tick_from_millisecond(500)) return false;
    rt_kprintf("[imu-c2] wrist wake trigger raw=%d,%d,%d hold_ms=%u\n",
        axes[0], axes[1], axes[2], (unsigned)((now - runtime_candidate_started) * 1000u / RT_TICK_PER_SECOND));
    runtime_armed = false;
    runtime_scan_started = 0;
    stop_sensor();
    wristflow_product_event_send(WF_EVENT_IMU_WAKE);
    return true;
}

static void worker(void *context)
{
    (void)context;
    for (;;) {
        rt_uint32_t event = 0;
        rt_int32_t wait = runtime_scan_started ? (rt_int32_t)rt_tick_from_millisecond(40) : RT_WAITING_FOREVER;
        rt_event_recv(&work, EV_ALL, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, wait, &event);
        rt_base_t level = rt_hw_interrupt_disable();
        enum command cmd = requested;
        unsigned count = requested_count;
        bool execute = (event & EV_COMMAND) && pending;
        if (execute) { pending = false; busy = true; }
        bool runtime_change = (event & EV_RUNTIME) != 0;
        bool want_runtime = runtime_requested;
        rt_hw_interrupt_enable(level);
        rt_pm_request(PM_SLEEP_MODE_IDLE);
        if (runtime_change) {
            if (want_runtime) (void)runtime_start();
            else {
                runtime_armed = false;
                runtime_scan_started = 0;
                runtime_candidate_started = 0;
                stop_sensor();
            }
        }
        if ((event & EV_IRQ) && runtime_armed &&
            read_register(NULL, WF_IMU_FUNC, &last_source, 1) &&
            (last_source & 0x20) && !runtime_scan_started)
            runtime_scan_started = rt_tick_get();
        if (runtime_scan_started && runtime_armed) (void)runtime_sample();
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        if (execute && runtime_armed) {
            rt_kprintf("[imu-c2] diagnostic rejected: wrist wake active; wf_imu stop first\n");
            execute = false;
        }
        if (execute && !(event & EV_STOP) && cmd != CMD_PROBE) experiment(cmd, count);
        rt_pm_request(PM_SLEEP_MODE_IDLE);
        if (execute && !(event & EV_STOP) && cmd == CMD_PROBE) { probe(); report(); }
        if (event & EV_STOP) {
            runtime_armed = false;
            runtime_scan_started = 0;
            runtime_candidate_started = 0;
            stop_sensor();
        }
        if (event & EV_STATUS) report();
        rt_pm_release(PM_SLEEP_MODE_IDLE);
        level = rt_hw_interrupt_disable();
        busy = false;
        rt_hw_interrupt_enable(level);
    }
}

void wristflow_product_imu_wrist_wake(bool enabled)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool changed = runtime_requested != enabled;
    runtime_requested = enabled;
    if (ready && changed) (void)rt_event_send(&work, EV_RUNTIME);
    rt_hw_interrupt_enable(level);
}

void wristflow_product_imu_start(void)
{
    RT_ASSERT(rt_event_init(&work, "wf_imu", RT_IPC_FLAG_FIFO) == RT_EOK);
    rt_thread_t thread = rt_thread_create("wf_imu", worker, NULL, 2048, 22, 10);
    if (!thread) { rt_kprintf("[imu-c1] diagnostic thread unavailable\n"); return; }
    ready = rt_thread_startup(thread) == RT_EOK;
    rt_kprintf("[imu-c1] diagnostic ready=%u; wf_imu probe; sensor idle until requested\n", ready);
}

static int wf_imu(int argc, char **argv)
{
    if (!ready) return -RT_ERROR;
    if (argc == 2 && (!strcmp(argv[1], "status") || !strcmp(argv[1], "stop")))
        return rt_event_send(&work, !strcmp(argv[1], "stop") ? EV_STOP : EV_STATUS);
    enum command cmd;
    unsigned count = 0;
    if (argc == 2 && !strcmp(argv[1], "probe")) cmd = CMD_PROBE;
    else if (argc == 3 && (!strcmp(argv[1], "sample") || !strcmp(argv[1], "arm"))) {
        char *end;
        unsigned long parsed = strtoul(argv[2], &end, 10);
        cmd = !strcmp(argv[1], "arm") ? CMD_ARM : CMD_SAMPLE;
        if (!*argv[2] || *end || parsed < 1 || parsed > (cmd == CMD_ARM ? 120u : 100u)) return -RT_EINVAL;
        count = (unsigned)parsed;
    } else {
        rt_kprintf("wf_imu probe | status | sample 1..100 | arm 1..120(seconds) | stop\n");
        return -RT_EINVAL;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    if (busy || pending) { rt_hw_interrupt_enable(level); return -RT_EBUSY; }
    requested = cmd; requested_count = count; pending = true;
    rt_hw_interrupt_enable(level);
    return rt_event_send(&work, EV_COMMAND);
}
MSH_CMD_EXPORT(wf_imu, Bounded IMU diagnostics and C2 wrist wake);
