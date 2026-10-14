#include "aw32001_inhibit.h"
#include <stddef.h>

enum {
    POWER_ON = 0x01, CURRENT = 0x02, VOLTAGE = 0x04, TIMER = 0x05,
    CHIP_ID = 0x0a, INDIVIDUAL = 0x0b,
    CEB = 0x08, WATCHDOG = 0x60, CURRENT_DIVIDE = 0x80,
    EXPECTED_ID = 0x49,
    CURRENT_CODE = WF_AW32001_PROFILE_MA / 8 - 1,
    VOLTAGE_CODE = (WF_AW32001_PROFILE_MV - 3600) / 15
};
_Static_assert(WF_AW32001_PROFILE_MA % 8 == 0 && CURRENT_CODE <= 63,
               "profile current must have an exact normal-mode encoding");
_Static_assert((WF_AW32001_PROFILE_MV - 3600) % 15 == 0 && VOLTAGE_CODE <= 63,
               "profile voltage must have an exact encoding");

static bool read_reg(const wristflow_aw32001_io_t *io,
                     wristflow_aw32001_result_t *result, uint8_t reg, uint8_t *value)
{
    if (io->read(io->context, reg, value)) return true;
    result->error = WF_AW32001_READ_FAILED;
    result->error_reg = reg;
    return false;
}

static bool write_reg(const wristflow_aw32001_io_t *io,
                      wristflow_aw32001_result_t *result, uint8_t reg, uint8_t value)
{
    if (io->write(io->context, reg, value)) return true;
    result->error = WF_AW32001_WRITE_FAILED;
    result->error_reg = reg;
    return false;
}

wristflow_aw32001_error_t wristflow_aw32001_inhibit(
    const wristflow_aw32001_io_t *io, wristflow_aw32001_result_t *result)
{
    if (result == NULL) return WF_AW32001_BAD_ARGUMENT;
    *result = (wristflow_aw32001_result_t){ .error = WF_AW32001_BAD_ARGUMENT };
    if (io == NULL || io->read == NULL || io->write == NULL) return result->error;
    result->error = WF_AW32001_OK;

    if (!read_reg(io, result, POWER_ON, &result->power_on) ||
        !write_reg(io, result, POWER_ON, result->power_on | CEB) ||
        !read_reg(io, result, POWER_ON, &result->power_on)) return result->error;
    if ((result->power_on & CEB) == 0) {
        result->error_reg = POWER_ON;
        return result->error = WF_AW32001_VERIFY_FAILED;
    }

    /* Preserve termination/safety timer settings. Disable the watchdog while
     * inhibited so its default timeout cannot reset the system supply rails. */
    if (!read_reg(io, result, TIMER, &result->timer) ||
        !write_reg(io, result, TIMER, result->timer & (uint8_t)~WATCHDOG) ||
        !read_reg(io, result, TIMER, &result->timer)) return result->error;
    if ((result->timer & WATCHDOG) != 0) {
        result->error_reg = TIMER;
        return result->error = WF_AW32001_VERIFY_FAILED;
    }
    if (!read_reg(io, result, POWER_ON, &result->power_on)) return result->error;
    if ((result->power_on & CEB) == 0) {
        result->error_reg = POWER_ON;
        return result->error = WF_AW32001_VERIFY_FAILED;
    }
    result->inhibit_verified = true;
    return result->error;
}

wristflow_aw32001_error_t wristflow_aw32001_prepare(
    const wristflow_aw32001_io_t *io, wristflow_aw32001_result_t *result)
{
    wristflow_aw32001_error_t error = wristflow_aw32001_inhibit(io, result);
    if (error != WF_AW32001_OK) return error;
    /* Only the final full readback establishes this transaction's result. */
    result->inhibit_verified = false;
    if (!read_reg(io, result, CHIP_ID, &result->chip_id)) return result->error;
    if (result->chip_id != EXPECTED_ID) {
        result->error_reg = CHIP_ID;
        return result->error = WF_AW32001_ID_MISMATCH;
    }

    /* REG02[7:6] are reset commands: never echo them from a read-modify-write.
     * Set the smaller current before changing the optional divide mode. */
    if (!write_reg(io, result, CURRENT, CURRENT_CODE) ||
        !read_reg(io, result, CURRENT, &result->charge_current)) return result->error;
    if (result->charge_current != CURRENT_CODE) {
        result->error_reg = CURRENT;
        return result->error = WF_AW32001_VERIFY_FAILED;
    }
    if (!read_reg(io, result, INDIVIDUAL, &result->individual)) return result->error;
    uint8_t individual = result->individual & (uint8_t)~CURRENT_DIVIDE;
    if (!write_reg(io, result, INDIVIDUAL, individual) ||
        !read_reg(io, result, INDIVIDUAL, &result->individual)) return result->error;
    if (result->individual != individual) {
        result->error_reg = INDIVIDUAL;
        return result->error = WF_AW32001_VERIFY_FAILED;
    }
    if (!read_reg(io, result, VOLTAGE, &result->charge_voltage)) return result->error;
    uint8_t voltage = (result->charge_voltage & 0x03) | (VOLTAGE_CODE << 2);
    if (!write_reg(io, result, VOLTAGE, voltage) ||
        !read_reg(io, result, VOLTAGE, &result->charge_voltage)) return result->error;
    if (result->charge_voltage != voltage) {
        result->error_reg = VOLTAGE;
        return result->error = WF_AW32001_VERIFY_FAILED;
    }

    /* Recheck inhibition/watchdog and all profile fields after the last write.
     * This is a past readback, not an atomic or continuous hardware guarantee. */
    const uint8_t registers[] = { POWER_ON, TIMER, CURRENT, INDIVIDUAL, VOLTAGE };
    uint8_t *values[] = { &result->power_on, &result->timer,
        &result->charge_current, &result->individual, &result->charge_voltage };
    const uint8_t masks[] = { CEB, WATCHDOG, 0xff, 0xff, 0xff };
    const uint8_t expected[] = { CEB, 0, CURRENT_CODE, individual, voltage };
    for (unsigned i = 0; i < sizeof(registers); ++i) {
        if (!read_reg(io, result, registers[i], values[i])) return result->error;
        if ((*values[i] & masks[i]) != expected[i]) {
            result->error_reg = registers[i];
            return result->error = WF_AW32001_VERIFY_FAILED;
        }
    }
    result->inhibit_verified = true;
    result->profile_verified = true;
    return result->error;
}
