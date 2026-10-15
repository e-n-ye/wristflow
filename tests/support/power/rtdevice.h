#ifndef TEST_POWER_RTDEVICE_H
#define TEST_POWER_RTDEVICE_H
#include "rtthread.h"
#include <stddef.h>
typedef struct rt_adc_device *rt_adc_device_t;
typedef void *rt_device_t;
#define RT_DEVICE_FLAG_RDONLY 1
#define PM_SLEEP_MODE_IDLE 1
void rt_pm_request(unsigned);
void rt_pm_release(unsigned);
struct rt_i2c_bus_device { struct rt_mutex lock; };
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char *name);
size_t rt_i2c_mem_read(struct rt_i2c_bus_device *, unsigned, unsigned, unsigned, void *, size_t);
size_t rt_i2c_mem_write(struct rt_i2c_bus_device *, unsigned, unsigned, unsigned, void *, size_t);
rt_device_t rt_device_find(const char *name);
int rt_device_open(rt_device_t, unsigned);
int rt_device_close(rt_device_t);
int rt_adc_enable(rt_adc_device_t, unsigned);
int rt_adc_disable(rt_adc_device_t, unsigned);
size_t rt_device_read(rt_device_t, unsigned, void *, size_t);
#endif
