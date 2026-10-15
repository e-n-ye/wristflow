#include "product_power.h"
#include <rtthread.h>
#include <rtdevice.h>

#ifdef BSP_USING_CHARGER
#error "Power observation must not enable the SDK automatic charger"
#endif

void wristflow_product_power_input(wristflow_power_snapshot_t *sample)
{
    sample->input_valid = false;
    sample->input_good = false;
    struct rt_i2c_bus_device *bus = rt_i2c_bus_device_find("i2c2");
    uint8_t id = 0, status = 0;
    /* AW32001E V1.4 p39: REG08 bit1 is PG, not battery presence or charging.
     * Registered bus API serializes each transfer; no raw HAL after startup. */
    if (bus && rt_i2c_mem_read(bus, 0x49, 0x0a, 8, &id, 1) == 1 && id == 0x49 &&
        rt_i2c_mem_read(bus, 0x49, 0x08, 8, &status, 1) == 1) {
        sample->input_valid = true;
        sample->input_good = (status & 2U) != 0;
    }
}

wristflow_power_snapshot_t wristflow_product_power_voltage(void)
{
    wristflow_power_snapshot_t sample = {0};
    rt_adc_device_t adc = (rt_adc_device_t)rt_device_find("bat1");
    if (adc && rt_device_open((rt_device_t)adc, RT_DEVICE_FLAG_RDONLY) == RT_EOK) {
        rt_uint32_t decimillivolts = 0;
        /* rt_adc_read discards conversion errors in the locked SDK. The device
         * read propagates them. This path already applies the VBAT factor. */
        bool enabled = rt_adc_enable(adc, 7) == RT_EOK;
        bool read = enabled && rt_device_read((rt_device_t)adc, 7, &decimillivolts,
                                              sizeof decimillivolts) == sizeof decimillivolts;
        bool disabled = !enabled || rt_adc_disable(adc, 7) == RT_EOK;
        bool closed = rt_device_close((rt_device_t)adc) == RT_EOK;
        if (read && disabled && closed && decimillivolts < 50000U) {
            sample.voltage_valid = true;
            sample.millivolts = (uint16_t)((decimillivolts + 5U) / 10U);
        }
    }
    return sample; /* Failed observations replace old values with unknown. */
}
