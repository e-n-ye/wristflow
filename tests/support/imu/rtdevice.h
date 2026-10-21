#include "../power/rtdevice.h"
struct rt_i2c_configuration { unsigned mode, addr, timeout, max_hz; };
int rt_i2c_configure(struct rt_i2c_bus_device *, struct rt_i2c_configuration *);
#define PIN_MODE_INPUT 1
void *rt_device_find(const char *);
void rt_pin_mode(int, int);
int rt_pin_read(int);
#define PIN_IRQ_MODE_RISING 1
#define PIN_IRQ_DISABLE 0
#define PIN_IRQ_ENABLE 1
int rt_pin_attach_irq(int, unsigned, void (*)(void *), void *);
int rt_pin_detach_irq(int);
int rt_pin_irq_enable(int, unsigned);
