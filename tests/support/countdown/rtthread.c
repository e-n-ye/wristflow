#include "rtthread.h"
#include "product_pm.h"
#include <stddef.h>
static uint32_t clock_ms, events;
static unsigned sends;
static struct rt_timer *timer;
static struct rt_mutex *mutex;
rt_tick_t rt_tick_get(void) { return clock_ms; }
rt_err_t rt_mutex_init(struct rt_mutex *m, const char *name, unsigned flags)
{ (void)name; assert(flags == RT_IPC_FLAG_PRIO); mutex = m; m->held = false; return RT_EOK; }
rt_err_t rt_mutex_take(struct rt_mutex *m, int32_t timeout)
{ assert(m == mutex && !m->held && timeout == RT_WAITING_FOREVER); m->held = true; return RT_EOK; }
rt_err_t rt_mutex_release(struct rt_mutex *m)
{ assert(m->held); m->held = false; return RT_EOK; }
void rt_timer_init(struct rt_timer *t, const char *name, void (*cb)(void *), void *ctx, uint32_t ticks, unsigned flags)
{
    (void)name; assert(flags == (RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER));
    timer = t; *t = (struct rt_timer){.callback=cb, .context=ctx, .ticks=ticks};
}
rt_err_t rt_timer_stop(struct rt_timer *t)
{ assert(mutex->held); bool active=t->active; t->active=false; return active ? RT_EOK : -RT_ERROR; }
rt_err_t rt_timer_control(struct rt_timer *t, int operation, void *value)
{ assert(mutex->held && operation == RT_TIMER_CTRL_SET_TIME); t->ticks=*(rt_tick_t *)value; return RT_EOK; }
rt_err_t rt_timer_start(struct rt_timer *t)
{ assert(mutex->held && t->ticks && t->ticks <= 300000U); t->at=clock_ms; t->active=true; return RT_EOK; }
void wristflow_product_event_send(rt_uint32_t mask)
{ assert(!mutex->held && mask == WF_EVENT_COUNTDOWN); events |= mask; ++sends; }
void test_countdown_advance(uint32_t ms, bool dispatch)
{
    clock_ms += ms;
    if (dispatch && timer && timer->active && clock_ms-timer->at >= timer->ticks) {
        timer->active=false; timer->callback(timer->context);
    }
}
void test_countdown_selected_callback(void) { timer->callback(timer->context); }
uint32_t test_countdown_wait(void) { return timer->active ? timer->ticks : 0; }
uint32_t test_countdown_events(void) { uint32_t copy=events; events=0; return copy; }
unsigned test_countdown_sends(void) { return sends; }
