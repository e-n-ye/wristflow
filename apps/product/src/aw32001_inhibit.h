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
    WF_AW32001_VERIFY_FAILED
} wristflow_aw32001_error_t;

typedef struct {
    wristflow_aw32001_error_t error;
    uint8_t error_reg;
    uint8_t power_on;
    uint8_t timer;
    bool inhibit_verified;
} wristflow_aw32001_result_t;

/* At most seven transfers, no retry or polling. An error leaves verification false;
 * software cannot guarantee physical inhibition when the bus does not respond. */
wristflow_aw32001_error_t wristflow_aw32001_inhibit(
    const wristflow_aw32001_io_t *io, wristflow_aw32001_result_t *result);

#endif
