#ifndef WF_CYCLE_RECORD_H
#define WF_CYCLE_RECORD_H
#include "battery_trace.h"
#define WF_CYCLE_ROW 64u
#define WF_CYCLE_MAX_MS UINT64_C(172800000)
#define WF_CYCLE_MAX_ROWS 40000u
enum { WF_CYCLE_START=1,WF_CYCLE_SAMPLE,WF_CYCLE_END,WF_CYCLE_EVENT };
enum { WF_CYCLE_CONTEXT=1,WF_CYCLE_KEY_DOWN,WF_CYCLE_KEY_UP,
       WF_CYCLE_LCD_ON_REQUEST,WF_CYCLE_LCD_ON_DONE,WF_CYCLE_LCD_OFF_DONE };
enum { WF_CYCLE_MANUAL=1,WF_CYCLE_LOW,WF_CYCLE_STALE,WF_CYCLE_LOAD,
       WF_CYCLE_USB,WF_CYCLE_LIMIT,WF_CYCLE_STORAGE,WF_CYCLE_QUEUE };
typedef struct {
    uint32_t session,sequence,utc,revision;
    uint64_t elapsed;
    uint8_t kind,reason;
    bool flashlight,utc_valid;
    /* Version2 is the explicit low-voltage experiment. Its former filtered
     * voltage slot holds ceil(keep_remaining_ms/1000), not a voltage. */
    bool low_voltage;
    uint8_t event;
    uint32_t keep_remaining_ms;
    uint32_t event_tick_ms; /* v2 EVENT: captured callback tick, may wrap uint32. */
    wristflow_battery_record_t observation;
} wf_cycle_row_t;
uint32_t wf_cycle_crc(const void *,size_t);
bool wf_cycle_encode(const wf_cycle_row_t *,uint8_t out[WF_CYCLE_ROW]);
bool wf_cycle_decode(const uint8_t in[WF_CYCLE_ROW],wf_cycle_row_t *);
/* FULL load cycle is sampled from existing ADC only, no acquisition side effects. */
unsigned wf_cycle_stop_reason(const wristflow_battery_record_t *,bool flashlight,
    uint64_t elapsed,bool smoke);
#endif
