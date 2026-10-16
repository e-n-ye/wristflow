#ifndef TEST_MOTOR_BF0_HAL_H
#define TEST_MOTOR_BF0_HAL_H
#include <stdint.h>
#define __IO volatile
#include "pmuc.h"
/* Compile production register access through a const view: writes must fail. */
const PMUC_TypeDef *wristflow_test_motor_pmuc(void);
#define hwp_pmuc wristflow_test_motor_pmuc()
#define PAD_PA20 21
#define GPIO_A20 120
#define PIN_PULLDOWN 2
int HAL_PIN_Set(int pad, int function, int flags, int hcpu);
#endif
