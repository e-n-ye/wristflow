#include "product_charger_detect.h"
#include <rtdevice.h>
#include "bf0_hal.h"
#include "bf0_pm.h"
#include "drv_gpio.h"

/* Locked Huangshan board.conf / bsp_pinmux: PA44 is VBUS_DET. Only restore
 * the SDK's detection/wakeup path, never its automatic 4215mV setup. */
#define DETECT_PIN GET_PIN(1, 44)
static void (*notify)(void);
static int8_t wake_pin = -1;
static bool attached;
static void changed(void *context)
{ (void)context; if (notify) notify(); }

void wristflow_product_charger_detect_stop(void)
{
    rt_pin_irq_enable(DETECT_PIN, 0);
    if (attached) rt_pin_detach_irq(DETECT_PIN);
    attached = false;
    if (wake_pin >= 0) pm_disable_pin_wakeup(wake_pin);
    wake_pin = -1;
    notify = RT_NULL;
}

bool wristflow_product_charger_detect_start(void (*callback)(void))
{
    if (!callback || attached) return false;
    rt_pin_mode(DETECT_PIN, PIN_MODE_INPUT);
    wake_pin = HAL_HPAON_QueryWakeupPin(GET_GPIO_INSTANCE(DETECT_PIN), GET_GPIOx_PIN(DETECT_PIN));
    if (wake_pin < 0 || pm_enable_pin_wakeup(wake_pin, AON_PIN_MODE_DOUBLE_EDGE) != RT_EOK) {
        wake_pin = -1;
        return false;
    }
    notify = callback;
    if (rt_pin_attach_irq(DETECT_PIN, PIN_IRQ_MODE_RISING_FALLING, changed, RT_NULL) != RT_EOK)
        goto failed;
    attached = true;
    if (rt_pin_irq_enable(DETECT_PIN, 1) != RT_EOK) goto failed;
    return true;
failed:
    wristflow_product_charger_detect_stop();
    return false;
}
