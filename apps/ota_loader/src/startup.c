/* Preserve the known board rail sequence, replacing its early charge-enable.
 * This probe has no automatic charging worker or LCD/audio lifecycle. */
#include "startup.h"
#include "aw32001_inhibit.h"
#include "bsp_board.h"
#include <rtthread.h>

#if defined(BSP_USING_CHARGER) || defined(CHARGE_USING_AW32001) || defined(BSP_USING_PM) || defined(RT_USING_PM) || defined(BSP_USING_LCD) || defined(BSP_USING_TOUCHD)
#error "Loader probe requires charger, PM, LCD and touch drivers excluded"
#endif
extern void BSP_GPIO_Set(int pin, int val, int is_porta);
static wristflow_aw32001_result_t result;
static bool attempted;
static bool transfer_read(void *context, uint8_t reg, uint8_t *value)
{
    I2C_HandleTypeDef *h = context;
    if (__HAL_I2C_GET_FLAG(h, I2C_FLAG_BUSY) == SET) return false;
    return HAL_I2C_Mem_Read(h, 0x49, reg, I2C_MEMADD_SIZE_8BIT, value, 1, 20) == HAL_OK;
}
static bool transfer_write(void *context, uint8_t reg, uint8_t value)
{
    I2C_HandleTypeDef *h = context;
    if (__HAL_I2C_GET_FLAG(h, I2C_FLAG_BUSY) == SET) return false;
    return HAL_I2C_Mem_Write(h, 0x49, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 20) == HAL_OK;
}
HAL_RAM_RET_CODE_SECT(wf_ota_startup, void BSP_PowerUpCustom(bool cold))
{
    attempted = false;
    if (!cold) return;
    BSP_GPIO_Set(38, 1, 1);
    BSP_GPIO_Set(26, 1, 1);
    BSP_GPIO_Set(30, 1, 1);
    HAL_Delay_us(500);
    attempted = true;
    result = (wristflow_aw32001_result_t){.error=WF_AW32001_BAD_ARGUMENT};
    I2C_HandleTypeDef h = {0};
    HAL_RCC_EnableModule(RCC_MOD_I2C2);
    h.Instance = I2C2;
    h.Mode = HAL_I2C_MODE_MASTER;
    h.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    h.Init.ClockSpeed = 400000;
    h.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    if (HAL_I2C_Init(&h) == HAL_OK) {
        __HAL_I2C_ENABLE(&h);
        const wristflow_aw32001_io_t io = {&h, transfer_read, transfer_write};
        wristflow_aw32001_inhibit(&io, &result);
    }
    __HAL_I2C_DISABLE(&h);
}
bool wf_ota_loader_inhibited(void) { return attempted && result.inhibit_verified; }
void wf_ota_loader_startup_report(void)
{
    rt_kprintf("[ota-startup] attempted=%u inhibit=%u error=%u; past readback, not physical current proof\n",
               attempted, result.inhibit_verified, (unsigned)result.error);
}
