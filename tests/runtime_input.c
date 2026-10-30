#include "../apps/product/src/runtime_input.c"
#include <assert.h>
static uint64_t now;
static wristflow_alarm_clock_t clock;
static wf_runtime_observation_t captured;
static wristflow_product_light_snapshot_t light;
static wf_compass_snapshot_t compass;
static bool wrist,storage;
uint64_t wristflow_product_services_monotonic(void) { return now; }
wristflow_alarm_clock_t wristflow_product_services_clock_cache(void) { return clock; }
bool wristflow_product_services_storage_failed(void) { return storage; }
wristflow_product_light_snapshot_t wristflow_product_light_snapshot(void) { return light; }
wf_compass_snapshot_t wristflow_product_compass_snapshot(void) { return compass; }
bool wristflow_product_wrist_failed(void) { return wrist; }
wf_product_runtime_health_t wf_product_runtime_health(void)
{ return (wf_product_runtime_health_t){.wrist_known=wrist,.wrist_fault=wrist,.retries=3,.recoveries=2}; }
void wf_product_runtime_observe(const wf_runtime_observation_t *o) { captured=*o; }
int main(void)
{
    wristflow_battery_record_t v={0};wf_product_runtime_capture(&v,false,false,false);assert(captured.scene==WF_RUN_UNKNOWN);
    v.context.flags=WF_BATTERY_UI_VALID;wf_product_runtime_capture(&v,false,false,false);assert(captured.scene==WF_RUN_OFF);
    v.context.flags|=WF_BATTERY_LCD_ON|WF_BATTERY_BRIGHTNESS_VALID;v.context.phase=WRISTFLOW_DISPLAY_ACTIVE;
    wf_product_runtime_capture(&v,false,false,false);assert(captured.scene==WF_RUN_ON);
    wf_product_runtime_capture(&v,false,true,false);assert(captured.scene==WF_RUN_FLASHLIGHT);
    v.context.phase=WRISTFLOW_DISPLAY_DIM;wf_product_runtime_capture(&v,false,true,false);assert(captured.scene==WF_RUN_DIM);
    v.context.flags|=WF_BATTERY_BRIGHTNESS_DEFERRED;wf_product_runtime_capture(&v,false,false,false);assert(captured.scene==WF_RUN_UNKNOWN);
    wf_product_runtime_capture(&v,true,false,false);assert(captured.scene==WF_RUN_AOD && captured.sample.acknowledged==WF_AOD_BRIGHTNESS);
    wf_product_runtime_capture(&v,true,false,true);assert(captured.scene==WF_RUN_UNKNOWN);
    now=5000;clock=(wristflow_alarm_clock_t){.utc=1793348000,.valid=true,.monotonic_ms=0,.revision=2};
    light.active=light.valid=true;compass.valid=compass.selftest_passed=true;
    wf_product_runtime_capture(&v,false,false,false);assert(captured.utc_valid && captured.utc==clock.utc && captured.clock_revision==2 && captured.fault_known==11 && !captured.fault_mask);
    now=15001;light=(wristflow_product_light_snapshot_t){0};compass=(wf_compass_snapshot_t){0};
    wf_product_runtime_capture(&v,false,false,false);assert(!captured.utc_valid && !captured.utc && captured.fault_known==8);
    light.fault=compass.cleanup_pending=wrist=storage=true;
    wf_product_runtime_capture(&v,false,false,false);assert(captured.fault_known==15 && captured.fault_mask==15);
    return 0;
}
