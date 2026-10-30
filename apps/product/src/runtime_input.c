#include "runtime_input.h"
#include "product_runtime_record.h"
#include "product_services.h"
#include "product_light.h"
#include "product_compass.h"
#include "product_runtime_health.h"
#include "display_policy.h"
#include "aod_display.h"
void wf_product_runtime_capture(const wristflow_battery_record_t *v,bool aod,bool flashlight,bool diagnostic)
{
    if(!v)return;
    uint64_t now=wristflow_product_services_monotonic();
    wristflow_alarm_clock_t clock=wristflow_product_services_clock_cache();
    bool fresh=clock.valid && now>=clock.monotonic_ms && now-clock.monotonic_ms<=15000;
    wf_runtime_observation_t o={.ms=now,.utc=fresh?clock.utc:0,.utc_valid=fresh,.clock_revision=clock.revision,.scene=WF_RUN_UNKNOWN};
    if(diagnostic)o.scene=WF_RUN_UNKNOWN;
    else if(aod)o.scene=WF_RUN_AOD;
    else if(v->context.flags&WF_BATTERY_UI_VALID) {
        if(!(v->context.flags&WF_BATTERY_LCD_ON))o.scene=WF_RUN_OFF;
        else if(!(v->context.flags&WF_BATTERY_BRIGHTNESS_DEFERRED) && (v->context.flags&WF_BATTERY_BRIGHTNESS_VALID)) {
            if(v->context.phase==WRISTFLOW_DISPLAY_DIM)o.scene=WF_RUN_DIM;
            else if(v->context.phase==WRISTFLOW_DISPLAY_ACTIVE)o.scene=flashlight?WF_RUN_FLASHLIGHT:WF_RUN_ON;
        }
    }
    wf_runtime_sample_t *s=&o.sample;
    s->sequence=v->power.voltage_sequence;s->sampled_ms=v->power.voltage_sampled_ms;s->millivolts=v->power.millivolts;
    s->percent=v->power.estimated_percent;s->requested=v->context.requested_brightness;
    s->acknowledged=v->context.acknowledged_brightness;s->charge=(uint8_t)v->power.charge;
    if(v->adc_valid)s->flags|=WF_RUN_ADC_VALID;
    if(v->power.estimate_valid)s->flags|=WF_RUN_ESTIMATE_VALID;
    if(v->power.input_valid)s->flags|=WF_RUN_INPUT_VALID;
    if(v->power.input_good)s->flags|=WF_RUN_INPUT_GOOD;
    if(v->context.flags&WF_BATTERY_BRIGHTNESS_VALID)s->flags|=WF_RUN_BRIGHTNESS_VALID;
    if(v->context.flags&WF_BATTERY_LCD_ON)s->flags|=WF_RUN_LCD_ON;
    if(aod && !diagnostic) {
        s->requested=s->acknowledged=WF_AOD_BRIGHTNESS;
        s->flags|=WF_RUN_BRIGHTNESS_VALID;
    }
    wristflow_product_light_snapshot_t light=wristflow_product_light_snapshot();
    wf_compass_snapshot_t compass=wristflow_product_compass_snapshot();
    bool light_fault=light.fault||light.cleanup_pending,compass_fault=compass.fault||compass.cleanup_pending;
    if(light_fault || (light.active && light.valid))o.fault_known|=1;
    if(light_fault)o.fault_mask|=1;
    if(compass_fault || (compass.valid && compass.selftest_passed))o.fault_known|=2;
    if(compass_fault)o.fault_mask|=2;
    wf_product_runtime_health_t health=wf_product_runtime_health();
    if(health.wrist_known)o.fault_known|=4;
    if(health.wrist_fault)o.fault_mask|=4;
    if(health.imu_known)o.fault_known|=16;
    if(health.imu_fault)o.fault_mask|=16;
    o.io_known=true;o.io_retries=health.retries;o.io_recoveries=health.recoveries;
    o.fault_known|=8;if(wristflow_product_services_storage_failed())o.fault_mask|=8;
    wf_product_runtime_observe(&o);
}
