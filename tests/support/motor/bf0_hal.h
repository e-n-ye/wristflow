#ifndef TEST_MOTOR_BF0_HAL_H
#define TEST_MOTOR_BF0_HAL_H
#include <stdint.h>
#include <stdbool.h>
#define __IO volatile
#include "pmuc.h"
/* Direct assignments must fail; the explicit masked restore is instrumented. */
const PMUC_TypeDef *wristflow_test_motor_pmuc(void);
#define hwp_pmuc wristflow_test_motor_pmuc()
void wristflow_test_motor_modify(const volatile uint32_t *, uint32_t, uint32_t);
#define MODIFY_REG(reg, mask, value) wristflow_test_motor_modify(&(reg), (mask), (value))
typedef enum { PMU_PERI_LDO3_3V3 = PMUC_PERI_LDO_EN_VDD33_LDO3_Pos } PMU_PeriLdoTypeDef;
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
HAL_StatusTypeDef HAL_PMU_ConfigPeriLdo(PMU_PeriLdoTypeDef, bool, bool);
void HAL_Delay_us(uint32_t);
#define PAD_PA20 21
#define GPIO_A20 120
#define PIN_PULLDOWN 2
int HAL_PIN_Set(int pad, int function, int flags, int hcpu);
#endif
