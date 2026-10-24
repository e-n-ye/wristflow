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
    WF_MAG_ID_MISMATCH, WF_MAG_NOT_READY, WF_MAG_SELFTEST_FAILED, WF_MAG_UNEXPECTED_DATA
} wf_mag_end_t;
typedef struct {
    wf_mag_phase_t phase;
    wf_mag_end_t ended;
    uint32_t session, started_ms, duration_ms, sampled_ms, samples, io_errors;
    int32_t x, y, z; /* Centered 20-bit counts; 1 count = 6.25 nT, uncalibrated. */
    uint8_t identity, status, after_status, selftest_status;
    uint8_t selftest_factory[3], selftest_threshold[3], raw[9];
    bool identity_valid, selftest_valid, selftest_passed;
    bool owned, stopped, sample_valid, saturated;
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
void wf_mmc5603_poll(wf_mmc5603_t *);
bool wf_mmc5603_stop(wf_mmc5603_t *, wf_mag_end_t);
void wf_mmc5603_bus_fault(wf_mmc5603_t *);
int32_t wf_mmc5603_wait_ms(const wf_mmc5603_t *, uint32_t now_ms);
#endif
