#include "../apps/product/src/product_charger_detect.c"
#include <assert.h>
#include <stdio.h>

static bool fail_query, fail_wake, fail_attach, fail_enable;
static unsigned enabled, wake_enabled, notices;
static void (*handler)(void *);
void rt_pin_mode(int pin, int mode) { assert(pin == 44 && mode == PIN_MODE_INPUT); }
int rt_pin_attach_irq(int pin, unsigned mode, void (*callback)(void *), void *arg)
{
    assert(pin == 44 && mode == PIN_IRQ_MODE_RISING_FALLING && !arg);
    if (fail_attach) return -1;
    handler = callback; return 0;
}
int rt_pin_detach_irq(int pin) { assert(pin == 44); handler = NULL; return 0; }
int rt_pin_irq_enable(int pin, unsigned value)
{
    assert(pin == 44);
    if (value && fail_enable) return -1;
    enabled = value; return 0;
}
int8_t HAL_HPAON_QueryWakeupPin(int port, int pin)
{ assert(port == 1 && pin == 44); return fail_query ? -1 : 8; }
int pm_enable_pin_wakeup(uint8_t pin, int mode)
{ assert(pin == 8 && mode == AON_PIN_MODE_DOUBLE_EDGE); if (fail_wake) return -1; ++wake_enabled; return 0; }
int pm_disable_pin_wakeup(uint8_t pin) { assert(pin == 8 && wake_enabled); --wake_enabled; return 0; }
static void notice(void) { ++notices; }
int main(void)
{
    assert(!wristflow_product_charger_detect_start(NULL));
    fail_query = true; assert(!wristflow_product_charger_detect_start(notice)); fail_query = false;
    fail_wake = true; assert(!wristflow_product_charger_detect_start(notice)); fail_wake = false;
    fail_attach = true; assert(!wristflow_product_charger_detect_start(notice)); fail_attach = false;
    assert(!wake_enabled && !enabled && !handler);
    fail_enable = true; assert(!wristflow_product_charger_detect_start(notice)); fail_enable = false;
    assert(!wake_enabled && !enabled && !handler);
    assert(wristflow_product_charger_detect_start(notice) && enabled && wake_enabled);
    assert(!wristflow_product_charger_detect_start(notice));
    handler(NULL); handler(NULL); assert(notices == 2);
    wristflow_product_charger_detect_stop();
    assert(!wake_enabled && !enabled && !handler && !notify);
    puts("charger detect: PA44 double-edge AON/IRQ, notifications and partial-init cleanup passed");
    return 0;
}
