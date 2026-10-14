#include "aw32001_inhibit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t registers[6];
    unsigned transfers;
    unsigned fail_at;
    uint8_t ignore_write_reg;
    bool lost_inhibit;
} fake_bus_t;

static bool read_register(void *context, uint8_t reg, uint8_t *value)
{
    fake_bus_t *bus = context;
    if (++bus->transfers == bus->fail_at) return false;
    if (bus->lost_inhibit && bus->transfers == 7) bus->registers[1] &= (uint8_t)~8;
    *value = bus->registers[reg];
    return true;
}

static bool write_register(void *context, uint8_t reg, uint8_t value)
{
    fake_bus_t *bus = context;
    if (++bus->transfers == bus->fail_at) return false;
    /* An independent safety property for every attempted bus write. */
    if (reg == 1) {
        assert((value & 8) != 0);
        assert((value & (uint8_t)~8) == (bus->registers[1] & (uint8_t)~8));
    } else {
        assert(reg == 5);
        assert((value & 0x60) == 0);
        assert((value & (uint8_t)~0x60) == (bus->registers[5] & (uint8_t)~0x60));
    }
    if (reg != bus->ignore_write_reg) bus->registers[reg] = value;
    return true;
}

static fake_bus_t fake(uint8_t power)
{
    fake_bus_t bus = { .ignore_write_reg = 0xff };
    bus.registers[1] = power;
    bus.registers[5] = 0x7a;
    return bus;
}

static wristflow_aw32001_error_t inhibit(fake_bus_t *bus, wristflow_aw32001_result_t *result)
{
    const wristflow_aw32001_io_t io = {bus, read_register, write_register};
    return wristflow_aw32001_inhibit(&io, result);
}

int main(void)
{
    wristflow_aw32001_result_t result;
    /* Previously enabled, POR default, and unrelated power flags must all work. */
    const uint8_t powers[] = {0xa4, 0xac, 0xf7};
    for (unsigned i = 0; i < sizeof(powers); ++i) {
        fake_bus_t bus = fake(powers[i]);
        assert(inhibit(&bus, &result) == WF_AW32001_OK);
        assert(result.inhibit_verified && bus.transfers == 7);
        assert(bus.registers[1] == (uint8_t)(powers[i] | 8));
        assert(bus.registers[5] == 0x1a); /* keep safety timer and termination */
    }
    /* Every transport failure must stop, report a failure, and avoid readiness. */
    for (unsigned step = 1; step <= 7; ++step) {
        fake_bus_t bus = fake(0xa4);
        bus.fail_at = step;
        assert(inhibit(&bus, &result) != WF_AW32001_OK);
        assert(!result.inhibit_verified && bus.transfers == step);
        assert(result.error == ((step == 2 || step == 5) ?
            WF_AW32001_WRITE_FAILED : WF_AW32001_READ_FAILED));
    }
    /* ACK does not prove that a register changed. */
    for (unsigned reg = 1; reg <= 5; reg += 4) {
        fake_bus_t bus = fake(0xa4);
        bus.ignore_write_reg = (uint8_t)reg;
        assert(inhibit(&bus, &result) == WF_AW32001_VERIFY_FAILED);
        assert(!result.inhibit_verified && result.error_reg == reg);
    }
    fake_bus_t bus = fake(0xa4);
    bus.lost_inhibit = true;
    assert(inhibit(&bus, &result) == WF_AW32001_VERIFY_FAILED);
    assert(!result.inhibit_verified && result.error_reg == 1);
    assert(wristflow_aw32001_inhibit(NULL, &result) == WF_AW32001_BAD_ARGUMENT);
    assert(!result.inhibit_verified);
    assert(wristflow_aw32001_inhibit(NULL, NULL) == WF_AW32001_BAD_ARGUMENT);
    puts("AW32001 inhibition: transport faults, readback and no-enable properties passed");
    return 0;
}
