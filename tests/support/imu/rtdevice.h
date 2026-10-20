#include "../power/rtdevice.h"
struct rt_i2c_configuration { unsigned mode, addr, timeout, max_hz; };
int rt_i2c_configure(struct rt_i2c_bus_device *, struct rt_i2c_configuration *);
#define PIN_MODE_INPUT 1
void *rt_device_find(const char *);
void rt_pin_mode(int, int);
int rt_pin_read(int);
