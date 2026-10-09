#include "imu_trace.h"
#include <string.h>

void wf_imu_trace_begin(wf_imu_trace_t *trace, uint32_t tick, uint32_t span)
{
    memset(trace, 0, sizeof *trace);
    trace->started = tick;
    trace->span = span;
    trace->enabled = true;
}

void wf_imu_trace_push(wf_imu_trace_t *trace, wf_imu_trace_entry_t entry)
{
    if (!trace->enabled) return;
    if (entry.tick - trace->started >= trace->span) entry.reason = WF_TRACE_EXPIRED;
    trace->entries[trace->next] = entry;
    trace->next = (trace->next + 1u) % WF_IMU_TRACE_CAPACITY;
    if (trace->count < WF_IMU_TRACE_CAPACITY) ++trace->count;
    else ++trace->overwritten;
    ++trace->total;
    if (entry.reason == WF_TRACE_DEADLINE || entry.reason == WF_TRACE_TRIGGER ||
        entry.reason == WF_TRACE_STOP || entry.reason == WF_TRACE_EXPIRED)
        trace->enabled = false;
}

const wf_imu_trace_entry_t *wf_imu_trace_get(const wf_imu_trace_t *trace, unsigned index)
{
    if (index >= trace->count) return NULL;
    unsigned first = (trace->next + WF_IMU_TRACE_CAPACITY - trace->count) % WF_IMU_TRACE_CAPACITY;
    return &trace->entries[(first + index) % WF_IMU_TRACE_CAPACITY];
}
