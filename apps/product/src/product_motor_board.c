#include "product_motor.h"
#include <rtdevice.h>
#include <rthw.h>
#include "bf0_hal.h"
#include "drv_gpio.h"

/* Official Huangshan V1.2 design: PA20 -> R37 -> Q2 gate, active high.
 * MT1 uses VDD33_VOUT/R36; PCB assembly and load limits still need verification.
 * Do not use the SDK generic motor defaults (44/45): PA44 is VBUS_DET here. */
#define MOTOR_PIN GET_PIN(1, 20)
#ifdef RGB_SK6812MINI_HS_ENABLE
#error "Motor supply borrowing requires RGB disabled; add a shared owner before enabling RGB."
#endif
#define SUPPLY_CONTROL (PMUC_PERI_LDO_EN_VDD33_LDO3_Msk | PMUC_PERI_LDO_VDD33_LDO3_PD_Msk)
#define SUPPLY_CONFIG (SUPPLY_CONTROL | PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Msk)
static bool configured;
static bool supply_leased, supply_changed, supply_prepared;
static uint32_t saved_supply;

static uint32_t active_supply(void)
{
    return (saved_supply & PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Msk) | PMUC_PERI_LDO_EN_VDD33_LDO3_Msk;
}

static bool release_supply(void)
{
    if (!supply_leased) return true;
    if (!supply_changed) { supply_leased = false; return true; }
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t raw = hwp_pmuc->PERI_LDO;
    bool ok = (raw & SUPPLY_CONFIG) == saved_supply;
    if (!ok && (raw & SUPPLY_CONFIG) == active_supply()) {
        /* HAL disable forces EN0/PD1; restore the original EN/PD exactly. */
        MODIFY_REG(hwp_pmuc->PERI_LDO, SUPPLY_CONTROL, saved_supply & SUPPLY_CONTROL);
        ok = (hwp_pmuc->PERI_LDO & SUPPLY_CONFIG) == saved_supply;
    }
    if (ok) supply_leased = supply_changed = false;
    rt_hw_interrupt_enable(level);
    return ok;
}

static bool initialize_off(void *context)
{
    (void)context;
    configured = false;
    supply_prepared = false;
    rt_pin_mode(MOTOR_PIN, PIN_MODE_INPUT);
    rt_pin_write(MOTOR_PIN, PIN_LOW); /* Preload LOW before selecting/enabling output. */
    if (HAL_PIN_Set(PAD_PA20, GPIO_A20, PIN_PULLDOWN, 1) != 0) return false;
    rt_pin_mode(MOTOR_PIN, PIN_MODE_OUTPUT);
    configured = rt_pin_read(MOTOR_PIN) == PIN_LOW;
    return configured && release_supply();
}

static bool prepare(void *context)
{
    (void)context;
    if (!configured || supply_leased || rt_pin_read(MOTOR_PIN) != PIN_LOW) return false;
    supply_prepared = false;
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t raw = hwp_pmuc->PERI_LDO;
    if ((raw & SUPPLY_CONTROL) == SUPPLY_CONTROL) {
        rt_hw_interrupt_enable(level); return false;
    }
    saved_supply = raw & SUPPLY_CONFIG;
    supply_changed = !(raw & PMUC_PERI_LDO_EN_VDD33_LDO3_Msk);
    /* Keep ownership before any partial write so OFF can compensate failures. */
    supply_leased = true;
    bool ok = !supply_changed || HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, true, false) == HAL_OK;
    ok = ok && (hwp_pmuc->PERI_LDO & SUPPLY_CONFIG) == active_supply();
    rt_hw_interrupt_enable(level);
    if (!ok) return false;
    /* Match the SDK settling delay, outside the short register critical section. */
    HAL_Delay_us(5000);
    level = rt_hw_interrupt_disable();
    supply_prepared = (hwp_pmuc->PERI_LDO & SUPPLY_CONFIG) == active_supply();
    rt_hw_interrupt_enable(level);
    return supply_prepared;
}

static bool set_enabled(void *context, bool enabled)
{
    if (!configured) return !enabled && initialize_off(context);
    if (enabled && (!supply_leased || !supply_prepared ||
            (hwp_pmuc->PERI_LDO & SUPPLY_CONFIG) != active_supply())) return false;
    if (!enabled) supply_prepared = false;
    rt_pin_write(MOTOR_PIN, enabled ? PIN_HIGH : PIN_LOW);
    /* The fixed SDK reads the DOR latch in output mode, not physical pad voltage. */
    bool matched = rt_pin_read(MOTOR_PIN) == (enabled ? PIN_HIGH : PIN_LOW);
    return matched && (enabled || release_supply());
}
const wristflow_motor_io_t *wristflow_product_motor_board_io(void)
{
    static const wristflow_motor_io_t backend = {
        .context = NULL, .initialize_off = initialize_off,
        .set_enabled = set_enabled, .prepare = prepare
    };
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
    rt_kprintf("[motor-power] LDO3=VOUT2 design; physical voltage/current/motion unmeasured\n");
    return RT_EOK;
}
MSH_CMD_EXPORT(wf_motor_power, Read-only peripheral LDO requests without enabling the supply or motor);
