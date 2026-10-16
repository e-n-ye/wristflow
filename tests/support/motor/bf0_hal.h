#ifndef TEST_MOTOR_BF0_HAL_H
#define TEST_MOTOR_BF0_HAL_H
#define PAD_PA20 21
#define GPIO_A20 120
#define PIN_PULLDOWN 2
int HAL_PIN_Set(int pad, int function, int flags, int hcpu);
#endif
