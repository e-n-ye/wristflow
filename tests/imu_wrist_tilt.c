#ifdef NDEBUG
#undef NDEBUG
#endif
#include "imu_wrist_tilt.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t user[256], bank_b[256], bank;
    uint16_t native;
    uint32_t now;
    unsigned reads, writes, resets, wrong_user_writes;
    int fail_reg;
    bool fail_restore, cancel, cancel_in_delay, expire_in_delay;
} bus_t;
static bool read_reg(void *context, uint8_t reg, uint8_t *out, size_t length)
{
    bus_t *b = context;
    assert(length == 1); ++b->reads;
    *out = reg == 1 ? b->bank : b->bank ? b->bank_b[reg] : b->user[reg];
    if (!b->bank && reg == 0x54) b->user[0x54] = b->user[0x55] = 0;
    return true;
}
static bool write_reg(void *context, uint8_t reg, uint8_t value)
{
    bus_t *b = context;
    ++b->writes;
    if ((int)reg == b->fail_reg || (reg == 1 && !value && b->fail_restore)) return false;
    if (reg == 1) b->bank = value;
    else if (b->bank) {
        assert(b->bank == 0xa0);
        if (reg != 0x50 && reg != 0x54 && reg != 0x59) ++b->wrong_user_writes;
        b->bank_b[reg] = value;
    } else {
        b->user[reg] = value;
        if (reg == 0x19 && (value & 2U)) { ++b->resets; b->native = 0; }
    }
    return true;
}
static uint32_t clock_ms(void *context) { return ((bus_t *)context)->now; }
static bool cancelled(void *context) { return ((bus_t *)context)->cancel; }
static void delay_ms(void *context, uint32_t delay)
{
    bus_t *b = context;
    assert(delay == 50);
    b->now += b->expire_in_delay ? 2000 : delay;
    if (b->cancel_in_delay) b->cancel = true;
}
static void init(bus_t *b, wf_imu_wrist_tilt_t *d)
{
    memset(b, 0, sizeof *b);
    b->fail_reg = -1; b->native = 123;
    b->user[0x10] = 0x20; b->user[0x19] = 0x14;
    b->user[0x0b] = 0x80; b->user[0x58] = 0x80;
    wf_imu_wrist_init(d, (wf_imu_io_t){b, read_reg, write_reg, clock_ms, cancelled}, delay_ms);
}
static void preserved(const bus_t *b)
{
    assert(!b->bank && b->user[0x10] == 0x20 && b->user[0x19] == 0x14);
    assert(b->user[0x0b] == 0x80 && b->user[0x58] == 0x80);
    assert(b->native == 123 && !b->resets && !b->wrong_user_writes);
}
int main(void)
{
    bus_t b; wf_imu_wrist_tilt_t d;
    init(&b, &d);
    assert(!wf_imu_wrist_begin(&d, 999, 0x40, 15));
    assert(!wf_imu_wrist_begin(&d, 120001, 0x40, 15));
    assert(!wf_imu_wrist_begin(&d, 1000, 1, 15));
    assert(!wf_imu_wrist_begin(&d, 1000, 0x40, 0) && !b.writes);
    assert(wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    assert(d.snapshot.phase == WF_IMU_RUNNING && d.snapshot.configured && !d.snapshot.stop_verified);
    assert(b.bank_b[0x50] == 15 && b.bank_b[0x54] == 32 && b.bank_b[0x59] == 0x40);
    assert(!b.bank && b.user[0x19] == 0x94 && b.user[0x0b] == 0x81 && b.user[0x58] == 0x81);
    uint32_t start = d.snapshot.started_ms;
    assert(!wf_imu_wrist_begin(&d, 120000, 0x80, 1) && d.snapshot.started_ms == start);
    b.user[0x54] = 1; b.user[0x55] = 0x40; b.now = 100;
    wf_imu_wrist_poll(&d, b.now);
    assert(d.snapshot.source_valid && d.snapshot.source == 1 && d.snapshot.axes == 0x40);
    assert(d.snapshot.observations == 1 && !b.user[0x54]);
    b.now = 200; wf_imu_wrist_poll(&d, b.now);
    assert(d.snapshot.observations == 1 && d.snapshot.source_valid && !d.snapshot.source);
    b.now = start + 1000; wf_imu_wrist_poll(&d, b.now);
    assert(d.snapshot.phase == WF_IMU_DONE && d.snapshot.stop_verified && !d.snapshot.source_valid);
    preserved(&b);
    unsigned io = b.reads + b.writes;
    wf_imu_wrist_poll(&d, 5000); assert(io == b.reads + b.writes);

    init(&b, &d); b.now = UINT32_MAX - 500;
    assert(wf_imu_wrist_begin(&d, 1000, 0x80, 5));
    b.now = 499; wf_imu_wrist_poll(&d, b.now);
    assert(d.snapshot.phase == WF_IMU_DONE); preserved(&b);
    init(&b, &d); b.cancel_in_delay = true;
    assert(!wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    assert(d.snapshot.ended == WF_IMU_USER_STOP && d.snapshot.stop_verified); preserved(&b);
    init(&b, &d); b.expire_in_delay = true;
    assert(!wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    assert(d.snapshot.ended == WF_IMU_DEADLINE && d.snapshot.stop_verified); preserved(&b);
    init(&b, &d); b.fail_reg = 0x59;
    assert(!wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.stop_verified); preserved(&b);
    b.fail_reg = -1;
    assert(wf_imu_wrist_stop(&d, b.now, WF_IMU_USER_STOP, true));
    assert(wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    b.user[0x19] = 0x14; b.now += 100; wf_imu_wrist_poll(&d, b.now);
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.stop_verified); preserved(&b);
    init(&b, &d); b.fail_restore = true;
    assert(!wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    assert(d.snapshot.phase == WF_IMU_FAULT && !d.snapshot.stop_verified && b.bank == 0xa0);
    assert(!b.wrong_user_writes && d.changed);
    b.fail_restore = false;
    assert(wf_imu_wrist_stop(&d, b.now, WF_IMU_USER_STOP, true)); preserved(&b);
    init(&b, &d); b.user[0x19] = 0x94;
    assert(!wf_imu_wrist_begin(&d, 1000, 0x40, 15) && !b.writes);
    assert(d.snapshot.phase == WF_IMU_FAULT && d.snapshot.stop_verified);
    init(&b, &d); assert(wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    assert(wf_imu_wrist_stop(&d, b.now, WF_IMU_USER_STOP, false));
    assert(b.user[0x19] == 0 && b.user[0x10] == 0x20 && b.native == 123 && !b.resets);
    return 0;
}
