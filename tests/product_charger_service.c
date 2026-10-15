#define RT_USING_PM
#include "../apps/product/src/product_charger_service.c"
#include <assert.h>
#include <stdio.h>

static struct rt_thread thread;
static struct rt_i2c_bus_device bus;
static uint8_t regs[13];
static rt_tick_t ticks;
static unsigned irq, pm, transfers, deleted, detached, enables;
static unsigned locks, expire_lock_at, cancel_lock_at;
static bool fail_event, fail_create, fail_thread, fail_send, fail_lock, fail_io;
static bool stop_during_io, adc_fail;
static bool fail_adc_init;
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
    } else *(uint8_t *)data = regs[reg];
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
static void background(void) { commands.bits=0; step(); assert(!irq && !bus.lock.held); }
static int command(const char *action, const char *duration)
{ char *args[]={"wf_charger",(char *)action,(char *)duration}; return wf_charger(duration ? 3 : 2,args); }
int main(void)
{
    assert(command("start","1") != RT_EOK && !transfers);
    fail_adc_init=true; assert(!wristflow_product_charger_service_start()); fail_adc_init=false;
    fail_event=true; assert(!wristflow_product_charger_service_start()); fail_event=false;
    fail_create=true; assert(!wristflow_product_charger_service_start() && detached==1); fail_create=false;
    fail_thread=true; assert(!wristflow_product_charger_service_start() && deleted==1 && detached==2); fail_thread=false;
    assert(wristflow_product_charger_service_start() && !transfers && !pm);
    regs[1]=0xac; regs[2]=9; regs[4]=0x9b; regs[5]=0x1a; regs[8]=0x42; regs[10]=0x49; regs[11]=0x43;
    assert(command("start","0") != RT_EOK && command("start","61") != RT_EOK);
    assert(command("start","-1") != RT_EOK && command("start","1abc") != RT_EOK);
    assert(command("start","99999999999999999999") != RT_EOK && !transfers);
    fail_send=true; assert(command("start","1") != RT_EOK && !busy); fail_send=false;
    assert(command("status",NULL)==RT_EOK && !transfers); background();
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
    puts("charger worker: asynchronous commands, stop precedence, initialization, PM, recursive lock and fault cleanup passed");
    return 0;
}
