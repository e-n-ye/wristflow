#include "../power/rtdevice.h"
struct rt_i2c_configuration { unsigned mode, addr, timeout, max_hz; };
int rt_i2c_configure(struct rt_i2c_bus_device *, struct rt_i2c_configuration *);
