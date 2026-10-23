/* Actual worker/cache with an independently driven background step. */
#define RT_USING_PM
#include "../apps/product/src/product_power_service.c"
#include <assert.h>
#include <string.h>

static rt_tick_t ticks;
static unsigned irq, idle, adc_calls, input_calls, deleted, detached;
static unsigned power_events;
void wristflow_product_event_send(rt_uint32_t events)
{ assert(events == WF_EVENT_POWER && !irq); ++power_events; }
static bool fail_event, fail_create, fail_start, off_in_adc, off_in_input, cycle_in_input;
static struct rt_thread fake_thread;
bool wristflow_product_power_adc_init(void) { return true; }
rt_base_t rt_hw_interrupt_disable(void) { return irq++; }
void rt_hw_interrupt_enable(rt_base_t previous) { assert(irq); irq = (unsigned)previous; }
rt_tick_t rt_tick_get(void) { return ticks; }
rt_tick_t rt_tick_from_millisecond(unsigned ms) { return ms; }
int rt_event_init(struct rt_event *event, const char *name, unsigned flags)
{ (void)name; (void)flags; event->bits = 0; return fail_event ? -1 : RT_EOK; }
int rt_event_detach(struct rt_event *event) { (void)event; ++detached; return RT_EOK; }
int rt_event_send(struct rt_event *event, unsigned bits) { event->bits |= bits; return RT_EOK; }
int rt_event_recv(struct rt_event *event, unsigned mask, unsigned flags, int wait, rt_uint32_t *bits)
{
    (void)flags; (void)wait; *bits = event->bits & mask; event->bits &= ~mask;
    return *bits ? RT_EOK : -1;
}
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *context,
                             unsigned stack, unsigned priority, unsigned slice)
{
    assert(!strcmp(name, "wf_power") && stack == 4096 && priority == 24 && slice == 10);
    fake_thread = (struct rt_thread){entry, context}; return fail_create ? NULL : &fake_thread;
}
int rt_thread_startup(rt_thread_t thread) { assert(thread == &fake_thread); return fail_start ? -1 : RT_EOK; }
int rt_thread_delete(rt_thread_t thread) { assert(thread == &fake_thread); ++deleted; return RT_EOK; }
void rt_kprintf(const char *format, ...) { (void)format; }
void rt_pm_request(unsigned mode) { assert(mode == PM_SLEEP_MODE_IDLE && !irq); ++idle; }
void rt_pm_release(unsigned mode) { assert(mode == PM_SLEEP_MODE_IDLE && idle && !irq); --idle; }

wristflow_power_snapshot_t wristflow_product_power_voltage(void)
{
    assert(!irq && idle == 1); ++adc_calls; ticks += 25;
    if (off_in_adc) wristflow_product_power_screen(false);
    return (wristflow_power_snapshot_t){.voltage_valid=true, .millivolts=3964};
}
void wristflow_product_power_input(wristflow_power_snapshot_t *value)
{
    assert(!irq && idle == 1); ++input_calls;
    /* During a five-second transfer the foreground can copy/enqueue without
     * I/O and can sleep/wake; fresh independent voltage is already published. */
    wristflow_power_snapshot_t interim = wristflow_product_power_snapshot();
    assert(interim.voltage_valid && interim.millivolts == 3964 && !interim.input_valid);
    assert(interim.voltage_sequence && interim.voltage_sequence == value->voltage_sequence);
    assert(interim.voltage_sampled_ms == value->voltage_sampled_ms && interim.voltage_sampled_ms == ticks);
    rt_tick_t before = ticks;
    if (!aod) wristflow_product_power_request();
    assert(rt_tick_get() == before);
    if (off_in_input || cycle_in_input) wristflow_product_power_screen(false);
    if (cycle_in_input) wristflow_product_power_screen(true);
    ticks += 5000;
    value->input_valid = value->input_good = true;
}
static void run_background(void)
{
    rt_uint32_t bits;
    if (rt_event_recv(&requests, 1, 3, -1, &bits) == RT_EOK) sample_once();
    assert(!idle && !irq);
}
int main(void)
{
    fail_event = true; assert(!wristflow_product_power_start()); fail_event = false;
    fail_create = true; assert(!wristflow_product_power_start() && detached == 1); fail_create = false;
    fail_start = true; assert(!wristflow_product_power_start() && detached == 2 && deleted == 1); fail_start = false;
    assert(wristflow_product_power_start());
    for (unsigned i=0; i<20; ++i) wristflow_product_power_request();
    assert(requests.bits == 1 && !adc_calls && !input_calls && !ticks);
    assert(!wristflow_product_power_snapshot().voltage_valid);
    run_background();
    assert(ticks == 5025 && adc_calls == 1 && input_calls == 1);
    assert(wristflow_product_power_snapshot().input_good);
    assert(wristflow_product_power_snapshot().voltage_sequence==1);
    assert(wristflow_product_power_snapshot().voltage_sampled_ms==25);
    wristflow_product_power_screen(false);
    assert(!wristflow_product_power_snapshot().voltage_valid);
    assert(!wristflow_product_power_snapshot().voltage_sequence);
    run_background(); /* A queued old request cannot sample while off. */
    assert(adc_calls == 1 && !requests.bits);
    ticks += 60000; wristflow_product_power_request(); run_background();
    assert(adc_calls == 1); /* No screen-off polling or wake timer. */
    wristflow_product_power_screen(true);
    assert(ticks == 65025 && !wristflow_product_power_snapshot().input_valid);
    off_in_adc = true; run_background(); off_in_adc = false;
    assert(adc_calls == 2 && input_calls == 1 && !wristflow_product_power_snapshot().voltage_valid);
    wristflow_product_power_screen(true); off_in_input = true;
    run_background(); off_in_input = false;
    assert(!wristflow_product_power_snapshot().voltage_valid); /* Reject late result after off. */
    wristflow_product_power_screen(true); cycle_in_input = true;
    run_background(); cycle_in_input = false;
    assert(!wristflow_product_power_snapshot().input_valid); /* Reject previous wake epoch. */
    run_background();
    assert(wristflow_product_power_snapshot().input_good && !idle && !irq);
    wristflow_product_power_screen(false);
    wristflow_product_power_aod(true); /* No autonomous sample on mode entry. */
    run_background(); assert(adc_calls == 5 && !power_events);
    wristflow_product_power_request(); run_background();
    assert(adc_calls == 6 && power_events == 1 && wristflow_product_power_snapshot().input_good);
    /* Model an old recv already dispatched when a new request was installed:
     * the leftover event bit cannot execute the same request serial twice. */
    unsigned before_calls = adc_calls;
    sample_once(); assert(adc_calls == before_calls);
    wristflow_product_power_request(); wristflow_product_power_aod(false);
    wristflow_product_power_aod(true); run_background();
    assert(adc_calls == 6); /* Queued old epoch cannot run after re-entry. */
    wristflow_product_power_request(); off_in_adc = true;
    run_background(); off_in_adc = false;
    assert(power_events == 1 && !wristflow_product_power_snapshot().voltage_valid);
    wristflow_product_power_request(); run_background(); assert(adc_calls == 7);
    wristflow_product_power_aod(true); wristflow_product_power_request();
    rt_uint32_t old_bits;
    assert(rt_event_recv(&requests, 1, 3, -1, &old_bits) == RT_EOK);
    wristflow_product_power_aod(false); wristflow_product_power_aod(true);
    wristflow_product_power_request();
    sample_once(); /* Old dispatched worker claims the new pending serial once. */
    assert(adc_calls == 8 && power_events == 2);
    run_background(); assert(adc_calls == 8 && power_events == 2);
    return 0;
}
