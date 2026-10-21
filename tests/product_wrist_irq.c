#ifdef NDEBUG
#undef NDEBUG
#endif
#define RT_USING_PM
#include "../apps/product/src/product_wrist_irq.c"
#include <assert.h>
#include <string.h>

static unsigned lock, signals, modes, attaches, detaches, enables, disables, aon_on, aon_off;
static int attach_error, enable_error, disable_error, detach_error, aon_enable_error, aon_disable_error;
static bool missing_pin, bad_query;
static uint32_t wer;
static void (*callback)(void *);
static void *argument;
rt_base_t rt_hw_interrupt_disable(void) { return lock++; }
void rt_hw_interrupt_enable(rt_base_t old) { lock=(unsigned)old; }
static void signal_worker(void) { assert(lock); ++signals; }
void *rt_device_find(const char *name) { assert(!strcmp(name,"pin")); return missing_pin ? NULL : (void *)1; }
int rt_pin_attach_irq(int pin, unsigned mode, void (*cb)(void *), void *arg)
{
    assert(pin==31 && mode==PIN_IRQ_MODE_RISING && !lock); ++attaches;
    if (attach_error) return attach_error;
    callback=cb; argument=arg; return 0;
}
int rt_pin_detach_irq(int pin)
{ assert(pin==31 && !lock); ++detaches; return detach_error; }
int rt_pin_irq_enable(int pin, unsigned enable)
{
    assert(pin==31 && !lock);
    if (enable) { ++enables; return enable_error; }
    ++disables; return disable_error;
}
void HAL_PIN_Set(unsigned pin, unsigned function, unsigned pull, unsigned cpu)
{ assert(pin==31 && function==GPIO_A31 && pull==PIN_PULLDOWN && cpu==1 && !lock); }
void rt_pin_mode(int pin,int mode) { assert(pin==31 && mode==PIN_MODE_INPUT && !lock); ++modes; }
int8_t HAL_HPAON_QueryWakeupPin(void *gpio,uint16_t pin)
{ assert(gpio==(void *)1 && pin==31); return bad_query ? -1 : 7; }
uint32_t test_wrist_wer(void) { return wer; }
int pm_enable_pin_wakeup(uint8_t pin,unsigned mode)
{ assert(pin==7 && mode==AON_PIN_MODE_POS_EDGE); ++aon_on; return aon_enable_error; }
int pm_disable_pin_wakeup(uint8_t pin)
{ assert(pin==7 && !lock); ++aon_off; return aon_disable_error; }
int main(void)
{
    assert(!wristflow_product_wrist_irq_start(NULL));
    missing_pin=true; assert(!wristflow_product_wrist_irq_start(signal_worker)); missing_pin=false;
    attach_error=7; assert(!wristflow_product_wrist_irq_start(signal_worker)); attach_error=0;
    assert(attaches==1 && !modes && !detaches && !disables && !aon_off);
    bad_query=true; assert(!wristflow_product_wrist_irq_start(signal_worker)); bad_query=false;
    assert(!modes && detaches==1 && !disables && !aon_off);
    wer=HPSYS_AON_WER_PIN0<<7; assert(!wristflow_product_wrist_irq_start(signal_worker)); wer=0;
    assert(!modes && detaches==2 && !disables && !aon_off);
    aon_enable_error=3; assert(!wristflow_product_wrist_irq_start(signal_worker)); aon_enable_error=0;
    assert(aon_on==1 && aon_off==1 && !wristflow_product_wrist_irq_snapshot().owned);
    enable_error=4; assert(!wristflow_product_wrist_irq_start(signal_worker)); enable_error=0;
    assert(disables==1 && !wristflow_product_wrist_irq_snapshot().owned);
    assert(wristflow_product_wrist_irq_start(signal_worker));
    assert(wristflow_product_wrist_irq_snapshot().armed);
    callback(argument); assert(signals==1);
    void *old_argument=argument;
    disable_error=5; aon_disable_error=6;
    assert(!wristflow_product_wrist_irq_stop()); callback(argument); assert(signals==1);
    assert(!wristflow_product_wrist_irq_snapshot().armed && wristflow_product_wrist_irq_snapshot().owned);
    assert(!wristflow_product_wrist_irq_start(signal_worker));
    disable_error=aon_disable_error=0; detach_error=8;
    assert(!wristflow_product_wrist_irq_stop() && wristflow_product_wrist_irq_snapshot().owned);
    detach_error=0; assert(wristflow_product_wrist_irq_stop());
    assert(!wristflow_product_wrist_irq_snapshot().owned);
    assert(wristflow_product_wrist_irq_start(signal_worker));
    callback(old_argument); assert(signals==1); /* Old generation cannot notify a new owner. */
    callback(argument); assert(signals==2 && wristflow_product_wrist_irq_snapshot().irqs==2);
    aon_disable_error=9; unsigned old_detaches=detaches;
    assert(!wristflow_product_wrist_irq_stop());
    assert(detaches==old_detaches+1 && wristflow_product_wrist_irq_snapshot().owned);
    aon_disable_error=0; assert(wristflow_product_wrist_irq_stop());
    assert(!wristflow_product_wrist_irq_snapshot().owned && !lock);
    return 0;
}
