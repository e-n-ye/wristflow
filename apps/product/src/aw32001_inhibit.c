#include "aw32001_inhibit.h"
#include <stddef.h>

enum { POWER_ON = 0x01, TIMER = 0x05, CEB = 0x08, WATCHDOG = 0x60 };

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
