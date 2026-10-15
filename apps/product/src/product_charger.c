/* Project-owned override of the locked lchspi-ulp board's weak power hook.
 * Preserve its rail sequence; replace automatic charge enable with inhibition.
 * The independent SDK bootloader does not call this Product hook. */
#include "product_charger.h"
#include "aw32001_inhibit.h"
#include "bsp_board.h"
#include <rtthread.h>
#include <rtdevice.h>

#ifdef BSP_USING_CHARGER
#error "Product charger inhibition requires the SDK charger driver to be excluded"
#endif
#ifdef PM_STANDBY_ENABLE
#error "Charger early HAL transaction is not supported before standby Flash restore"
#endif

enum { CHARGER_ADDRESS = 0x49, HAL_TRANSFER_TIMEOUT_MS = 20 };
/* Defined by the locked board's bsp_power.c, without a public declaration. */
extern void BSP_GPIO_Set(int pin, int val, int is_porta);
static wristflow_aw32001_result_t early_result;
static HAL_StatusTypeDef early_init_status;
static bool early_attempted;

static bool early_read(void *context, uint8_t reg, uint8_t *value)
{
    I2C_HandleTypeDef *handle = context;
    /* Avoid the SDK's separate 1000 ms pre-BUSY wait when already busy.
     * A hardware change after this check can still reach that SDK wait;
     * the 20 ms argument is not a strict whole-call deadline. */
    if (__HAL_I2C_GET_FLAG(handle, I2C_FLAG_BUSY) == SET) return false;
    return HAL_I2C_Mem_Read(handle, CHARGER_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT,
                            value, 1, HAL_TRANSFER_TIMEOUT_MS) == HAL_OK;
}

static bool early_write(void *context, uint8_t reg, uint8_t value)
{
    I2C_HandleTypeDef *handle = context;
    if (__HAL_I2C_GET_FLAG(handle, I2C_FLAG_BUSY) == SET) return false;
    return HAL_I2C_Mem_Write(handle, CHARGER_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1, HAL_TRANSFER_TIMEOUT_MS) == HAL_OK;
}

/* DEEP calls this before restoring NOR Flash. Its entry and false return path
 * must stay in retained SRAM. The true path is supported only at cold startup;
 * standby requires a separate RAM/ROM-safe transaction and is compile-blocked. */
HAL_RAM_RET_CODE_SECT(wf_charger_power, void BSP_PowerUpCustom(bool is_deep_sleep))
{
    if (!is_deep_sleep) return; /* ordinary DEEP preserves the existing board path */
    BSP_GPIO_Set(38, 1, 1);
    BSP_GPIO_Set(26, 1, 1);
    BSP_GPIO_Set(30, 1, 1);
    HAL_Delay_us(500);

    early_attempted = true;
    early_result = (wristflow_aw32001_result_t){ .error = WF_AW32001_BAD_ARGUMENT };
    I2C_HandleTypeDef handle = {0};
    HAL_RCC_EnableModule(RCC_MOD_I2C2);
    handle.Instance = I2C2;
    handle.Mode = HAL_I2C_MODE_MASTER;
    handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    handle.Init.ClockSpeed = 400000;
    handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    handle.Init.OwnAddress1 = 0;
    early_init_status = HAL_I2C_Init(&handle);
    if (early_init_status == HAL_OK) {
        __HAL_I2C_ENABLE(&handle);
        const wristflow_aw32001_io_t io = { &handle, early_read, early_write };
        wristflow_aw32001_prepare(&io, &early_result);
    }
    __HAL_I2C_DISABLE(&handle);
}

void wristflow_product_charger_report(void)
{
    rt_kprintf("[charger] early attempted=%u hal=%u result=%u reg=0x%02x verified=%u profile=%u\n",
        early_attempted, (unsigned)early_init_status, (unsigned)early_result.error,
        early_result.error_reg, early_result.inhibit_verified, early_result.profile_verified);
    rt_kprintf("[charger] early REG01=0x%02x REG05=0x%02x; charging policy=inhibit\n",
        early_result.power_on, early_result.timer);
    rt_kprintf("[charger] early ID=0x%02x REG02=0x%02x REG04=0x%02x REG0b=0x%02x\n",
        early_result.chip_id, early_result.charge_current,
        early_result.charge_voltage, early_result.individual);
    rt_kprintf("[charger] bench request=%umV/%umA; past startup readback, not measured limits\n",
        (unsigned)WF_AW32001_PROFILE_MV, (unsigned)WF_AW32001_PROFILE_MA);
}
