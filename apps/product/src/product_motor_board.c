#include "product_motor.h"
#include <rtdevice.h>
#include "bf0_hal.h"
#include "drv_gpio.h"

/* Official Huangshan V1.2 design: PA20 -> R37 -> Q2 gate, active high.
 * MT1 uses VDD33_VOUT/R36; PCB assembly and load limits still need verification.
 * Do not use the SDK generic motor defaults (44/45): PA44 is VBUS_DET here. */
#define MOTOR_PIN GET_PIN(1, 20)
static bool configured;
static bool initialize_off(void *context)
{
    (void)context;
    configured = false;
    rt_pin_mode(MOTOR_PIN, PIN_MODE_INPUT);
    rt_pin_write(MOTOR_PIN, PIN_LOW); /* Preload LOW before selecting/enabling output. */
    if (HAL_PIN_Set(PAD_PA20, GPIO_A20, PIN_PULLDOWN, 1) != 0) return false;
    rt_pin_mode(MOTOR_PIN, PIN_MODE_OUTPUT);
    configured = rt_pin_read(MOTOR_PIN) == PIN_LOW;
    return configured;
}
static bool set_enabled(void *context, bool enabled)
{
    if (!configured) return !enabled && initialize_off(context);
    rt_pin_write(MOTOR_PIN, enabled ? PIN_HIGH : PIN_LOW);
    /* The fixed SDK reads the DOR latch in output mode, not physical pad voltage. */
    return rt_pin_read(MOTOR_PIN) == (enabled ? PIN_HIGH : PIN_LOW);
}
const wristflow_motor_io_t *wristflow_product_motor_board_io(void)
{
    static const wristflow_motor_io_t backend = {NULL, initialize_off, set_enabled};
    return &backend;
}
