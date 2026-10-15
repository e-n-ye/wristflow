#include "product_power.h"
#include "product_state.h"
#include <rtdevice.h>
#include <assert.h>
#include <string.h>

static struct rt_i2c_bus_device bus;
static bool no_bus, no_adc, id_fail, status_fail, enable_fail, read_fail, disable_fail;
static bool open_fail, close_fail, opened;
static uint8_t chip_id = 0x49, raw_status = 0x42;
static uint32_t raw_voltage = 38888;
static unsigned enables, disables, adc_reads, i2c_reads;
static wristflow_power_snapshot_t sample_power(void)
{
    wristflow_power_snapshot_t value = wristflow_product_power_voltage();
    wristflow_product_power_input(&value);
    return value;
}
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char *name)
{ assert(!strcmp(name, "i2c2")); return no_bus ? NULL : &bus; }
size_t rt_i2c_mem_read(struct rt_i2c_bus_device *dev, unsigned addr, unsigned reg,
                       unsigned width, void *value, size_t size)
{
    assert(dev == &bus && addr == 0x49 && width == 8 && size == 1);
    ++i2c_reads;
    assert(reg == 0x0a || reg == 0x08);
    if ((reg == 0x0a && id_fail) || (reg == 0x08 && status_fail)) return 0;
    *(uint8_t *)value = reg == 0x0a ? chip_id : raw_status;
    return 1;
}
rt_device_t rt_device_find(const char *name)
{ assert(!strcmp(name, "bat1")); return no_adc ? NULL : &bus; }
int rt_device_open(rt_device_t dev, unsigned flags)
{
    assert(dev == &bus && flags == RT_DEVICE_FLAG_RDONLY && !opened);
    if (open_fail) return -1;
    opened = true; return RT_EOK;
}
int rt_device_close(rt_device_t dev)
{
    assert(dev == &bus && opened); opened = false;
    return close_fail ? -1 : RT_EOK;
}
int rt_adc_enable(rt_adc_device_t dev, unsigned channel)
{ assert((void *)dev == &bus && channel == 7); ++enables; return enable_fail ? -1 : RT_EOK; }
int rt_adc_disable(rt_adc_device_t dev, unsigned channel)
{ assert((void *)dev == &bus && channel == 7); ++disables; return disable_fail ? -1 : RT_EOK; }
size_t rt_device_read(rt_device_t dev, unsigned channel, void *data, size_t size)
{
    assert(dev == &bus && channel == 7 && size == sizeof(uint32_t));
    if (!opened) return 0; /* Locked RT-Thread's ref_count gate. */
    ++adc_reads;
    *(uint32_t *)data = raw_voltage;
    return read_fail ? 0 : size;
}

int main(void)
{
    wristflow_power_snapshot_t sample = sample_power();
    assert(sample.input_valid && sample.input_good && sample.voltage_valid && sample.millivolts == 3889);
    assert(enables == 1 && disables == 1 && adc_reads == 1 && i2c_reads == 2);
    raw_status = 0x40;
    sample = sample_power();
    assert(sample.input_valid && !sample.input_good && sample.voltage_valid);
    assert(!strcmp(wristflow_power_input_text(&sample), "USB --"));
    id_fail = true;
    sample = sample_power();
    assert(!sample.input_valid && sample.voltage_valid);
    id_fail = false; chip_id = 0;
    assert(!sample_power().input_valid);
    chip_id = 0x49; status_fail = true;
    assert(!sample_power().input_valid);
    status_fail = false; no_bus = true;
    assert(!sample_power().input_valid);
    no_bus = false; no_adc = true;
    sample = sample_power();
    assert(sample.input_valid && !sample.voltage_valid);
    no_adc = false; enable_fail = true;
    unsigned before = adc_reads;
    assert(!sample_power().voltage_valid && adc_reads == before);
    assert(!opened);
    enable_fail = false; read_fail = true;
    before = disables;
    assert(!sample_power().voltage_valid && disables == before + 1);
    read_fail = false; disable_fail = true;
    assert(!sample_power().voltage_valid);
    disable_fail = false; raw_voltage = 50000;
    assert(!sample_power().voltage_valid);
    open_fail = true;
    assert(!sample_power().voltage_valid && !opened);
    open_fail = false; close_fail = true;
    assert(!sample_power().voltage_valid && !opened);
    close_fail = false;
    raw_voltage = UINT32_MAX;
    assert(!sample_power().voltage_valid);
    raw_voltage = 0;
    sample = sample_power();
    assert(sample.voltage_valid && sample.millivolts == 0);
    wristflow_watch_snapshot_t snapshot = wristflow_product_snapshot(false, 0);
    char text[16];
    wristflow_battery_text(&snapshot, text, sizeof text); assert(!strcmp(text, "--"));
    snapshot.power = sample;
    wristflow_battery_text(&snapshot, text, sizeof text); assert(!strcmp(text, "0.00V"));
    snapshot.power.millivolts = 3995;
    wristflow_battery_text(&snapshot, text, sizeof text); assert(!strcmp(text, "4.00V"));
    snapshot.battery_unavailable = false; snapshot.battery_percent = 53;
    wristflow_battery_text(&snapshot, text, sizeof text); assert(!strcmp(text, "53%"));
    assert(wristflow_power_equal(&sample, &sample));
    assert(!wristflow_power_equal(&sample, &snapshot.power));
    sample = (wristflow_power_snapshot_t){0};
    assert(!strcmp(wristflow_power_input_text(&sample), "USB ?"));
    return 0;
}
