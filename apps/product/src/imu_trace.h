#ifndef WRISTFLOW_IMU_TRACE_H
#define WRISTFLOW_IMU_TRACE_H
#include <stdbool.h>
#include <stdint.h>

#define WF_IMU_TRACE_CAPACITY 128u
enum wf_imu_trace_reason {
    WF_TRACE_ARM = 1, WF_TRACE_IRQ, WF_TRACE_SCAN, WF_TRACE_SAMPLE,
    WF_TRACE_CANDIDATE, WF_TRACE_NO_FLAT, WF_TRACE_NOT_DEPARTED,
    WF_TRACE_NOT_X, WF_TRACE_NOT_Y, WF_TRACE_NOT_Z,
    WF_TRACE_NO_DATA, WF_TRACE_IO, WF_TRACE_DEADLINE,
    WF_TRACE_TRIGGER, WF_TRACE_STOP, WF_TRACE_EXPIRED
};
enum wf_imu_trace_flags {
    WF_TRACE_FLAT = 1, WF_TRACE_DEPARTED = 2,
    WF_TRACE_X = 4, WF_TRACE_Y = 8, WF_TRACE_Z = 16,
    WF_TRACE_AXES = 32, WF_TRACE_SOURCE_OK = 64
};
typedef struct {
    uint32_t tick, scan, irq;
    uint16_t read_ticks, hold_ticks;
    int16_t axes[3], baseline[3];
    uint8_t reason, flags, source;
} wf_imu_trace_entry_t;
/* One worker owns every access. No IRQ/UI copies or printing while scanning. */
typedef struct {
    wf_imu_trace_entry_t entries[WF_IMU_TRACE_CAPACITY];
    uint32_t started, span, total, overwritten;
    unsigned next, count;
    bool enabled;
} wf_imu_trace_t;
void wf_imu_trace_begin(wf_imu_trace_t *trace, uint32_t tick, uint32_t span);
void wf_imu_trace_push(wf_imu_trace_t *trace, wf_imu_trace_entry_t entry);
const wf_imu_trace_entry_t *wf_imu_trace_get(const wf_imu_trace_t *trace, unsigned index);
#endif
