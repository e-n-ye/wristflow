#ifndef WRISTFLOW_TEST_MMC5603_FAKE_H
#define WRISTFLOW_TEST_MMC5603_FAKE_H
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef struct {
    uint32_t ms, otp_due, data_due;
    uint8_t id, status, factory[3], threshold[3], data[9];
    unsigned reads, writes, operations, resets, triggers, fail_at;
    int slow_reg;
    uint32_t read_delay_ms;
    bool reset_wait, measuring, selftest, selftest_fail, never_done, fail_reset;
    bool stuck_done, cancel, cancel_data, otp_missing;
    bool clear_selftest_sat_on_read;
    bool partial_data_failure;
} fake_mmc5603_t;
static void fake_mag_counts(fake_mmc5603_t *s, int32_t x, int32_t y, int32_t z)
{
    int32_t values[3] = {x,y,z};
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t value = (uint32_t)(values[i] + 524288);
        s->data[2*i] = (uint8_t)(value >> 12);
        s->data[2*i+1] = (uint8_t)(value >> 4);
        s->data[6+i] = (uint8_t)(value << 4);
    }
}
static void fake_mag_init(fake_mmc5603_t *s)
{
    *s = (fake_mmc5603_t){.id=0x10,.status=0x10,.factory={100,150,200},.slow_reg=-1};
    fake_mag_counts(s, 12345, -23456, 34567);
}
static bool fake_mag_read(fake_mmc5603_t *s, uint8_t reg, uint8_t *data, size_t size)
{
    ++s->reads;
    if (++s->operations == s->fail_at) return false;
    /* Controls/ODR/thresholds are write-only: a test must never invent a
     * successful readback of their configured values. */
    assert((reg==0 && size==9) || (reg==0x27 && size==3) ||
        ((reg==0x18 || reg==0x39) && size==1));
    if (s->reset_wait && (int32_t)(s->ms-s->otp_due)>=0) {
        s->reset_wait=false; s->status=s->otp_missing ? 0 : 0x10;
    }
    if (s->measuring && !s->never_done && (int32_t)(s->ms-s->data_due)>=0) {
        s->measuring=false; s->status |= 0x40;
        if (s->selftest && s->selftest_fail) s->status |= 0x20;
    }
    if (reg==0x39) *data=s->id;
    else if (reg==0x18) *data=s->status;
    else if (reg==0x27) memcpy(data,s->factory,3);
    else {
        if (s->partial_data_failure) { data[0]=s->data[0]; return false; }
        memcpy(data,s->data,9);
        if (!s->stuck_done) s->status &= (uint8_t)~0x40U;
        if (s->selftest && s->clear_selftest_sat_on_read) s->status &= (uint8_t)~0x20U;
        if (s->cancel_data) s->cancel=true;
    }
    if (reg==s->slow_reg) s->ms+=s->read_delay_ms;
    return true;
}
static bool fake_mag_write(fake_mmc5603_t *s, uint8_t reg, uint8_t value)
{
    ++s->writes;
    if (++s->operations == s->fail_at || (reg==0x1c && s->fail_reset)) return false;
    if (reg==0x1c) {
        assert(value==0x80); ++s->resets;
        s->status=0; s->measuring=false; s->reset_wait=true; s->otp_due=s->ms+20;
        memset(s->threshold,0,3);
    } else if (reg>=0x1e && reg<=0x20) s->threshold[reg-0x1e]=value;
    else {
        assert(reg==0x1b && (value==0x41 || value==0x21));
        assert(!s->reset_wait && !s->measuring && !(s->status&0x40));
        s->selftest=value==0x41;
        s->data_due=s->ms+7; s->measuring=true; ++s->triggers;
    }
    return true;
}
#endif
