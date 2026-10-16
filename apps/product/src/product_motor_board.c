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

/* One register read only: do not configure the shared motor/RGB supply here.
 * EN/PD/SET_VOUT are requests, not output-voltage or load-current feedback. */
static int wf_motor_power(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        rt_kprintf("wf_motor_power; read-only peripheral LDO register requests\n");
        return -RT_EINVAL;
    }
    uint32_t raw = hwp_pmuc->PERI_LDO;
    unsigned en2 = (raw & PMUC_PERI_LDO_EN_VDD33_LDO2_Msk) != 0;
    unsigned pd2 = (raw & PMUC_PERI_LDO_VDD33_LDO2_PD_Msk) != 0;
    unsigned code2 = (raw & PMUC_PERI_LDO_VDD33_LDO2_SET_VOUT_Msk)
        >> PMUC_PERI_LDO_VDD33_LDO2_SET_VOUT_Pos;
    unsigned en3 = (raw & PMUC_PERI_LDO_EN_VDD33_LDO3_Msk) != 0;
    unsigned pd3 = (raw & PMUC_PERI_LDO_VDD33_LDO3_PD_Msk) != 0;
    unsigned code3 = (raw & PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Msk)
        >> PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Pos;
    rt_kprintf("[motor-power] PERI_LDO=0x%08lx\n", (unsigned long)raw);
    rt_kprintf("[motor-power] LDO2 en_req=%u pd_req=%u vout_code=%u\n", en2, pd2, code2);
    rt_kprintf("[motor-power] LDO3 en_req=%u pd_req=%u vout_code=%u\n", en3, pd3, code3);
    rt_kprintf("[motor-power] requests only; VOUT2 mapping/voltage/current/motion unverified\n");
    return RT_EOK;
}
MSH_CMD_EXPORT(wf_motor_power, Read-only peripheral LDO requests without enabling the supply or motor);
