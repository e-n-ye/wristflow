#ifndef WF_RUNTIME_RECORD_H
#define WF_RUNTIME_RECORD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { WF_RUNTIME_BYTES=512, WF_RUNTIME_EVENTS=8, WF_RUNTIME_SAMPLES=64 };
#define WF_RUNTIME_PERIOD_MS UINT64_C(1800000)
typedef enum { WF_RUN_UNKNOWN,WF_RUN_ON,WF_RUN_FLASHLIGHT,WF_RUN_DIM,WF_RUN_OFF,WF_RUN_AOD,WF_RUN_SCENES } wf_runtime_scene_t;
enum { WF_RUN_UTC_VALID=1, WF_RUN_RECORDING=2, WF_RUN_RESTORED=4 };
enum { WF_RUN_ADC_VALID=1,WF_RUN_ESTIMATE_VALID=2,WF_RUN_INPUT_VALID=4,WF_RUN_INPUT_GOOD=8,
       WF_RUN_BRIGHTNESS_VALID=16,WF_RUN_LCD_ON=32 };
typedef enum { WF_RUN_BOOT=1,WF_RUN_SCENE,WF_RUN_POWER,WF_RUN_CLOCK,WF_RUN_FAULT,
    WF_RUN_RECOVERY,WF_RUN_START,WF_RUN_STOP,WF_RUN_SAVE_FAILED } wf_runtime_event_kind_t;
typedef struct {
    uint64_t ms;
    uint32_t sequence,sampled_ms,utc;
    uint16_t millivolts,flags;
    uint8_t percent,requested,acknowledged,charge,scene;
} wf_runtime_sample_t;
typedef struct {
    uint64_t ms;
    uint32_t utc,value,detail,boot,clock_revision;
    uint16_t kind;
    uint8_t scene,flags;
} wf_runtime_event_t;
typedef struct {
    uint32_t generation,boot,flags,utc,clock_revision;
    /* Recorded active time, excluding explicitly stopped intervals; this is
     * not proof of the processor's complete uptime or physical sleep. */
    uint64_t uptime_ms,total_ms,scene_ms[WF_RUN_SCENES];
    wf_runtime_sample_t first,last;
    uint32_t fault_mask,fault_known,faults,recoveries,clock_errors,event_drops,sample_drops,samples;
    uint32_t attempts,successes,failures,periodic_saves,explicit_saves,events_total,io_retries,io_recoveries;
    uint32_t boot_mode,boot_raw,segment,event_count;
    uint64_t saved_uptime_ms;
    wf_runtime_event_t events[WF_RUNTIME_EVENTS];
} wf_runtime_summary_t;
typedef struct {
    uint64_t ms;
    uint32_t utc,clock_revision,fault_known,fault_mask;
    bool utc_valid;
    bool io_known;
    uint32_t io_retries,io_recoveries;
    wf_runtime_scene_t scene;
    wf_runtime_sample_t sample;
} wf_runtime_observation_t;
typedef struct {
    wf_runtime_summary_t summary;
    uint64_t last_ms;
    wf_runtime_scene_t scene;
    bool running,have_sample;
    bool have_io,io_baseline_only;
    uint32_t last_io_retries,last_io_recoveries;
    unsigned sample_head,sample_count;
    wf_runtime_sample_t samples[WF_RUNTIME_SAMPLES];
} wf_runtime_record_t;
/* Caller owns synchronization. Pure data only: no allocation, I/O or clocks. */
void wf_runtime_boot(wf_runtime_record_t *,const wf_runtime_summary_t *seed,uint64_t now,uint32_t mode,uint32_t raw);
bool wf_runtime_observe(wf_runtime_record_t *,const wf_runtime_observation_t *);
void wf_runtime_control(wf_runtime_record_t *,uint64_t now,bool start);
void wf_runtime_save_result(wf_runtime_record_t *,uint64_t saved_uptime,bool success,bool periodic);
bool wf_runtime_sample_read(const wf_runtime_record_t *,unsigned index,wf_runtime_sample_t *);
bool wf_runtime_encode(const wf_runtime_summary_t *,uint8_t out[WF_RUNTIME_BYTES]);
bool wf_runtime_sample_encode(const wf_runtime_sample_t *,uint8_t out[32]);
bool wf_runtime_decode(wf_runtime_summary_t *,const uint8_t *,size_t);
/* -1 means no trusted seed. Equal conflicting and half-range generations fail closed. */
int wf_runtime_select(wf_runtime_summary_t *,const uint8_t *,size_t,const uint8_t *,size_t);
#endif
