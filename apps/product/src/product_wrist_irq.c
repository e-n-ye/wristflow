#include "product_wrist_irq.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>
#include <board.h>
#include "bf0_pm.h"
#include "drv_gpio.h"

#define WRIST_PIN GET_PIN(1, 31)
static bool attached, gpio_owned, aon_owned, armed;
static uint32_t generation, irqs;
static int8_t wake_pin = -1;
static void (*notify_worker)(void);

static void interrupt(void *cookie)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool current = armed && (uintptr_t)cookie == generation;
    if (current) ++irqs;
    void (*signal)(void) = current ? notify_worker : NULL;
    if (signal) signal();
    rt_hw_interrupt_enable(level);
}

wristflow_wrist_irq_snapshot_t wristflow_product_wrist_irq_snapshot(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wristflow_wrist_irq_snapshot_t s = {armed, attached || gpio_owned || aon_owned, irqs};
    rt_hw_interrupt_enable(level);
    return s;
}

bool wristflow_product_wrist_irq_stop(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    armed = false;
    notify_worker = NULL;
    rt_hw_interrupt_enable(level);
    bool ok = true;
#ifdef RT_USING_PM
    if (aon_owned) {
        if (pm_disable_pin_wakeup(wake_pin) == RT_EOK) aon_owned = false;
        else ok = false;
    }
#endif
    if (gpio_owned) {
        if (rt_pin_irq_enable(WRIST_PIN, PIN_IRQ_DISABLE) == RT_EOK) gpio_owned = false;
        else ok = false;
    }
    if (attached && !gpio_owned) {
        if (rt_pin_detach_irq(WRIST_PIN) == RT_EOK) attached = false;
        else ok = false;
    }
    return ok;
}

bool wristflow_product_wrist_irq_start(void (*signal)(void))
{
    if (!signal || attached || gpio_owned || aon_owned || !rt_device_find("pin")) return false;
    uint32_t next = generation + 1U;
    if (!next) next = 1;
    /* Claim the callback before touching PA31. EBUSY must leave its owner alone. */
    if (rt_pin_attach_irq(WRIST_PIN, PIN_IRQ_MODE_RISING, interrupt, (void *)(uintptr_t)next) != RT_EOK)
        return false;
    attached = true;
    generation = next;
#ifdef RT_USING_PM
    wake_pin = HAL_HPAON_QueryWakeupPin(GET_GPIO_INSTANCE(WRIST_PIN), GET_GPIOx_PIN(WRIST_PIN));
    if (wake_pin < 0) goto fail;
    /* No AON reference counting: refuse an existing source instead of taking
     * responsibility for somebody else's enabled wakeup. PA31 maps to pin7. */
    if (HAL_HPAON_GET_WER() & (HPSYS_AON_WER_PIN0 << wake_pin)) goto fail;
#endif
    HAL_PIN_Set(PAD_PA31, GPIO_A31, PIN_PULLDOWN, 1);
    rt_pin_mode(WRIST_PIN, PIN_MODE_INPUT);
#ifdef RT_USING_PM
    /* Treat even a failed enable as a cleanup responsibility. */
    aon_owned = true;
    if (pm_enable_pin_wakeup(wake_pin, AON_PIN_MODE_POS_EDGE) != RT_EOK) goto fail;
#endif
    rt_base_t level = rt_hw_interrupt_disable();
    notify_worker = signal;
    armed = true;
    rt_hw_interrupt_enable(level);
    gpio_owned = true;
    /* SDK rising mode applies PULLDOWN too; a latched high is checked by worker. */
    if (rt_pin_irq_enable(WRIST_PIN, PIN_IRQ_ENABLE) != RT_EOK) goto fail;
    return true;
fail:
    (void)wristflow_product_wrist_irq_stop();
    return false;
}
