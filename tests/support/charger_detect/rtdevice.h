#ifndef TEST_CHARGER_DETECT_RTDEVICE_H
#define TEST_CHARGER_DETECT_RTDEVICE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define RT_NULL NULL
#define RT_EOK 0
#define PIN_MODE_INPUT 0
#define PIN_IRQ_MODE_RISING_FALLING 3
#define AON_PIN_MODE_DOUBLE_EDGE 3
#define GET_PIN(port, pin) (pin)
#define GET_GPIO_INSTANCE(pin) 1
#define GET_GPIOx_PIN(pin) (pin)
void rt_pin_mode(int, int);
int rt_pin_attach_irq(int, unsigned, void (*)(void *), void *);
int rt_pin_detach_irq(int);
int rt_pin_irq_enable(int, unsigned);
int8_t HAL_HPAON_QueryWakeupPin(int, int);
int pm_enable_pin_wakeup(uint8_t, int);
int pm_disable_pin_wakeup(uint8_t);
#endif
