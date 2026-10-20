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
    int fail_read_reg, cancel_read_reg;
    uint32_t read_ms;
    bool fail_restore, cancel, cancel_in_delay, expire_in_delay;
    unsigned raw_reads, source_reads, pin_reads;
    bool fail_pin;
} bus_t;
static bool read_reg(void *context, uint8_t reg, uint8_t *out, size_t length)
{
    bus_t *b = context;
    assert(length == 1 || (reg == 0x28 && length == 6)); ++b->reads;
    b->now += b->read_ms;
    if ((int)reg == b->cancel_read_reg) b->cancel = true;
    if ((int)reg == b->fail_read_reg) return false;
    if (length == 6) {
        assert(!b->bank); ++b->raw_reads;
        memcpy(out, b->user + reg, length);
        b->user[0x1e] &= (uint8_t)~1U;
        return true;
    }
    *out = reg == 1 ? b->bank : b->bank ? b->bank_b[reg] : b->user[reg];
    if (!b->bank && reg == 0x54) { ++b->source_reads; b->user[0x54] = b->user[0x55] = 0; }
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
static int pin_read(void *context)
{
    bus_t *b = context; ++b->pin_reads;
    return b->fail_pin ? -1 : !!((b->user[0x13] & 0x20U) && (b->user[0x54] & 1U));
}
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
    b->fail_reg = b->fail_read_reg = b->cancel_read_reg = -1; b->native = 123;
    b->user[0x0f] = 0x6a; b->user[0x12] = 0x44;
    b->user[0x10] = 0x20; b->user[0x19] = 0x14;
    b->user[0x0b] = 0x80; b->user[0x58] = 0x80;
    b->user[0x13] = 0x08; /* Preserve an unrelated CTRL4 bit. */
    wf_imu_wrist_init(d, (wf_imu_io_t){b, read_reg, write_reg, clock_ms, cancelled}, delay_ms);
}
static void preserved(const bus_t *b)
{
    assert(!b->bank && b->user[0x10] == 0x20 && b->user[0x19] == 0x14);
    assert(b->user[0x0b] == 0x80 && b->user[0x58] == 0x80);
    assert(b->user[0x13] == 0x08);
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
    /* Distinct new-read phases; byte-pair decoding, freshness and source clears.
     * XYZ input includes negative/full-scale values to catch endian/sign bugs. */
    init(&b, &d);
    const uint8_t raw[] = {0x00,0x80,0xff,0x7f,0x00,0xc0};
    memcpy(b.user+0x28, raw, sizeof raw); b.user[0x1e]=1; b.user[0x54]=0x84;
    assert(wf_imu_wrist_begin(&d, 1000, 0x40, 15));
    assert(d.snapshot.before.valid==WF_WRIST_ALL && d.snapshot.before.xyz_valid && d.snapshot.before.data_ready);
    assert(d.snapshot.before.source==0x84 && d.snapshot.before.xyz[0]==-32768 &&
        d.snapshot.before.xyz[1]==32767 && d.snapshot.before.xyz[2]==-16384);
    assert(!d.snapshot.running.attempted && !d.snapshot.after.attempted && !d.snapshot.observations);
    b.now=100; b.user[0x54]=1; b.user[0x55]=0x40; b.user[0x28]=1;
    unsigned sources=b.source_reads; wf_imu_wrist_poll(&d,b.now);
    assert(b.source_reads==sources+1 && d.snapshot.observations==1);
    assert(d.snapshot.running.valid==WF_WRIST_ALL && d.snapshot.running.xyz_valid && !d.snapshot.running.data_ready);
    assert(d.snapshot.running.ctrl10==0x94 && d.snapshot.running.source==1 && d.snapshot.running.axes==0x40);
    b.user[0x54]=0x80; b.user[0x55]=0x04; b.user[0x29]=0x40;
    assert(wf_imu_wrist_stop(&d,b.now,WF_IMU_USER_STOP,true));
    assert(d.snapshot.after.valid==WF_WRIST_ALL && d.snapshot.after.ctrl10==0x14 &&
        d.snapshot.after.source==0 && d.snapshot.after.xyz[0]==16385);
    assert(d.snapshot.source==1 && d.snapshot.running.source==1 && d.snapshot.before.source==0x84);
    io=b.reads; assert(wf_imu_wrist_stop(&d,b.now,WF_IMU_USER_STOP,true));
    assert(io==b.reads); /* Idempotent stop preserves the original after frame. */
    init(&b,&d); b.bank=0xa0;
    assert(!wf_imu_wrist_begin(&d,1000,0x40,15));
    assert(b.reads==1 && !b.writes && d.snapshot.before.valid==WF_WRIST_BANK && !b.raw_reads);
    init(&b,&d); b.fail_read_reg=0x28;
    assert(!wf_imu_wrist_begin(&d,1000,0x40,15) && !b.writes && d.snapshot.stop_verified);
    assert(d.snapshot.before.valid==(WF_WRIST_ALL & ~WF_WRIST_RAW) && !d.snapshot.before.xyz_valid);
    init(&b,&d); assert(wf_imu_wrist_begin(&d,1000,0x40,15));
    b.fail_read_reg=0x28; b.now=100; wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.phase==WF_IMU_FAULT && d.snapshot.stop_verified && !d.changed);
    assert(d.snapshot.running.valid==(WF_WRIST_ALL & ~WF_WRIST_RAW) && !d.snapshot.running.xyz_valid);
    assert(d.snapshot.after.valid==(WF_WRIST_ALL & ~WF_WRIST_RAW) && !d.snapshot.after.xyz_valid);
    preserved(&b);
    init(&b,&d); assert(wf_imu_wrist_begin(&d,1000,0x40,15));
    b.user[0x12]=0x46; b.now=100; wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.phase==WF_IMU_FAULT && d.snapshot.stop_verified && !d.snapshot.running.xyz_valid);
    init(&b,&d); assert(wf_imu_wrist_begin(&d,1000,0x40,15));
    b.fail_read_reg=0x28;
    assert(wf_imu_wrist_stop(&d,b.now,WF_IMU_USER_STOP,true));
    assert(d.snapshot.phase==WF_IMU_READY && d.snapshot.stop_verified && !d.changed && d.snapshot.io_errors==1);
    assert(d.snapshot.after.attempted && d.snapshot.after.valid==(WF_WRIST_ALL & ~WF_WRIST_RAW));
    preserved(&b);
    init(&b,&d); assert(wf_imu_wrist_begin(&d,1000,0x40,15));
    b.user[0x54]=1; b.read_ms=100;
    wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.phase==WF_IMU_DONE && d.snapshot.ended==WF_IMU_DEADLINE && d.snapshot.stop_verified);
    assert(d.snapshot.running.source==1 && !d.snapshot.observations && !d.snapshot.source_valid);
    assert(d.snapshot.running.completed_ms-d.snapshot.running.started_ms==1200);
    preserved(&b);
    init(&b,&d); assert(wf_imu_wrist_begin(&d,1000,0x40,15));
    b.user[0x54]=1; b.cancel_read_reg=0x28;
    wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.phase==WF_IMU_READY && d.snapshot.ended==WF_IMU_USER_STOP && d.snapshot.stop_verified);
    assert(d.snapshot.running.source==1 && !d.snapshot.observations && !d.snapshot.source_valid);
    preserved(&b);
    /* Explicit INT1 routing preserves CTRL4, records physical levels bracketing
     * the clearing SOURCE read, and retains the event after later empty polls. */
    init(&b,&d);
    assert(!wf_imu_wrist_route_begin(&d,1000,0x40,15) && !b.writes);
    d.read_int1=pin_read;
    assert(wf_imu_wrist_route_begin(&d,1000,0x40,15));
    assert(d.snapshot.route_int1 && b.user[0x13]==0x28 && d.snapshot.before.ctrl4==0x08);
    assert(d.snapshot.before.pin_valid==3 && !d.snapshot.before.pin_before && !d.snapshot.before.pin_after);
    b.user[0x54]=0x85; b.now+=100; wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.observations==1 && d.snapshot.event.source==0x85);
    assert(d.snapshot.event.ctrl4==0x28 && d.snapshot.event.pin_valid==3);
    assert(d.snapshot.event.pin_before==1 && d.snapshot.event.pin_after==0);
    assert(d.snapshot.event.pin_before_ms<=d.snapshot.event.pin_after_ms);
    b.now+=100; wf_imu_wrist_poll(&d,b.now);
    assert(!d.snapshot.running.pin_before && d.snapshot.event.pin_before==1 && d.snapshot.observations==1);
    b.now=d.snapshot.started_ms+1000; wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.phase==WF_IMU_DONE && d.snapshot.after.ctrl4==0x08 && d.snapshot.after.pin_valid==3);
    assert(d.snapshot.event.source==0x85 && !d.changed); preserved(&b);
    init(&b,&d); d.read_int1=pin_read; b.fail_pin=true;
    assert(!wf_imu_wrist_route_begin(&d,1000,0x40,15) && !b.writes && d.snapshot.stop_verified);
    assert(!d.snapshot.before.pin_valid && d.snapshot.io_errors==2);
    init(&b,&d); d.read_int1=pin_read; b.cancel_in_delay=true;
    assert(!wf_imu_wrist_route_begin(&d,1000,0x40,15));
    assert(d.snapshot.ended==WF_IMU_USER_STOP && d.snapshot.stop_verified); preserved(&b);
    init(&b,&d); d.read_int1=pin_read; b.fail_reg=0x13;
    assert(!wf_imu_wrist_route_begin(&d,1000,0x40,15));
    assert(d.snapshot.phase==WF_IMU_FAULT && !d.snapshot.stop_verified && d.changed);
    b.fail_reg=-1; assert(wf_imu_wrist_stop(&d,b.now,WF_IMU_USER_STOP,true)); preserved(&b);
    init(&b,&d); d.read_int1=pin_read;
    assert(wf_imu_wrist_route_begin(&d,1000,0x40,15));
    b.user[0x13]=0x08; wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.phase==WF_IMU_FAULT && d.snapshot.stop_verified); preserved(&b);
    init(&b,&d); d.read_int1=pin_read;
    assert(wf_imu_wrist_route_begin(&d,1000,0x40,15)); b.fail_pin=true;
    wf_imu_wrist_poll(&d,b.now);
    assert(d.snapshot.phase==WF_IMU_FAULT && d.snapshot.stop_verified && !d.changed);
    assert(!d.snapshot.running.pin_valid && !d.snapshot.after.pin_valid); preserved(&b);
    init(&b,&d); d.read_int1=pin_read;
    assert(wf_imu_wrist_route_begin(&d,1000,0x40,15)); b.fail_reg=0x13;
    assert(!wf_imu_wrist_stop(&d,b.now,WF_IMU_USER_STOP,true) && d.changed);
    assert(b.user[0x19]==0x14 && !d.snapshot.stop_verified);
    b.fail_reg=-1; assert(wf_imu_wrist_stop(&d,b.now,WF_IMU_USER_STOP,true)); preserved(&b);
    const uint8_t routes[]={0x0d,0x0e,0x5e,0x5f};
    for (unsigned i=0;i<sizeof routes;++i) {
        init(&b,&d); d.read_int1=pin_read; b.user[routes[i]]=1;
        assert(!wf_imu_wrist_route_begin(&d,1000,0x40,15) && !b.writes && d.snapshot.stop_verified);
        assert(d.snapshot.before.valid==WF_WRIST_ROUTE_ALL);
        init(&b,&d); d.read_int1=pin_read;
        assert(wf_imu_wrist_route_begin(&d,1000,0x40,15));
        b.user[routes[i]]=1; b.user[0x54]=1; wf_imu_wrist_poll(&d,b.now);
        assert(d.snapshot.phase==WF_IMU_FAULT && d.snapshot.stop_verified && !d.snapshot.observations);
        assert(!d.snapshot.event.attempted && b.user[routes[i]]==1); preserved(&b);
        init(&b,&d); d.read_int1=pin_read; b.fail_read_reg=routes[i];
        assert(!wf_imu_wrist_route_begin(&d,1000,0x40,15) && !b.writes);
        assert(d.snapshot.before.valid==(WF_WRIST_ROUTE_ALL & ~(WF_WRIST_INT1<<i)));
    }
    return 0;
}
