/* Exercise the real adapter against counted RTOS/PM calls, not a second policy. */
#include <stdarg.h>
#include <stdio.h>
#include "../apps/product/src/product_pm.c"

static int irq_masked, idle_requests, logs;
static uint32_t test_tick, test_mode = PM_SLEEP_MODE_DEEP;
static void (*notify_callback)(uint8_t, uint8_t, void *);
rt_base_t rt_hw_interrupt_disable(void) { int old = irq_masked; irq_masked = 1; return old; }
void rt_hw_interrupt_enable(rt_base_t old) { irq_masked = old; }
rt_tick_t rt_tick_get(void) { return test_tick; }
rt_tick_t rt_tick_from_millisecond(int ms) { return (uint32_t)ms; }
rt_err_t rt_event_init(struct rt_event *e, const char *n, unsigned f) { (void)n; (void)f; e->pending=0; return 0; }
rt_err_t rt_event_send(struct rt_event *e, uint32_t mask) { e->pending |= mask; return 0; }
rt_err_t rt_event_recv(struct rt_event *e, uint32_t mask, unsigned f, int32_t t, uint32_t *out)
{ (void)t; *out=e->pending & mask; if (f & RT_EVENT_FLAG_CLEAR) e->pending &= ~*out; return *out ? 0 : -RT_ETIMEOUT; }
void rt_timer_init(struct rt_timer *t, const char *n, void (*cb)(void *), void *ctx, uint32_t ticks, unsigned f)
{ (void)n; (void)f; t->callback=cb; t->context=ctx; t->ticks=ticks; }
rt_err_t rt_timer_start(struct rt_timer *t) { assert(t->ticks==5000); return 0; }
void rt_kprintf(const char *f, ...)
{
    assert(!irq_masked);
    char buffer[128];
    va_list args; va_start(args,f);
    int size=vsnprintf(buffer,sizeof buffer,f,args);
    va_end(args);
    assert(size>=0 && size<(int)sizeof buffer);
    ++logs;
}
int8_t HAL_HPAON_QueryWakeupPin(int port, int pin) { assert(port==1 && pin==34); return 4; }
rt_err_t pm_enable_pin_wakeup(uint8_t pin, int mode) { assert(pin==4 && mode==3); return 0; }
void rt_pm_request(uint8_t mode) { assert(mode==PM_SLEEP_MODE_IDLE); ++idle_requests; }
void rt_pm_release(uint8_t mode) { assert(mode==PM_SLEEP_MODE_IDLE && idle_requests>0); --idle_requests; }
void rt_pm_notify_set(void (*cb)(uint8_t,uint8_t,void *), void *ctx) { assert(!ctx); notify_callback=cb; }
uint8_t rt_pm_sleep_mode_state_get(uint8_t mode) { assert(mode==PM_SLEEP_MODE_IDLE); return idle_requests; }
uint32_t pm_get_power_mode(void) { return test_mode; }
uint32_t pm_get_wakeup_src(void) { return 0x20; }
static void command(char *name) { char *args[]={"wf_pm",name}; assert(wf_pm(2,args)==0); }
static void sleep_return(uint32_t delta, uint8_t exit_mode)
{
    int before=logs;
    rt_base_t level=rt_hw_interrupt_disable();
    notify_callback(RT_PM_ENTER_SLEEP, PM_SLEEP_MODE_DEEP, NULL);
    test_tick += delta;
    notify_callback(RT_PM_EXIT_SLEEP, exit_mode, NULL);
    rt_hw_interrupt_enable(level);
    assert(logs==before); /* No I/O from the IRQ-disabled PM callback. */
}
int main(void)
{
    wristflow_product_pm_start();
    assert(idle_requests==1); /* Default boot hold. */
    wristflow_product_pm_screen(true);
    assert(idle_requests==1);
    command("allow"); command("allow");
    assert(idle_requests==0);
    wristflow_product_pm_phone_event();
    wristflow_product_event_send(WF_EVENT_KEY);
    assert(wristflow_product_event_wait(0)==(WF_EVENT_PHONE|WF_EVENT_KEY));
    assert(wristflow_product_event_wait(0)==0);
    assert(display_off && idle_requests==0 && stats.phone_off==1);
    for (int i=0;i<10;++i) {
        wristflow_product_pm_screen(false); wristflow_product_pm_screen(false);
        assert(idle_requests==1);
        wristflow_product_pm_screen(true); wristflow_product_pm_screen(true);
        assert(idle_requests==0);
    }
    command("hold"); command("hold"); assert(idle_requests==1);
    wristflow_product_pm_screen(false); command("allow"); assert(idle_requests==1);
    wristflow_product_pm_screen(true); assert(idle_requests==0);
    sleep_return(0,PM_SLEEP_MODE_DEEP); /* Peripheral veto contributes no elapsed sleep. */
    sleep_return(0,PM_SLEEP_MODE_IDLE); /* Timer shortened between suspend and entry. */
    assert(stats.deep_elapsed==0);
    test_tick=UINT32_MAX-10;
    sleep_return(50,PM_SLEEP_MODE_DEEP); /* Tick wrap. */
    assert(stats.deep_elapsed==1 && stats.deep_ticks==50 && stats.max_deep_ticks==50);
    assert(stats.enter==stats.exit && stats.last_wake_source==0x20);
    sample_timer.callback(sample_timer.context);
    assert(wristflow_product_event_wait(0)==WF_EVENT_PM_SAMPLE);
    assert(display_off && idle_requests==0);
    command("wake"); assert(wristflow_product_event_wait(0)==WF_EVENT_TEST_WAKE);
    command("hold"); assert(idle_requests==1);
    memset((void *)&stats,0xff,sizeof stats);
    command("status"); /* Large counters must not truncate console evidence. */
    puts("PM gate, mixed events, callback context and tick accounting passed");
    return 0;
}
