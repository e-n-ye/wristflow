#include "product_charger_bus.h"
#include <rtdevice.h>
#include "bsp_board.h"

#ifndef LCD_USING_CO5300
#error "Shared charger/LCD lease is scoped to the locked CO5300 Huangshan board"
#endif

/* Locked sf32lb52-lchspi-ulp_base/bsp_lcd_tp.c owns PA26 and remaps PA10/11
 * on LCD power-down. R42/R43 pull charger I2C up to shared VCC_3V3. Serialize
 * LCD transitions with all registered I2C clients; never blink that rail under
 * an active/unconfirmed charger session. No SDK source is modified. */
static bool leased;
static bool prepared;
static bool lcd_on = true;
void __real_BSP_LCD_PowerDown(void);
void __real_BSP_LCD_PowerUp(void);

bool wristflow_product_charger_bus_prepare_locked(void)
{
    if (leased && prepared) return true;
    /* Mark responsibility before partial configuration; a failed stop retains
     * the lease. Rail first, then pull-ups/mux, to avoid powering an off rail. */
    leased = true;
    BSP_GPIO_Set(26, 1, 1);
    rt_thread_mdelay(5);
    prepared = HAL_PIN_Set(PAD_PA10, I2C2_SCL, PIN_PULLUP, 1) == 0;
    prepared = HAL_PIN_Set(PAD_PA11, I2C2_SDA, PIN_PULLUP, 1) == 0 && prepared;
    return prepared;
}

bool wristflow_product_charger_bus_release(void)
{
    struct rt_i2c_bus_device *bus = rt_i2c_bus_device_find("i2c2");
    if (!bus) return !leased;
    if (rt_mutex_take(&bus->lock, RT_WAITING_FOREVER) != RT_EOK) return false;
    if (leased && !lcd_on) {
        prepared = false;
        /* Match the board's off state, with internal pull-ups removed first. */
        bool ok = HAL_PIN_Set(PAD_PA10, GPIO_A10, PIN_PULLDOWN, 1) == 0;
        ok = HAL_PIN_Set(PAD_PA11, GPIO_A11, PIN_PULLDOWN, 1) == 0 && ok;
        if (!ok) { rt_mutex_release(&bus->lock); return false; }
#ifndef RT_USING_SPI_MSD
        BSP_GPIO_Set(26, 0, 1);
#endif
    }
    leased = false;
    prepared = false;
    rt_mutex_release(&bus->lock);
    return true;
}

static struct rt_i2c_bus_device *lcd_lock(void)
{
    struct rt_i2c_bus_device *bus = rt_i2c_bus_device_find("i2c2");
    if (bus && rt_mutex_take(&bus->lock, RT_WAITING_FOREVER) != RT_EOK) {
        rt_kprintf("[charger-bus] LCD transition lock failed; retain shared power\n");
        return RT_NULL;
    }
    return bus;
}

void __wrap_BSP_LCD_PowerDown(void)
{
    struct rt_i2c_bus_device *bus = lcd_lock();
    if (!bus) return; /* Product always configures i2c2; do not cut unknown ownership. */
    if (!leased) __real_BSP_LCD_PowerDown();
    else {
        /* Same locked board's CO5300 sequence, omitting only PA26 LOW and the
         * PA10/11 GPIO remap. The panel/reset/audio and LCD pins still go off. */
        BSP_GPIO_Set(0, 0, 1);
        BSP_GPIO_Set(1, 0, 1);
        BSP_GPIO_Set(42, 0, 1);
        const int pads[] = { PAD_PA00, PAD_PA01, PAD_PA02, PAD_PA03, PAD_PA04,
            PAD_PA05, PAD_PA06, PAD_PA07, PAD_PA08, PAD_PA42 };
        const int pins[] = { GPIO_A0, GPIO_A1, GPIO_A2, GPIO_A3, GPIO_A4,
            GPIO_A5, GPIO_A6, GPIO_A7, GPIO_A8, GPIO_A42 };
        for (unsigned i = 0; i < sizeof pads / sizeof *pads; ++i)
            HAL_PIN_Set(pads[i], pins[i], PIN_PULLDOWN, 1);
    }
    lcd_on = false;
    rt_mutex_release(&bus->lock);
}

void __wrap_BSP_LCD_PowerUp(void)
{
    struct rt_i2c_bus_device *bus = lcd_lock();
    if (!bus) return;
    __real_BSP_LCD_PowerUp();
    lcd_on = true;
    rt_mutex_release(&bus->lock);
}
