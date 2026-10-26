#ifdef NDEBUG
#undef NDEBUG
#endif
#include "imu_pedometer.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t user[256], bank_a[256], bank;
    uint16_t count;
    unsigned reads, writes, resets, stop_writes;
    int fail_write;
    int fail_read, read_failures;
    unsigned fail_read_skip, retry_delays;
    uint16_t retry_steps;
    bool cancel_retry, lose_config_retry;
    uint32_t now, active_delay;
    unsigned enabled;
    bool rollover, unstable, cancelled, cancel_during_setup, mismatch_restore_once;
} bus_t;
static bool read_reg(void *context, uint8_t reg, uint8_t *data, size_t length)
{
    bus_t *b = context;
    assert(length == 1);
    ++b->reads;
    if (reg == b->fail_read) {
        if (b->fail_read_skip) --b->fail_read_skip;
        else if (b->read_failures) {
            if (b->read_failures > 0) --b->read_failures;
            return false;
        }
    }
    if (b->user[0x10]) b->now += b->active_delay;
    if (reg == 1) *data = b->bank;
    else if (!b->bank && reg == 0x4c) {
        *data = (uint8_t)(b->count >> 8);
        if (b->unstable) b->count += 256;
    } else if (!b->bank && reg == 0x4b) {
        if (b->rollover) { b->count = 256; b->rollover = false; }
        *data = (uint8_t)b->count;
    } else *data = b->bank ? b->bank_a[reg] : b->user[reg];
    return true;
}
static bool write_reg(void *context, uint8_t reg, uint8_t value)
{
    bus_t *b = context;
    ++b->writes;
    if (!value) ++b->stop_writes;
    if (reg == 0x12 && b->cancel_during_setup) b->cancelled = true;
    if (reg == 0x19 && value == 0x14) ++b->enabled;
    if (reg == b->fail_write) return false;
    if (reg == 1) {
        b->bank = value;
        if (!value && b->cancelled && b->mismatch_restore_once) {
            b->bank = 0x80; b->mismatch_restore_once = false;
        }
    }
    else if (b->bank) b->bank_a[reg] = value;
    else {
        b->user[reg] = value;
        if (reg == 0x19 && (value & 2)) { b->count = 0; ++b->resets; }
    }
    return true;
}
static uint32_t clock_ms(void *context) { return ((bus_t *)context)->now; }
static bool cancelled(void *context) { return ((bus_t *)context)->cancelled; }
static void retry_delay(void *context, uint32_t ms)
{
    bus_t *b = context;
    assert(ms == 20);
    ++b->retry_delays; b->now += ms; b->count += b->retry_steps;
    if (b->cancel_retry) b->cancelled = true;
    if (b->lose_config_retry) b->user[0x10] = 0;
}
static void init(bus_t *b, wf_imu_pedometer_t *d)
{
    memset(b, 0, sizeof *b);
    b->user[0x0f] = 0x6a;
    b->fail_write = -1;
    b->fail_read = -1;
    wf_imu_pedometer_init(d, (wf_imu_io_t){b, read_reg, write_reg, clock_ms, cancelled});
}
static bool begin(bus_t *b, wf_imu_pedometer_t *d, uint32_t now, uint32_t span)
{ b->now = now; return wf_imu_pedometer_begin(d, now, span); }
static void stopped(const bus_t *b)
{
    assert(b->bank == 0);
    const uint8_t regs[] = {0x0d, 0x0e, 0x5e, 0x5f, 0x19, 0x10, 0x11};
    for (size_t i = 0; i < sizeof regs; ++i) assert(b->user[regs[i]] == 0);
}
static void transient_reads(void)
{
    /* Every transfer in the safe daily observation, including the second H. */
    const uint8_t regs[] = {1, 0x10, 0x19, 0x4c, 0x4b, 0x4c};
    for (unsigned i = 0; i < sizeof regs; ++i) {
        bus_t b; wf_imu_pedometer_t d;
        init(&b, &d); d.retry_delay_ms = retry_delay;
        assert(wf_imu_pedometer_probe(&d) && wf_imu_pedometer_begin_continuous(&d));
        b.now = 1000; b.count = 120; wf_imu_pedometer_poll(&d, b.now);
        unsigned writes = b.writes, resets = b.resets;
        b.now = 2000; b.fail_read = regs[i]; b.read_failures = 1;
        b.fail_read_skip = i == 5 ? 1 : 0; b.retry_steps = 3;
        wf_imu_pedometer_poll(&d, b.now);
        assert(d.snapshot.phase == WF_IMU_RUNNING && d.snapshot.counter_valid && d.snapshot.steps == 123);
        assert(b.writes == writes && b.resets == resets && b.retry_delays == 1);
        assert(d.snapshot.poll_retries == 1 && d.snapshot.poll_recoveries == 1 && d.snapshot.io_errors == 1);
        assert(d.snapshot.poll_failure == WF_IMU_POLL_TRANSFER && d.snapshot.failure_reg == regs[i]);
        assert(d.snapshot.failure_ms == 2000 && d.snapshot.sampled_ms == 2020);
        b.now = 3000; b.count = 124; wf_imu_pedometer_poll(&d, b.now);
        assert(d.snapshot.steps == 124 && d.snapshot.poll_retries == 1);
    }
    bus_t b; wf_imu_pedometer_t d;
    init(&b, &d); d.retry_delay_ms = retry_delay;
    assert(wf_imu_pedometer_probe(&d) && wf_imu_pedometer_begin_continuous(&d));
    b.now = 1000; b.count = 65535; wf_imu_pedometer_poll(&d, b.now);
    b.now = 2000; b.fail_read = 0x4b; b.read_failures = 1; b.retry_steps = 2;
    wf_imu_pedometer_poll(&d, b.now);
    assert(d.snapshot.phase == WF_IMU_RUNNING && d.snapshot.steps == 1 && b.resets == 1);
    b.now = UINT32_MAX - 10U; b.read_failures = 1; b.retry_steps = 0;
    wf_imu_pedometer_poll(&d, b.now);
    assert(d.snapshot.phase == WF_IMU_RUNNING && d.snapshot.sampled_ms == 9);
    /* Two transfer failures exhaust this observation's sole retry. */
    b.now = 100; b.fail_read = 0x10; b.read_failures = 2;
    wf_imu_pedometer_poll(&d, b.now);
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.stop_verified && b.resets == 1);
    assert(d.snapshot.poll_retries == 3 && d.snapshot.poll_recoveries == 2);
    unsigned reads = b.reads; wf_imu_pedometer_poll(&d, 1000); assert(b.reads == reads);
    /* Configuration loss and incoherent counters never qualify for I/O retry. */
    init(&b, &d); d.retry_delay_ms = retry_delay;
    assert(wf_imu_pedometer_probe(&d) && wf_imu_pedometer_begin_continuous(&d));
    b.user[0x10] = 0; wf_imu_pedometer_poll(&d, 1);
    assert(d.snapshot.phase == WF_IMU_FAULT && !b.retry_delays && d.snapshot.poll_failure == WF_IMU_POLL_CONFIG);
    init(&b, &d); d.retry_delay_ms = retry_delay;
    assert(wf_imu_pedometer_probe(&d) && wf_imu_pedometer_begin_continuous(&d));
    b.unstable = true; wf_imu_pedometer_poll(&d, 1);
    assert(d.snapshot.phase == WF_IMU_FAULT && !b.retry_delays && d.snapshot.poll_failure == WF_IMU_POLL_INCOHERENT);
    for (unsigned cancel = 0; cancel < 2; ++cancel) {
        init(&b, &d); d.retry_delay_ms = retry_delay;
        assert(wf_imu_pedometer_probe(&d) && wf_imu_pedometer_begin_continuous(&d));
        b.fail_read = 0x4b; b.read_failures = 1;
        b.cancel_retry = cancel; b.lose_config_retry = !cancel;
        wf_imu_pedometer_poll(&d, 1);
        assert(d.snapshot.poll_retries == 1 && !d.snapshot.poll_recoveries && b.resets == 1);
        assert(d.snapshot.phase == (cancel ? WF_IMU_READY : WF_IMU_FAULT));
        assert(d.snapshot.ended == (cancel ? WF_IMU_USER_STOP : WF_IMU_IO_FAILURE));
    }
    /* Explicit short diagnostics retain their original failure/cleanup rule. */
    init(&b, &d); d.retry_delay_ms = retry_delay;
    assert(wf_imu_pedometer_probe(&d) && begin(&b, &d, 0, 1000));
    b.fail_read = 0x4b; b.read_failures = 1; wf_imu_pedometer_poll(&d, 1);
    assert(d.snapshot.phase == WF_IMU_FAULT && !b.retry_delays);
}
int main(void)
{
    bus_t b;
    wf_imu_pedometer_t d;
    init(&b, &d);
    assert(d.snapshot.phase == WF_IMU_UNPROBED && !b.reads && !b.writes);
    assert(!begin(&b, &d, 0, 1000) && !b.writes);
    b.user[0x0f] = 0;
    assert(!wf_imu_pedometer_probe(&d) && !b.writes);
    b.user[0x0f] = 0x6a;
    assert(wf_imu_pedometer_probe(&d) && !b.writes);
    assert(!begin(&b, &d, 0, 999));
    assert(!begin(&b, &d, 0, 120001));
    assert(begin(&b, &d, 100, 1000));
    assert(b.bank == 0 && b.bank_a[0x0f] == 0x10 && b.bank_a[0x14] == 0x6e);
    assert(b.user[0x10] == 0x20 && b.user[0x19] == 0x14 && b.resets == 1);
    assert(b.user[0x0d] == 0 && b.user[0x0e] == 0 && b.user[0x11] == 0);
    assert(!begin(&b, &d, 500, 120000)); /* Does not extend deadline. */
    b.count = 255; b.rollover = true;
    wf_imu_pedometer_poll(&d, 1099);
    assert(d.snapshot.steps == 256 && d.snapshot.counter_valid);
    wf_imu_pedometer_poll(&d, 1100);
    assert(d.snapshot.phase == WF_IMU_DONE && d.snapshot.stop_verified);
    assert(d.snapshot.steps == 256 && d.snapshot.ended == WF_IMU_DEADLINE);
    stopped(&b);
    unsigned reads = b.reads, writes = b.writes;
    wf_imu_pedometer_poll(&d, 100000);
    assert(b.reads == reads && b.writes == writes);
    assert(begin(&b, &d, UINT32_MAX - 500, 1000));
    wf_imu_pedometer_poll(&d, 499);
    assert(d.snapshot.phase == WF_IMU_DONE); /* Unsigned deadline wrap. */

    init(&b, &d);
    assert(wf_imu_pedometer_probe(&d));
    b.fail_write = 0x14; /* Failure in bank A must still restore user bank. */
    assert(!begin(&b, &d, 0, 1000));
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.stop_verified);
    stopped(&b);
    b.fail_write = -1;
    assert(wf_imu_pedometer_stop(&d, 1, WF_IMU_USER_STOP));
    assert(d.snapshot.phase == WF_IMU_READY);
    assert(begin(&b, &d, 2, 120000));
    b.fail_write = 0x19;
    unsigned attempts = b.stop_writes;
    assert(!wf_imu_pedometer_stop(&d, 3, WF_IMU_USER_STOP));
    assert(b.stop_writes - attempts == 8); /* Bank + all seven stop writes. */
    assert(d.snapshot.phase == WF_IMU_FAULT && !d.snapshot.stop_verified);
    assert(!d.snapshot.counter_valid);
    writes = b.writes;
    assert(!begin(&b, &d, 4, 1000) && b.writes == writes);
    b.fail_write = -1;
    assert(wf_imu_pedometer_stop(&d, 5, WF_IMU_USER_STOP));
    stopped(&b);
    assert(begin(&b, &d, 10, 1000));
    b.unstable = true;
    wf_imu_pedometer_poll(&d, 11);
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.stop_verified);
    assert(!d.snapshot.counter_valid && d.snapshot.incoherent_reads >= 1);
    assert(d.snapshot.io_errors >= 1); /* Earlier injected failures remain visible. */

    init(&b, &d); assert(wf_imu_pedometer_probe(&d));
    b.cancel_during_setup = true;
    assert(!begin(&b, &d, 0, 1000));
    assert(!b.enabled && d.snapshot.ended == WF_IMU_USER_STOP && d.snapshot.stop_verified);
    stopped(&b);
    init(&b, &d); assert(wf_imu_pedometer_probe(&d));
    b.cancel_during_setup = true; b.fail_write = 0x12;
    assert(!begin(&b, &d, 0, 1000));
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.ended == WF_IMU_IO_FAILURE);
    assert(d.snapshot.stop_verified); /* Successful cleanup cannot erase the I/O fault. */
    stopped(&b);
    init(&b, &d); assert(wf_imu_pedometer_probe(&d));
    b.cancel_during_setup = b.mismatch_restore_once = true;
    assert(!begin(&b, &d, 0, 1000));
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.ended == WF_IMU_IO_FAILURE);
    assert(d.snapshot.io_errors == 1 && d.snapshot.stop_verified);
    stopped(&b);
    init(&b, &d); assert(wf_imu_pedometer_probe(&d));
    b.active_delay = 1200; /* First ODR readback crosses the actual live deadline. */
    assert(!begin(&b, &d, 5000, 1000));
    assert(!b.enabled && d.snapshot.ended == WF_IMU_DEADLINE && d.snapshot.stop_verified);
    stopped(&b);
    init(&b,&d); assert(wf_imu_pedometer_probe(&d));
    assert(wf_imu_pedometer_begin_continuous(&d));
    unsigned resets=b.resets;
    b.count=42; b.now=600000;
    wf_imu_pedometer_poll(&d,b.now);
    assert(d.continuous && d.snapshot.phase==WF_IMU_RUNNING && d.snapshot.steps==42);
    assert(b.resets==resets); /* Sampling never resets or restarts the native detector. */
    b.user[0x19]=0;
    wf_imu_pedometer_poll(&d,600001);
    assert(d.snapshot.phase==WF_IMU_FAULT && d.snapshot.stop_verified && !d.continuous);
    transient_reads();
    return 0;
}
