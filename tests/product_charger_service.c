#define RT_USING_PM
#include "../apps/product/src/product_charger_service.c"
#include <assert.h>
#include <stdio.h>
#include "endurance.h"

static struct rt_thread thread;
static struct rt_i2c_bus_device bus;
static uint8_t regs[13];
static rt_tick_t ticks;
static unsigned irq, pm, transfers, deleted, detached, enables;
static unsigned locks, expire_lock_at, cancel_lock_at, edge_lock_at;
static bool fail_event, fail_create, fail_thread, fail_send, fail_lock, fail_io;
static bool stop_during_io, adc_fail;
static bool fail_adc_init;
static bool fail_detect;
static unsigned detect_stops;
static unsigned waits;
static bool edge_during_wait;
static bool rail_leased, fail_rail_prepare, fail_rail_release;
static unsigned stop_prepare_at;
static unsigned ui_events;
void wristflow_product_event_send(rt_uint32_t bits)
{ assert(!irq && bits == WF_EVENT_CHARGER); ++ui_events; }
bool wristflow_product_charger_bus_prepare_locked(void)
{
    assert(bus.lock.held && pm);
    rail_leased = true;
    if (stop_prepare_at && locks == stop_prepare_at) assert(request_stop());
    return !fail_rail_prepare;
}
bool wristflow_product_charger_bus_release(void)
{
    assert(session.state != WF_CHARGE_ENABLED && session.state != WF_CHARGE_UNCONFIRMED);
    if (fail_rail_release) return false;
    rail_leased = false;
    return true;
}
int rt_thread_mdelay(rt_int32_t ms)
{
    assert(ms == 150 && pm && !bus.lock.held && (regs[1] & 8));
    ticks += ms; ++waits;
    if (edge_during_wait) { edge_during_wait = false; input_changed(); }
    return RT_EOK;
}
bool wristflow_product_charger_detect_start(void (*changed)(void))
{ assert(changed == input_changed); return !fail_detect; }
void wristflow_product_charger_detect_stop(void) { ++detect_stops; }
bool wristflow_product_power_adc_init(void) { return !fail_adc_init; }
rt_base_t rt_hw_interrupt_disable(void) { return irq++; }
void rt_hw_interrupt_enable(rt_base_t previous) { assert(irq); irq = (unsigned)previous; }
rt_tick_t rt_tick_get(void) { return ticks; }
rt_tick_t rt_tick_from_millisecond(unsigned ms) { return ms; }
void rt_kprintf(const char *format, ...) { (void)format; }
void rt_pm_request(unsigned mode) { assert(mode == PM_SLEEP_MODE_IDLE && !irq && !pm); ++pm; }
void rt_pm_release(unsigned mode) { assert(mode == PM_SLEEP_MODE_IDLE && !irq && pm); --pm; }
int rt_mutex_init(struct rt_mutex *m, const char *name, unsigned flags)
{ (void)name; (void)flags; m->held = 0; return 0; }
int rt_mutex_take(struct rt_mutex *m, int wait)
{
    assert(m == &bus.lock && !irq && pm);
    if (!m->held) {
        assert(wait == 50); if (fail_lock) return -1;
        ++locks;
        if (locks == expire_lock_at) ticks = session.deadline_ms;
        if (locks == cancel_lock_at) assert(request_stop());
        if (locks == edge_lock_at) input_changed();
    }
    else assert(wait == RT_WAITING_FOREVER);
    ++m->held; return 0;
}
int rt_mutex_release(struct rt_mutex *m) { assert(m == &bus.lock && m->held); --m->held; return 0; }
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char *name)
{ assert(!strcmp(name, "i2c2")); return &bus; }
static size_t mem(bool write, struct rt_i2c_bus_device *b, unsigned addr,
                  unsigned reg, unsigned width, void *data, size_t size)
{
    assert(b == &bus && bus.lock.held == 1 && !irq && pm);
    assert(addr == 0x49 && width == 8 && size == 1);
    rt_mutex_take(&bus.lock, RT_WAITING_FOREVER);
    ++transfers; ++ticks;
    if (write) {
        if (reg == 1 && !(*(uint8_t *)data & 8)) ++enables;
        regs[reg] = *(uint8_t *)data;
    } else {
        *(uint8_t *)data = regs[reg];
        if (reg == 9) regs[9] &= 0xc0;
    }
    if (stop_during_io) {
        stop_during_io = false;
        assert(request_stop()); /* Foreground command does not touch hardware. */
    }
    rt_mutex_release(&bus.lock);
    return fail_io ? 0 : 1;
}
size_t rt_i2c_mem_read(struct rt_i2c_bus_device *b, unsigned a, unsigned r, unsigned w, void *v, size_t n)
{ return mem(false,b,a,r,w,v,n); }
size_t rt_i2c_mem_write(struct rt_i2c_bus_device *b, unsigned a, unsigned r, unsigned w, void *v, size_t n)
{ return mem(true,b,a,r,w,v,n); }
wristflow_power_snapshot_t wristflow_product_power_voltage(void)
{ assert(pm && !irq); return (wristflow_power_snapshot_t){.voltage_valid=!adc_fail, .millivolts=3960}; }
int rt_event_init(struct rt_event *event, const char *name, unsigned flags)
{ assert(!strcmp(name,"wf_chg") && flags == RT_IPC_FLAG_PRIO); event->bits=0; return fail_event ? -1 : 0; }
int rt_event_detach(struct rt_event *event) { (void)event; ++detached; return 0; }
int rt_event_send(struct rt_event *event, unsigned bits)
{ if (fail_send) return -1; event->bits |= bits; return 0; }
int rt_event_recv(struct rt_event *event, unsigned mask, unsigned flags, int wait, rt_uint32_t *bits)
{ (void)flags; (void)wait; *bits=event->bits&mask; event->bits &= ~mask; return *bits ? 0 : -1; }
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *context,
                             unsigned stack, unsigned priority, unsigned slice)
{
    assert(!strcmp(name,"wf_charge") && stack==4096 && priority==23 && slice==10);
    thread=(struct rt_thread){entry,context}; return fail_create ? NULL : &thread;
}
int rt_thread_startup(rt_thread_t t) { assert(t==&thread); return fail_thread ? -1 : 0; }
int rt_thread_delete(rt_thread_t t) { assert(t==&thread); ++deleted; return 0; }
static void background(void)
{
    if (detect_pending && (int32_t)(ticks - detect_after_ms) < 0) ticks = detect_after_ms;
    commands.bits=0; step(); assert(!irq && !bus.lock.held);
}
static int command(const char *action, const char *duration)
{ char *args[]={"wf_charger",(char *)action,(char *)duration}; return wf_charger(duration ? 3 : 2,args); }
static void input_observation_tests(void)
{
    assert(!pm && !rail_leased);
    /* A boot with USB already present must not create an insertion. */
    session=(wristflow_charge_session_t){.state=WF_CHARGE_STOPPED,.reason=WF_CHARGE_MANUAL};
    input_known=cached_input_known=false;input_good=cached_input_good=false;
    insertion_armed=false;input_insertions=cached_input_insertions=0;
    auto_blocked=true;published_charge=WF_POWER_CHARGE_STOPPED;
    presentation_cycle=cached_presentation_cycle=0;
    regs[8]=0x42;regs[9]=0;input_changed();background();
    wristflow_power_snapshot_t off={0};wristflow_product_charger_display(&off);
    assert(off.input_observation.valid && off.input_observation.good && !off.input_observation.insertions);
    assert(!off.input_valid && !off.voltage_valid && !off.charge_cycle);
    wristflow_endurance_t mode;wristflow_endurance_init(&mode,true,&off.input_observation);
    assert(!wristflow_endurance_usb(&mode,&off.input_observation));
    unsigned notifications=ui_events;
    regs[8]=0x40;input_changed();background();
    assert(published_charge==WF_POWER_CHARGE_STOPPED && ui_events>notifications);
    /* No consumer read between low and high: the durable insertion survives
     * merged mailbox bits, even if entry is refused and no ACTIVE cycle exists. */
    fail_io=true;input_changed();background();
    wristflow_product_charger_display(&off);assert(!off.input_observation.valid);
    fail_io=false;adc_fail=true;regs[8]=0x42;input_changed();background();adc_fail=false;
    wristflow_product_charger_display(&off);
    assert(off.input_observation.valid && off.input_observation.good && off.input_observation.insertions==1);
    assert(!off.charge_cycle && wristflow_endurance_usb(&mode,&off.input_observation));
    wristflow_endurance_init(&mode,true,&off.input_observation);
    fail_io=true;input_changed();background();fail_io=false;
    input_changed();background();wristflow_product_charger_display(&off);
    assert(off.input_observation.insertions==1 && !wristflow_endurance_usb(&mode,&off.input_observation));
    input_changed();wristflow_product_charger_display(&off);assert(!off.input_observation.valid);
    background();wristflow_product_charger_display(&off);
    assert(off.input_observation.insertions==1 && !wristflow_endurance_usb(&mode,&off.input_observation));
    /* Full indication also has no automatic ACTIVE presentation identity. */
    regs[8]=0x40;input_changed();background();regs[8]=0x5a;input_changed();background();
    wristflow_product_charger_display(&off);
    assert(off.charge==WF_POWER_CHARGE_FULL && !off.charge_cycle && off.input_observation.insertions==2);
    assert(wristflow_endurance_usb(&mode,&off.input_observation));
}
int main(void)
{
    assert(command("start","1") != RT_EOK && !transfers);
    fail_adc_init=true; assert(!wristflow_product_charger_service_start()); fail_adc_init=false;
    fail_event=true; assert(!wristflow_product_charger_service_start()); fail_event=false;
    fail_create=true; assert(!wristflow_product_charger_service_start() && detached==1); fail_create=false;
    fail_thread=true; assert(!wristflow_product_charger_service_start() && deleted==1 && detached==2); fail_thread=false;
    assert(detect_stops == 1);
    fail_detect=true; assert(!wristflow_product_charger_service_start()); fail_detect=false;
    assert(wristflow_product_charger_service_start() && !transfers && !pm);
    assert(worker_wait()==150);
    step(); assert(!transfers && pm && detect_pending && worker_wait()==150);
    ticks+=50; input_changed(); assert(worker_wait()==100); /* No sliding window. */
    regs[1]=0xac; regs[2]=9; regs[4]=0x9b; regs[5]=0x1a; regs[8]=0x42; regs[10]=0x49; regs[11]=0x43;
    regs[8] = 0x40; background(); /* Initial USB-absent check, then no polling. */
    assert(input_known && !input_good && !pm && !auto_blocked);
    /* The voltage worker clears its cache while off. USB policy must still
     * receive the charger's qualified PG without requesting an ADC sample. */
    wristflow_power_snapshot_t off={0};
    wristflow_product_charger_display(&off);
    assert(off.input_observation.valid && !off.input_observation.good);
    assert(!off.input_valid && !off.voltage_valid);
    regs[8] = 0x42;
    assert(command("start","0") != RT_EOK && command("start","61") != RT_EOK);
    assert(command("start","-1") != RT_EOK && command("start","1abc") != RT_EOK);
    unsigned initial_transfers = transfers;
    assert(command("start","99999999999999999999") != RT_EOK && transfers == initial_transfers);
    fail_send=true; assert(command("start","1") != RT_EOK && !busy); fail_send=false;
    assert(command("status",NULL)==RT_EOK && transfers == initial_transfers); background();
    assert(!pm && !enables && cached.state==WF_CHARGE_IDLE);
    unsigned before=transfers;
    assert(command("start","1")==RT_EOK && command("start","1")!=RT_EOK && transfers==before);
    assert(command("stop",NULL)==RT_EOK && transfers==before); background();
    assert(cached.state==WF_CHARGE_STOPPED && !enables && !pm && !busy);
    assert(command("start","1")==RT_EOK); background();
    assert(cached.state==WF_CHARGE_ENABLED && pm==1 && enables==1);
    assert(command("start","60")!=RT_EOK); /* no extending the session */
    ticks=cached.deadline_ms;
    background(); /* independent RTOS worker, no UI/LVGL callback */
    assert(cached.reason==WF_CHARGE_TIMEOUT && cached.state==WF_CHARGE_STOPPED && !pm);
    assert(command("start","1")==RT_EOK); stop_during_io=true; background();
    assert(cached.reason==WF_CHARGE_MANUAL && cached.state==WF_CHARGE_STOPPED && !pm && enables==1);
    assert(command("start","2")==RT_EOK); background(); assert(pm);
    fail_lock=true; before=transfers;
    assert(command("stop",NULL)==RT_EOK); background();
    assert(transfers==before && cached.state==WF_CHARGE_UNCONFIRMED && pm==1 && busy);
    assert(command("start","2")!=RT_EOK); /* no restart after unconfirmed stop */
    before=transfers; background(); assert(transfers==before && pm==1); /* no autonomous retry */
    fail_lock=false; assert(command("stop",NULL)==RT_EOK); background();
    assert(cached.state==WF_CHARGE_STOPPED && !busy && !pm);
    fail_io=true; assert(command("start","2")==RT_EOK); background();
    assert(cached.state==WF_CHARGE_UNCONFIRMED && pm==1 && !bus.lock.held);
    fail_io=false; assert(command("stop",NULL)==RT_EOK); background(); assert(!pm);
    before=enables; expire_lock_at=locks+31;
    assert(command("start","2")==RT_EOK); background();
    assert(cached.reason==WF_CHARGE_TIMEOUT && !pm && enables==before);
    expire_lock_at=0; cancel_lock_at=locks+31;
    assert(command("start","2")==RT_EOK); background();
    assert(cached.reason==WF_CHARGE_MANUAL && !pm && enables==before);
    cancel_lock_at=0;
    adc_fail=true; assert(command("start","2")==RT_EOK); background();
    assert(cached.reason==WF_CHARGE_VOLTAGE && cached.state==WF_CHARGE_STOPPED && !pm);
    adc_fail=false;
    /* Complete confirmed unplug/replug arms exactly one automatic attempt. */
    regs[8]=0x40; input_changed(); background(); assert(!auto_blocked && !pm);
    regs[8]=0x42; input_changed(); background();
    assert(cached.automatic && cached.state==WF_CHARGE_ENABLED && auto_blocked && pm);
    assert(regs[2]==0x18 && cached.current==0x18); /*200mA, not startup/manual80mA*/
    wristflow_power_snapshot_t display={0};
    wristflow_product_charger_display(&display);
    assert(display.charge==WF_POWER_CHARGE_UNKNOWN); /* enable readback alone is not charging */
    assert(!display.charge_cycle);
    regs[8]=0x52; background(); wristflow_product_charger_display(&display);
    assert(display.charge==WF_POWER_CHARGE_ACTIVE);
    uint32_t first_cycle = display.charge_cycle;
    assert(first_cycle && ui_events);
    /* Status INT pulses while PG stays high do not stop/restart or extend the
     * session. This is the unplug/replug user's extra-edge regression. */
    uint32_t deadline=cached.deadline_ms;
    before=enables;
    input_changed(); uint32_t first_due=detect_after_ms;
    ticks+=40; input_changed(); ticks+=40; input_changed();
    assert(detect_after_ms==first_due && worker_wait()==70);
    unsigned reads_before=transfers;
    step(); assert(transfers==reads_before && cached.state==WF_CHARGE_ENABLED && pm);
    background(); assert(cached.state==WF_CHARGE_ENABLED && enables==before && cached.deadline_ms==deadline);
    for (unsigned pulse=0; pulse<3; ++pulse) {
        input_changed(); background();
        assert(cached.state==WF_CHARGE_ENABLED && enables==before && cached.deadline_ms==deadline);
        wristflow_product_charger_display(&display); assert(display.charge_cycle == first_cycle);
    }
    /* Repeated edges inside the PG reads leave new work pending, but cannot
     * starve active samples or the original deadline. */
    for (unsigned pulse=0; pulse<3; ++pulse) {
        reads_before=transfers; input_changed(); edge_lock_at=locks+2; background();
        assert(detect_pending && transfers>reads_before+2 && cached.state==WF_CHARGE_ENABLED);
    }
    ticks=deadline; edge_lock_at=locks+2; background(); edge_lock_at=0;
    assert(cached.reason==WF_CHARGE_TIMEOUT && cached.state==WF_CHARGE_STOPPED && detect_pending && pm);
    background(); assert(!pm && auto_blocked && enables==before);
    regs[8]=0x40; input_changed(); background();
    wristflow_product_charger_display(&display); assert(!display.charge_cycle);
    regs[8]=0x42; input_changed(); background(); regs[8]=0x52; background();
    wristflow_product_charger_display(&display); uint32_t full_cycle = display.charge_cycle;
    assert(full_cycle && full_cycle != first_cycle);
    before=enables; regs[8]=0x5a; background();
    assert(cached.reason==WF_CHARGE_COMPLETE && !pm && auto_blocked);
    wristflow_product_charger_display(&display); assert(display.charge==WF_POWER_CHARGE_FULL);
    assert(display.charge_cycle == full_cycle);
    input_changed(); wristflow_product_charger_display(&display); assert(display.charge==WF_POWER_CHARGE_UNKNOWN);
    background(); assert(enables==before && !pm); /* PG remains high */
    wristflow_product_charger_display(&display); assert(display.charge==WF_POWER_CHARGE_FULL);
    assert(display.charge_cycle == full_cycle);
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; input_changed(); background(); assert(pm && enables==before+1);
    assert(command("stop",NULL)==RT_EOK); background();
    before=enables; input_changed(); background(); assert(!pm && enables==before);
    /* Unknown input is not unplug and cannot clear a lock. */
    fail_io=true; input_changed(); background(); assert(!input_known && auto_blocked && !pm);
    fail_io=false; input_changed(); background(); assert(enables==before && auto_blocked);
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; input_changed(); background(); assert(pm);
    fail_lock=true; assert(command("stop",NULL)==RT_EOK); background();
    assert(cached.state==WF_CHARGE_UNCONFIRMED && pm);
    regs[8]=0x40; input_changed(); background(); assert(auto_blocked && pm);
    fail_lock=false; assert(command("stop",NULL)==RT_EOK); background(); assert(!pm);
    /* Detach during automatic preparation cancels before enable and latches. */
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; stop_during_io=true; before=enables; input_changed(); background();
    assert(enables==before && stop_pending); background(); assert(!pm && auto_blocked);
    /* Stop wins, but a simultaneous confirmed unplug still rearms the next plug. */
    regs[8]=0x40; input_changed(); assert(command("stop",NULL)==RT_EOK); background();
    assert(!auto_blocked && !pm);
    regs[8]=0x42; before=enables; input_changed(); background();
    assert(enables==before+1 && pm);
    assert(command("stop",NULL)==RT_EOK); background();
    /* Edge racing the final enable-lock acquisition prevents enable entirely. */
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; before=enables; edge_lock_at=locks+39;
    input_changed(); background();
    assert(enables==before && cached.state==WF_CHARGE_STOPPED && pm && detect_pending);
    edge_lock_at=0; background(); assert(auto_blocked && enables==before && !pm);
    /* Failed board preparation never enables; unconfirmed stop keeps both
     * IDLE and rail responsibility, then explicit stop may finish cleanup. */
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; regs[9]=0; fail_rail_prepare=true; before=enables;
    assert(command("start","2")==RT_EOK); background();
    assert(enables==before && cached.state==WF_CHARGE_UNCONFIRMED && pm && rail_leased);
    fail_rail_prepare=false; assert(command("stop",NULL)==RT_EOK); background();
    assert(!pm && !rail_leased);
    fail_rail_release=true; assert(command("stop",NULL)==RT_EOK); background();
    assert(pm && busy);
    fail_rail_release=false; assert(command("stop",NULL)==RT_EOK); background();
    assert(!pm && !busy && !rail_leased);
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; regs[9]=0; before=enables; stop_prepare_at=locks+39;
    input_changed(); background();
    assert(enables==before && cached.state==WF_CHARGE_STOPPED && !pm);
    stop_prepare_at=0;
    /* Recovered entry VIN latch qualifies under inhibition, but an input edge
     * during either wait still cancels without enabling or retrying. */
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; regs[9]=0x20; before=enables; input_changed(); background();
    assert(enables==before+1 && pm && cached.input_qualified && cached.entry_fault==0x20 && waits==2);
    assert(command("stop",NULL)==RT_EOK); background();
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; regs[9]=0x20; edge_during_wait=true; before=enables; input_changed(); background();
    assert(enables==before && cached.state==WF_CHARGE_STOPPED && pm && detect_pending);
    background(); assert(auto_blocked && enables==before && !pm);
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; regs[9]=0x20; before=enables; edge_lock_at=locks+57;
    input_changed(); background();
    assert(enables==before && cached.state==WF_CHARGE_STOPPED && pm && detect_pending);
    edge_lock_at=0; background(); assert(auto_blocked && enables==before && !pm);
    /* A real PG0 stops and rearms; the subsequent plug starts exactly once,
     * and its additional PG1 pulse leaves that new session running. */
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; input_changed(); background(); assert(pm);
    regs[8]=0x52; input_changed(); background(); before=enables;
    regs[8]=0x40; input_changed(); background();
    assert(cached.reason==WF_CHARGE_INPUT && cached.state==WF_CHARGE_STOPPED && !auto_blocked && !pm);
    regs[8]=0x42; input_changed(); background();
    assert(enables==before+1 && cached.state==WF_CHARGE_ENABLED);
    regs[8]=0x52; input_changed(); background(); assert(enables==before+1 && cached.state==WF_CHARGE_ENABLED);
    /* Explicit stop does not wait for the input settling window. */
    input_changed(); assert(command("stop",NULL)==RT_EOK); step();
    assert(cached.state==WF_CHARGE_STOPPED && cached.reason==WF_CHARGE_MANUAL && detect_pending && pm);
    background(); assert(!pm && auto_blocked && enables==before+1);
    /* An edge arriving during PG detection remains pending, then is handled. */
    regs[8]=0x40; input_changed(); edge_lock_at=locks+2; background();
    assert(detect_pending && pm); edge_lock_at=0; background(); assert(!auto_blocked && !pm);
    /* Tick wrap still waits exactly 150ms; a failed active PG read cannot
     * retain a normal charging indication or clear an unconfirmed stop. */
    ticks=UINT32_MAX-70; regs[8]=0x42; before=enables; input_changed(); assert(worker_wait()==150);
    ticks+=149; step(); assert(worker_wait()==1 && enables==before);
    background(); assert(cached.state==WF_CHARGE_ENABLED);
    fail_io=true; input_changed(); background();
    assert(!input_known && auto_blocked && cached.state==WF_CHARGE_UNCONFIRMED && pm);
    fail_io=false; regs[8]=0x40; input_changed(); background();
    assert(cached.state==WF_CHARGE_UNCONFIRMED && auto_blocked && pm);
    assert(command("stop",NULL)==RT_EOK); background(); assert(!pm);
    /* A genuine active fault is still stopped and locked, never qualified
     * again by a later PG1 status pulse. */
    input_changed(); background(); regs[8]=0x42; input_changed(); background();
    regs[8]=0x52; regs[9]=0x20; before=enables; input_changed(); background();
    assert(cached.reason==WF_CHARGE_FAULT && cached.state==WF_CHARGE_STOPPED && auto_blocked && !pm);
    input_changed(); background(); assert(enables==before && auto_blocked && !pm);
    /* Stop accepted after a first ACTIVE poll but before publication cannot
     * create a new presentation, even though inhibition runs next turn. */
    regs[8]=0x40; input_changed(); background();
    regs[8]=0x42; input_changed(); background();
    regs[8]=0x52; wristflow_charge_poll(&session,&port);
    assert(command("stop",NULL)==RT_EOK); publish();
    before=transfers; wristflow_product_charger_display(&display);
    assert(!display.charge_cycle && transfers==before); background(); assert(!pm);
    input_observation_tests();
    puts("charger worker: asynchronous commands, stop precedence, initialization, PM, recursive lock and fault cleanup passed");
    return 0;
}
