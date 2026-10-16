#ifndef TEST_MOTOR_RTDEVICE_H
#define TEST_MOTOR_RTDEVICE_H
#include "../power/rtdevice.h"
#define PIN_MODE_INPUT 0
#define PIN_MODE_OUTPUT 1
#define PIN_LOW 0
#define PIN_HIGH 1
void rt_pin_mode(rt_base_t pin, rt_base_t mode);
void rt_pin_write(rt_base_t pin, rt_base_t value);
int rt_pin_read(rt_base_t pin);
#endif
