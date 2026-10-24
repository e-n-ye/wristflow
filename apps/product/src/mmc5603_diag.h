#ifndef WRISTFLOW_MMC5603_DIAG_H
#define WRISTFLOW_MMC5603_DIAG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *context;
    bool (*read)(void *, uint8_t reg, uint8_t *data, size_t size);
    bool (*write)(void *, uint8_t reg, uint8_t value);
    uint32_t (*now_ms)(void *);
    bool (*cancelled)(void *);
    void (*delay_ms)(void *, uint32_t);
} wf_mmc5603_io_t;
typedef enum { WF_MAG_IDLE, WF_MAG_RUNNING, WF_MAG_DONE, WF_MAG_FAULT } wf_mag_phase_t;
typedef enum {
    WF_MAG_NO_END, WF_MAG_DEADLINE, WF_MAG_CANCELLED, WF_MAG_IO_FAILURE,
    WF_MAG_ID_MISMATCH, WF_MAG_NOT_READY, WF_MAG_SELFTEST_FAILED, WF_MAG_UNEXPECTED_DATA,
    WF_MAG_SELFTEST_COMPLETE = 8
} wf_mag_end_t;
typedef enum {
    WF_MAG_THRESHOLD_RAW80,
    WF_MAG_THRESHOLD_ENCODED_EXPERIMENTAL
} wf_mag_threshold_t;
enum { WF_MAG_SELFTEST_TIMEOUT_MS = 10000 };
/* Transport evidence bits, not a magnetic-validity or selftest-PASS mask.
 * COMMAND records an attempt; ACK separately records its successful return. */
enum {
    WF_MAG_TRACE_RESET = 0x01, WF_MAG_TRACE_FACTORY = 0x02,
    WF_MAG_TRACE_COMMAND = 0x04, WF_MAG_TRACE_ACK = 0x08,
    WF_MAG_TRACE_STATUS = 0x10, WF_MAG_TRACE_DATA = 0x20, WF_MAG_TRACE_POST = 0x40
};
typedef struct {
    wf_mag_phase_t phase;
    wf_mag_end_t ended;
    wf_mag_threshold_t threshold_mode;
    uint32_t session, started_ms, duration_ms, sampled_ms, samples, io_errors;
    int32_t x, y, z; /* Centered 20-bit counts; 1 count = 6.25 nT, uncalibrated. */
    uint8_t identity, status, after_status, selftest_status;
    uint8_t selftest_factory[3], selftest_threshold[3], raw[9];
    uint8_t selftest_trace, reset_status, selftest_post_status, selftest_raw[9];
    uint32_t reset_observed_ms, selftest_command_ms, selftest_ack_ms;
    uint32_t selftest_status_ms, selftest_data_ms, selftest_post_ms, selftest_polls;
    bool identity_valid, selftest_valid, selftest_passed;
    bool owned, stopped, sample_valid, saturated, selftest_only;
} wf_mmc5603_snapshot_t;
typedef struct {
    wf_mmc5603_io_t io;
    wf_mmc5603_snapshot_t snapshot;
    uint32_t due_ms, frame_ms;
    unsigned stage;
} wf_mmc5603_t;

/* Existing sensor worker only, explicit 1..120s. No continuous mode, IRQ,
 * heading, calibration or shared-rail reset. Write-only controls cannot be
 * saved/read back: local reset establishes a known default stopped baseline.
 * stopped means reset ACK + OTP ready + drained/no new DONE over 20ms; it is
 * not arbitrary prior-state restoration or a current measurement. */
void wf_mmc5603_init(wf_mmc5603_t *, wf_mmc5603_io_t);
bool wf_mmc5603_begin(wf_mmc5603_t *, uint32_t started_ms, uint32_t duration_ms);
/* Explicit encoded-threshold candidate; native selftest must PASS before
 * bounded Auto_SR single-shot XYZ. Does not change the default begin(). */
bool wf_mmc5603_begin_encoded(wf_mmc5603_t *, uint32_t started_ms, uint32_t duration_ms);
/* Explicit diagnostic hypothesis only; fixed total deadline, no ordinary XYZ.
 * Both PASS and FAIL stop locally. The default begin() always uses raw80. */
bool wf_mmc5603_begin_selftest(wf_mmc5603_t *, uint32_t started_ms, wf_mag_threshold_t);
void wf_mmc5603_poll(wf_mmc5603_t *);
bool wf_mmc5603_stop(wf_mmc5603_t *, wf_mag_end_t);
void wf_mmc5603_bus_fault(wf_mmc5603_t *);
int32_t wf_mmc5603_wait_ms(const wf_mmc5603_t *, uint32_t now_ms);
#endif
