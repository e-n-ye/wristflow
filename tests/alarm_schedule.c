#include "alarm_schedule.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* 2026-01-05 Monday, 06:59:00 in UTC+8. */
#define MONDAY_0659 1767567540U
static wristflow_alarm_clock_t clock_at(uint32_t utc, uint64_t ms, unsigned revision)
{ return (wristflow_alarm_clock_t){.utc=utc, .revision=revision, .monotonic_ms=ms, .valid=true}; }
static void add(wristflow_alarm_schedule_t *s, uint8_t weekdays, wristflow_alarm_clock_t c)
{
    wristflow_alarm_command_t cmd = {.operation=WF_ALARM_ADD, .draft={.hour=7,.weekdays=weekdays}};
    assert(wristflow_alarm_schedule_command(s, &cmd, c));
}
int main(void)
{
    wristflow_alarm_schedule_t s; wristflow_alarm_schedule_init(&s);
    wristflow_alarm_clock_t c=clock_at(MONDAY_0659, 1000, 0);
    assert(!wristflow_alarm_schedule_update(&s,c)); add(&s,0,c);
    wristflow_alarm_command_t bad_add={.operation=WF_ALARM_ADD,.id=1,.draft={.hour=8}};
    assert(!wristflow_alarm_schedule_command(&s,&bad_add,c) && s.data.alarms.count==1);
    assert(s.data.dates[0].once_utc == MONDAY_0659+60 && s.next_utc[0] == MONDAY_0659+60);
    assert(wristflow_alarm_schedule_wait(&s,c)==60000);
    assert(wristflow_alarm_schedule_update(&s,clock_at(c.utc+60,61000,0)));
    assert(!s.data.alarms.entries[0].enabled && s.pending[0].active && s.pending[0].due_ms==61000);
    assert(!wristflow_alarm_schedule_ack(&s,1,s.pending[0].sequence+1));
    assert(wristflow_alarm_schedule_ack(&s,1,s.pending[0].sequence));
    wristflow_alarm_command_t enable={.operation=WF_ALARM_ENABLE,.id=1,.enabled=true};
    assert(wristflow_alarm_schedule_command(&s,&enable,clock_at(c.utc+60,61000,0)));
    assert(s.data.dates[0].once_utc == c.utc+60+86400); /* Current minute -> tomorrow. */
    wristflow_alarm_schedule_t reboot; wristflow_alarm_schedule_init(&reboot); reboot.data=s.data;
    assert(!wristflow_alarm_schedule_update(&reboot,clock_at(c.utc+2*86400,1,0)));
    assert(!reboot.data.alarms.entries[0].enabled && !reboot.pending[0].active);

    wristflow_alarm_schedule_init(&s); c=(wristflow_alarm_clock_t){0};
    wristflow_alarm_schedule_update(&s,c); add(&s,0,c); add(&s,WRISTFLOW_ALARM_WORKDAYS,c);
    assert(s.data.alarms.entries[0].enabled && !s.data.dates[0].once_utc && !s.time_valid);
    c=clock_at(MONDAY_0659+60,10000,1);
    assert(!wristflow_alarm_schedule_update(&s,c));
    assert(s.data.dates[0].once_utc==c.utc+86400 && s.next_utc[1]==c.utc+86400);
    /* Forward set across both deadlines skips, even at the exact minute. */
    assert(!wristflow_alarm_schedule_update(&s,clock_at(c.utc+86400+600,20000,2)));
    assert(!s.data.alarms.entries[0].enabled && !s.pending[0].active && !s.pending[1].active);
    assert(s.next_utc[1]==c.utc+2*86400);
    /* Wednesday fires; backward to Monday suppresses Mon/Tue/Wed thereafter. */
    c=clock_at(c.utc+2*86400,20000+85800000,2);
    assert(wristflow_alarm_schedule_update(&s,c));
    uint32_t day=wristflow_alarm_local_day(c.utc);
    assert(s.data.dates[1].last_fired_day==day);
    assert(!wristflow_alarm_schedule_update(&s,clock_at(MONDAY_0659,100000000,3)));
    assert(s.next_utc[1]==MONDAY_0659+60+3*86400);
    wristflow_alarm_command_t edit={.operation=WF_ALARM_EDIT,.id=2,.draft={.hour=8,.weekdays=WRISTFLOW_ALARM_DAILY}};
    assert(wristflow_alarm_schedule_command(&s,&edit,clock_at(MONDAY_0659,100000000,3)));
    assert(s.data.dates[1].last_fired_day==day && s.next_utc[1]==MONDAY_0659+3660+3*86400);
    assert(wristflow_alarm_data_valid(&s.data));

    /* Real elapsed time, unlike a set/reboot, delivers once even if callback late. */
    wristflow_alarm_schedule_init(&s); c=clock_at(MONDAY_0659,UINT32_MAX-10ULL,0);
    wristflow_alarm_schedule_update(&s,c); add(&s,WRISTFLOW_ALARM_DAILY,c);
    assert(wristflow_alarm_schedule_update(&s,clock_at(c.utc+65,c.monotonic_ms+65000,0)));
    assert(s.pending[0].due_ms==c.monotonic_ms+60000 && s.data.alarms.entries[0].enabled);
    uint32_t first=s.pending[0].sequence; uint64_t due=s.pending[0].due_ms;
    assert(wristflow_alarm_schedule_update(&s,clock_at(c.utc+60+86400,c.monotonic_ms+86460000,0)));
    assert(s.pending[0].sequence!=first && s.pending[0].due_ms==due); /* Coalesce, keep first position. */
    assert(!wristflow_alarm_schedule_ack(&s,1,first));
    /* Monday bit, weekend shortcut and UTC/local midnight crossing. */
    wristflow_alarm_schedule_init(&s); c=clock_at(MONDAY_0659,0,0); wristflow_alarm_schedule_update(&s,c);
    add(&s,1,c); add(&s,WRISTFLOW_ALARM_WEEKEND,c);
    assert(s.next_utc[0]==c.utc+60 && s.next_utc[1]==c.utc+60+5*86400);
    wristflow_alarm_command_t midnight={.operation=WF_ALARM_ADD,.draft={.hour=0,.weekdays=WRISTFLOW_ALARM_DAILY}};
    assert(wristflow_alarm_schedule_command(&s,&midnight,c));
    assert(((uint64_t)s.next_utc[2]+28800)%86400==0);
    assert(wristflow_alarm_local_day(s.next_utc[2])==wristflow_alarm_local_day(c.utc)+1);
    /* An unannounced RTC discontinuity is also skipped. */
    assert(!wristflow_alarm_schedule_update(&s,clock_at(c.utc+600,1,0)));
    assert(!s.pending[0].active);
    /* Boundary has no uint32_t overflow or date beyond the accepted RTC range. */
    c=clock_at(WRISTFLOW_TIME_MAX,UINT64_C(0x100000000),5);
    assert(!wristflow_alarm_schedule_update(&s,c));
    for(unsigned i=0;i<s.data.alarms.count;++i) assert(!s.next_utc[i] || s.next_utc[i]<=WRISTFLOW_TIME_MAX);
    /* Distinct creation samples across a RTC second cannot reorder a common deadline. */
    wristflow_alarm_schedule_init(&s); c=clock_at(MONDAY_0659,900,0); c.subsecond_ms=900;
    wristflow_alarm_schedule_update(&s,c); add(&s,WRISTFLOW_ALARM_DAILY,c);
    c=clock_at(MONDAY_0659+1,1016,0);c.subsecond_ms=16;wristflow_alarm_schedule_update(&s,c);add(&s,WRISTFLOW_ALARM_DAILY,c);
    assert(s.due_ms[0]==s.due_ms[1] && s.due_ms[0]==60000);
    assert(wristflow_alarm_schedule_update(&s,clock_at(MONDAY_0659+60,60016,0)));
    assert(s.pending[0].due_ms==s.pending[1].due_ms && s.pending[0].due_ms==60000);
    puts("alarm calendar, once dates, RTC jumps, day watermark and coalescing passed");
}
