#include "../apps/product/src/product_charger_bus.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static struct rt_i2c_bus_device bus;
static int mux[43], gpio[43];
static unsigned delays, real_down, real_up, low26, ticks;
static int fail_pad = -1;
static bool missing_bus, fail_lock;

struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char *name)
{ assert(!strcmp(name, "i2c2")); return missing_bus ? NULL : &bus; }
int rt_mutex_take(struct rt_mutex *m, int timeout)
{
    assert(m == &bus.lock && timeout == RT_WAITING_FOREVER);
    if (fail_lock) return -1;
    ++m->held; return RT_EOK;
}
int rt_mutex_release(struct rt_mutex *m)
{ assert(m == &bus.lock && m->held); --m->held; return RT_EOK; }
int rt_thread_mdelay(rt_int32_t ms)
{ assert(bus.lock.held && ms == 5 && gpio[26] == 1); ++delays; ticks += ms; return 0; }
void rt_kprintf(const char *format, ...) { (void)format; }
void BSP_GPIO_Set(uint16_t pin, uint8_t value, int hcpu)
{
    assert(bus.lock.held && hcpu == 1);
    if (pin == 26 && !value) ++low26;
    gpio[pin] = value;
}
int HAL_PIN_Set(int pad, int function, int pull, int hcpu)
{
    assert(bus.lock.held && hcpu == 1);
    if (function == I2C2_SCL || function == I2C2_SDA) {
        assert(gpio[26] == 1 && ticks >= 5 && pull == PIN_PULLUP);
    }
    if (pad == fail_pad) return -1;
    mux[pad] = function;
    return 0;
}
/* Deliberately model the SDK's destructive shared-rail/mux behavior. Linked
 * wrappers must keep it away from the active I2C lease. */
void BSP_LCD_PowerDown(void)
{
    assert(bus.lock.held); ++real_down;
    BSP_GPIO_Set(0, 0, 1); BSP_GPIO_Set(1, 0, 1); BSP_GPIO_Set(26, 0, 1);
    HAL_PIN_Set(PAD_PA10, GPIO_A10, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA11, GPIO_A11, PIN_PULLDOWN, 1);
}
void BSP_LCD_PowerUp(void)
{
    assert(bus.lock.held); ++real_up;
    BSP_GPIO_Set(0, 1, 1); BSP_GPIO_Set(1, 1, 1); BSP_GPIO_Set(26, 1, 1);
    rt_thread_mdelay(5);
    HAL_PIN_Set(PAD_PA10, I2C2_SCL, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA11, I2C2_SDA, PIN_PULLUP, 1);
}
/* References originate in a separate translation unit in the firmware; use
 * explicit wrapper entry points here, while --wrap resolves __real symbols. */
int main(void)
{
    __wrap_BSP_LCD_PowerUp(); assert(real_up == 1 && lcd_on && gpio[26]);
    __wrap_BSP_LCD_PowerDown(); assert(real_down == 1 && !lcd_on && !gpio[26]);
    assert(rt_mutex_take(&bus.lock, RT_WAITING_FOREVER) == RT_EOK);
    assert(wristflow_product_charger_bus_prepare_locked());
    unsigned old_delays = delays;
    assert(wristflow_product_charger_bus_prepare_locked() && delays == old_delays);
    rt_mutex_release(&bus.lock);
    unsigned old_low = low26;
    __wrap_BSP_LCD_PowerDown();
    assert(real_down == 1 && low26 == old_low && !lcd_on);
    assert(gpio[26] == 1 && mux[10] == I2C2_SCL && mux[11] == I2C2_SDA);
    assert(!gpio[0] && !gpio[1] && !gpio[42] && mux[0] == GPIO_A0 && mux[42] == GPIO_A42);
    assert(wristflow_product_charger_bus_release());
    assert(!leased && !prepared && !gpio[26] && mux[10] == GPIO_A10 && mux[11] == GPIO_A11);
    assert(rt_mutex_take(&bus.lock, RT_WAITING_FOREVER) == RT_EOK);
    assert(wristflow_product_charger_bus_prepare_locked());
    rt_mutex_release(&bus.lock);
    __wrap_BSP_LCD_PowerUp();
    old_low = low26; assert(wristflow_product_charger_bus_release());
    assert(lcd_on && gpio[26] && low26 == old_low); /* never cut a lit LCD's rail */
    __wrap_BSP_LCD_PowerDown();
    fail_pad = PAD_PA11;
    rt_mutex_take(&bus.lock, RT_WAITING_FOREVER);
    assert(!wristflow_product_charger_bus_prepare_locked() && leased && !prepared);
    fail_pad = -1;
    assert(wristflow_product_charger_bus_prepare_locked() && prepared); /* stop can recover partial setup */
    rt_mutex_release(&bus.lock);
    fail_pad = PAD_PA10;
    assert(!wristflow_product_charger_bus_release() && leased && !prepared && gpio[26]);
    fail_pad = -1;
    assert(wristflow_product_charger_bus_release() && !leased && !gpio[26]);
    missing_bus = true;
    assert(wristflow_product_charger_bus_release());
    old_low = low26; __wrap_BSP_LCD_PowerDown(); assert(low26 == old_low);
    missing_bus = false; fail_lock = true;
    __wrap_BSP_LCD_PowerDown(); assert(low26 == old_low);
    assert(!wristflow_product_charger_bus_release());
    assert(!bus.lock.held);
    puts("charger bus: serialized LCD rail/mux lease, no active power glitch, off return and partial-failure recovery passed");
    return 0;
}
