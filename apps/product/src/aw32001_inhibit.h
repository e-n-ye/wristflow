#ifndef WRISTFLOW_AW32001_INHIBIT_H
#define WRISTFLOW_AW32001_INHIBIT_H

#include <stdbool.h>
#include <stdint.h>

/* AW32001E V1.4: REG01[3]=1 inhibits charging. No enable operation exists here. */
typedef struct {
    void *context;
    bool (*read)(void *context, uint8_t reg, uint8_t *value);
    bool (*write)(void *context, uint8_t reg, uint8_t value);
} wristflow_aw32001_io_t;

typedef enum {
    WF_AW32001_OK,
    WF_AW32001_BAD_ARGUMENT,
    WF_AW32001_READ_FAILED,
    WF_AW32001_WRITE_FAILED,
    WF_AW32001_VERIFY_FAILED,
    WF_AW32001_ID_MISMATCH
} wristflow_aw32001_error_t;

typedef struct {
    wristflow_aw32001_error_t error;
    uint8_t error_reg;
    uint8_t power_on;
    uint8_t timer;
    uint8_t chip_id;
    uint8_t charge_current;
    uint8_t charge_voltage;
    uint8_t individual;
    bool inhibit_verified;
    bool profile_verified;
} wristflow_aw32001_result_t;

/* At most seven transfers, no retry or polling. An error leaves verification false;
 * software cannot guarantee physical inhibition when the bus does not respond. */
wristflow_aw32001_error_t wristflow_aw32001_inhibit(
    const wristflow_aw32001_io_t *io, wristflow_aw32001_result_t *result);

/* Fixed bench candidate, not a permission to charge or a measured limit. */
enum { WF_AW32001_PROFILE_MV = 4170, WF_AW32001_PROFILE_MA = 80 };

/* First inhibit, then check ID and configure the fixed profile while inhibited.
 * At most 21 transfers, no retry, reset command or charge-enable operation.
 * Any failure leaves both verification flags false; physical state is unknown. */
wristflow_aw32001_error_t wristflow_aw32001_prepare(
    const wristflow_aw32001_io_t *io, wristflow_aw32001_result_t *result);

#endif
