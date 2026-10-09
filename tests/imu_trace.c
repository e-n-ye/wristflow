#include "imu_trace.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    static wf_imu_trace_t trace;
    _Static_assert(sizeof(wf_imu_trace_entry_t) == 32, "Trace entry budget changed");
    _Static_assert(sizeof trace <= 4136, "Trace memory budget changed");
    wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){.tick=1, .reason=WF_TRACE_SAMPLE});
    assert(trace.count == 0); /* Disabled traces collect nothing. */
    wf_imu_trace_begin(&trace, 100, 10000);
    for (unsigned i = 0; i < WF_IMU_TRACE_CAPACITY + 3; ++i)
        wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){
            .tick=100+i, .scan=7, .reason=WF_TRACE_NOT_Z,
            .axes={-16000, 0, (int16_t)i}, .baseline={0,0,-16384},
            .hold_ticks=17, .flags=WF_TRACE_AXES | WF_TRACE_FLAT});
    assert(trace.count == 128 && trace.overwritten == 3 && trace.total == 131);
    assert(wf_imu_trace_get(&trace, 0)->tick == 103);
    assert(wf_imu_trace_get(&trace, 127)->axes[2] == 130);
    assert(wf_imu_trace_get(&trace, 0)->baseline[2] == -16384);
    assert(wf_imu_trace_get(&trace, 0)->hold_ticks == 17);
    assert(wf_imu_trace_get(&trace, 128) == NULL);
    wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){.tick=240, .reason=WF_TRACE_TRIGGER});
    assert(!trace.enabled && trace.total == 132);
    wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){.tick=241, .reason=WF_TRACE_ARM});
    assert(trace.total == 132 && wf_imu_trace_get(&trace, 127)->reason == WF_TRACE_TRIGGER);
    /* A new trial replaces the old frozen buffer; unsigned time survives wrap. */
    wf_imu_trace_begin(&trace, UINT32_MAX-9, 20);
    wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){.tick=9, .reason=WF_TRACE_SAMPLE});
    assert(trace.enabled && trace.count == 1);
    wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){.tick=10, .reason=WF_TRACE_SAMPLE});
    assert(!trace.enabled && wf_imu_trace_get(&trace, 1)->reason == WF_TRACE_EXPIRED);
    wf_imu_trace_begin(&trace, 0, 100);
    wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){.tick=5, .reason=WF_TRACE_DEADLINE});
    assert(!trace.enabled);
    wf_imu_trace_begin(&trace, 0, 100);
    wf_imu_trace_push(&trace, (wf_imu_trace_entry_t){.tick=5, .reason=WF_TRACE_STOP});
    assert(!trace.enabled && trace.overwritten == 0);
    puts("IMU diagnostic retention, freeze, overflow and wrap passed");
    return 0;
}
