#include "aod_display.h"
#include "aod_policy.h"
#include <assert.h>
#include <string.h>

typedef struct {
    wf_panel_observation_t panel;
    unsigned operations, draws, hides;
    unsigned fail_at;
    bool fail_restore;
} device_t;
static bool operation(device_t *d) { return ++d->operations != d->fail_at; }
static bool observe(wf_panel_observation_t *out, void *ctx)
{ device_t *d=ctx; if (!operation(d)) return false; *out=d->panel; return true; }
static bool brightness(uint8_t value, void *ctx)
{
    device_t *d=ctx;
    if (!operation(d) || (d->fail_restore && !value)) return false;
    d->panel.brightness=value;
    /* Locked SDK LCD_MSG_SET_BRIGHTNESS calls lcd_display_on afterwards,
     * changing its state back to NORMAL even when the panel was in Idle. */
    d->panel.mode=WF_PANEL_NORMAL;
    return true;
}
static bool idle(bool value, void *ctx)
{ device_t *d=ctx; if (!operation(d) || (d->fail_restore && !value)) return false; d->panel.mode=value?WF_PANEL_IDLE:WF_PANEL_NORMAL; return true; }
static bool draw(void *ctx) { device_t *d=ctx; assert(d->panel.mode==WF_PANEL_NORMAL && !d->panel.brightness); ++d->draws; return operation(d); }
static void hide(void *ctx) { ++((device_t *)ctx)->hides; }
static const wf_aod_display_io_t io={observe,brightness,idle,draw,hide};

int main(void)
{
    wf_aod_display_t state={0}; device_t d={.panel={WF_PANEL_NORMAL,0}};
    assert(wf_aod_display_frame(&state,&io,&d,100,false));
    assert(state.phase==WF_AOD_PANEL_RESIDENT && state.owned && d.draws==1);
    unsigned calls=d.operations;
    for(unsigned i=0;i<100;++i) assert(wf_aod_display_frame(&state,&io,&d,100,false));
    assert(d.operations==calls && d.draws==1);
    assert(wf_aod_display_frame(&state,&io,&d,100,true) && d.draws==2);
    assert(state.frame_minute==100 && d.panel.brightness==WF_AOD_BRIGHTNESS);
    assert(wf_aod_display_frame(&state,&io,&d,101,false) && d.draws==3);
    assert(wf_aod_display_frame(&state,&io,&d,99,false) && d.draws==4); /* Backward clock correction. */
    assert(wf_aod_display_restore(&state,&io,&d));
    assert(!state.owned && d.hides==1 && d.panel.mode==WF_PANEL_NORMAL && !d.panel.brightness);
    for(unsigned fail=1;fail<=6;++fail) {
        state=(wf_aod_display_t){0}; d=(device_t){.panel={WF_PANEL_NORMAL,0},.fail_at=fail};
        assert(!wf_aod_display_frame(&state,&io,&d,100,false));
        assert(state.failed && !state.owned && d.hides==1);
        calls=d.operations;
        assert(!wf_aod_display_frame(&state,&io,&d,101,false) && d.operations==calls);
    }
    state=(wf_aod_display_t){0};d=(device_t){.panel={WF_PANEL_NORMAL,0}};
    assert(wf_aod_display_frame(&state,&io,&d,200,false));d.fail_restore=true;
    assert(!wf_aod_display_restore(&state,&io,&d));
    assert(state.failed && state.owned && state.phase==WF_AOD_PANEL_FAULT && !d.hides);
    d.fail_restore=false;assert(wf_aod_display_restore(&state,&io,&d));
    assert(!state.owned && state.failed && d.hides==1);

    wristflow_settings_t settings=wristflow_settings_default();
    wristflow_watch_snapshot_t clock=wristflow_product_snapshot(true,WRISTFLOW_TIME_MIN);
    assert(!wristflow_settings_aod_active(&settings,&clock));
    assert(wristflow_aod_wait_ms(&settings,&clock,0)==UINT32_MAX);
    settings.aod_mode=WRISTFLOW_AOD_SCHEDULED;
    for(unsigned overnight=0;overnight<2;++overnight) {
        settings.aod_start_minute=overnight?1320:480; settings.aod_end_minute=overnight?480:1320;
        for(unsigned minute=0;minute<1440;++minute) {
            clock.hour_24=minute/60;clock.minute=minute%60;
            clock.utc_seconds=WRISTFLOW_TIME_MIN+minute*60+17;
            bool active=overnight?minute>=1320 || minute<480:minute>=480 && minute<1320;
            assert(wristflow_settings_aod_active(&settings,&clock)==active);
            unsigned delta=(settings.aod_start_minute+1440-minute)%1440;
            assert(wristflow_aod_wait_ms(&settings,&clock,321)==
                (active?42679U:delta*60000U-17321U));
        }
    }
    clock.time_unavailable=true;
    assert(!wristflow_settings_aod_active(&settings,&clock));
    assert(wristflow_aod_wait_ms(&settings,&clock,0)==UINT32_MAX);
    clock.time_unavailable=false;settings.endurance_mode=true;
    assert(!wristflow_settings_aod_active(&settings,&clock));
    assert(wristflow_aod_wait_ms(&settings,&clock,0)==UINT32_MAX);
    settings.endurance_mode=false;settings.aod_style=WRISTFLOW_AOD_CORNER;
    uint8_t bytes[WRISTFLOW_SETTINGS_BYTES];wristflow_settings_t restored=wristflow_settings_default();
    assert(wristflow_settings_encode(&settings,bytes));
    assert(wristflow_settings_decode(&restored,bytes,sizeof bytes));
    assert(wristflow_settings_equal(&settings,&restored));
    settings.aod_start_minute=settings.aod_end_minute;
    assert(!wristflow_settings_encode(&settings,bytes));
    return 0;
}
