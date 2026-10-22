#include "product_ui.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks, requests;
static uint8_t pixels[390*40*4];
static unsigned brightness;
static wristflow_ui_shell_t *shell;
static wristflow_settings_t saved;
static wristflow_save_state_t save_state;
static wristflow_input_observation_t cached_input;
static wristflow_input_observation_t read_input(void *context)
{ assert(context==&cached_input);return cached_input; }
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{ (void)a;(void)p;lv_display_flush_ready(d); }
static void light(uint8_t value,void *ctx) { (void)ctx;brightness=value; }
static uint32_t save(const wristflow_settings_t *s,void *ctx)
{ (void)ctx;saved=*s;save_state=WRISTFLOW_SAVE_PENDING;return ++requests; }
static wristflow_save_state_t status(uint32_t id,void *ctx)
{ (void)ctx;assert(id==requests);return save_state; }
static void advance(unsigned ms)
{ for(unsigned i=0;i<ms;i+=16) { ticks+=16;lv_timer_handler(); } lv_obj_update_layout(lv_screen_active()); }
static lv_obj_t *named(const char *name)
{ lv_obj_t *o=lv_obj_find_by_name(lv_screen_active(),name);assert(o);return o; }
static const char *label(const char *name) { return lv_label_get_text(named(name)); }
static void surface(wristflow_surface_t s)
{ assert(wristflow_ui_shell_navigation(shell)->surface==s); }
static void create(wristflow_watch_snapshot_t *s,wristflow_settings_t *p)
{
    shell=wristflow_product_ui_create(s,p,light,NULL);assert(shell);advance(240);
    wristflow_ui_shell_enable_display_policy(shell);
    wristflow_ui_shell_bind_settings_store(shell,save,status,NULL);
}
static void mode(bool enabled)
{ assert(wristflow_ui_shell_set_endurance(shell,enabled));advance(240); }
int main(void)
{
    lv_init();lv_tick_set_cb(tick);
    lv_display_t *d=lv_display_create(390,450);
    lv_display_set_buffers(d,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);wristflow_ui_init("");
    wristflow_settings_t prefs=wristflow_settings_default();prefs.brightness=27;prefs.wrist_wake=true;
    wristflow_watch_snapshot_t s=wristflow_product_snapshot(true,1792656000U);
    s.power.input_valid=true;s.power.input_good=true;
    s.power.input_observation=(wristflow_input_observation_t){.valid=true,.good=true};
    create(&s,&prefs);
    assert(wristflow_ui_shell_keep_awake(shell,5));
    mode(true);surface(WRISTFLOW_SURFACE_ENDURANCE);
    assert(!wristflow_ui_shell_keep_minutes(shell) && brightness==27);
    assert(!wristflow_ui_shell_keep_awake(shell,5));
    assert(!wristflow_ui_shell_home(shell) && !wristflow_ui_shell_back(shell));
    assert(!wristflow_ui_shell_open_controls(shell));
    assert(!wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_SETTINGS));
    assert(!wristflow_ui_shell_open_notification(shell,false,0));
    assert(!wristflow_ui_shell_wrist_wake(shell));
    wristflow_ui_shell_sync_settings_save(shell);assert(saved.endurance_mode && saved.wrist_wake);
    assert(!strcmp(label("endurance_save"),"保存中"));
    save_state=WRISTFLOW_SAVE_FAILED;wristflow_ui_shell_sync_settings_save(shell);
    assert(!strcmp(label("endurance_save"),"保存失败，退出后重试"));
    save_state=WRISTFLOW_SAVE_DONE;wristflow_ui_shell_retry_settings_save(shell);
    wristflow_ui_shell_sync_settings_save(shell);save_state=WRISTFLOW_SAVE_DONE;
    wristflow_ui_shell_sync_settings_save(shell);assert(!*label("endurance_save"));
    wristflow_ui_shell_endurance_services(shell,false,true);
    assert(!strcmp(label("endurance_status"),"后台暂停失败，请退出"));
    wristflow_ui_shell_endurance_services(shell,true,false);assert(!*label("endurance_status"));
    assert(!wristflow_ui_shell_charging_event(shell,&s));assert(wristflow_ui_shell_endurance(shell));
    advance(11000);assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    lv_timer_enable(false);ticks+=180000; /* Same mode after long sleep, no LVGL timers. */
    wristflow_ui_shell_endurance_key(shell,true,1,1500);lv_timer_enable(true);advance(16);
    surface(WRISTFLOW_SURFACE_ENDURANCE);assert(brightness==27);
    assert(lv_arc_get_value(named("endurance_ring"))==50);
    wristflow_ui_shell_endurance_key(shell,false,1,1500);assert(wristflow_ui_shell_endurance(shell));
    assert(lv_obj_has_flag(named("endurance_exit"),LV_OBJ_FLAG_HIDDEN));
    wristflow_ui_shell_endurance_key(shell,true,2,1000);
    lv_obj_t *reminder=lv_obj_create(lv_layer_top());
    assert(wristflow_ui_shell_present_reminder(shell,reminder));
    wristflow_ui_shell_endurance_key(shell,true,2,3500);assert(wristflow_ui_shell_endurance(shell));
    advance(31000);assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    wristflow_ui_shell_endurance_key(shell,true,3,0);
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_ACTIVE);
    wristflow_ui_shell_endurance_key(shell,true,3,3500);assert(wristflow_ui_shell_endurance(shell));
    wristflow_ui_shell_close_reminder(shell);advance(240);surface(WRISTFLOW_SURFACE_ENDURANCE);
    /* Release/repress coalesced: a new physical press must still work. */
    wristflow_ui_shell_endurance_key(shell,true,4,2999);assert(wristflow_ui_shell_endurance(shell));
    wristflow_ui_shell_endurance_key(shell,true,4,3000);advance(240);surface(WRISTFLOW_SURFACE_HOME);
    assert(wristflow_ui_shell_keep_minutes(shell)==5); /* Original deadline, not a fresh five minutes. */
    wristflow_settings_t active;assert(wristflow_ui_shell_get_settings(shell,&active));
    assert(!active.endurance_mode && active.wrist_wake);
    mode(true);advance(310000);mode(false);assert(!wristflow_ui_shell_keep_minutes(shell));
    mode(true);
    s.power.input_observation.valid=false;s.power.input_observation.good=false;wristflow_ui_shell_update(shell,&s);
    s.power.input_observation.valid=true;s.power.input_observation.good=true;wristflow_ui_shell_update(shell,&s);
    assert(wristflow_ui_shell_endurance(shell)); /* Unknown does not invent unplug. */
    s.power.input_observation.good=false;wristflow_ui_shell_update(shell,&s);
    s.power.input_observation.valid=false;s.power.input_observation.good=true;wristflow_ui_shell_update(shell,&s);
    assert(wristflow_ui_shell_endurance(shell));
    s.power.input_observation.valid=true;wristflow_ui_shell_charging_event(shell,&s);advance(240);
    assert(!wristflow_ui_shell_endurance(shell));surface(WRISTFLOW_SURFACE_HOME);
    cached_input=s.power.input_observation;
    ++cached_input.insertions; /* Confirmed before entering, old UI snapshot. */
    wristflow_ui_shell_bind_input_observation(shell,read_input,&cached_input);
    mode(true);s.power.input_observation=cached_input;wristflow_ui_shell_update(shell,&s);
    assert(wristflow_ui_shell_endurance(shell));mode(false);
    wristflow_ui_shell_bind_input_observation(shell,NULL,NULL);
    /* Screen-off ADC cache is empty, both charger mailbox edges coalesced,
     * and no charging presentation exists (full/fault/refused). */
    mode(true);advance(11000);assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    s.power.input_valid=false;s.power.voltage_valid=false;s.power.charge_cycle=0;
    ++s.power.input_observation.insertions;
    lv_timer_enable(false);wristflow_ui_shell_charging_event(shell,&s);
    assert(!wristflow_ui_shell_endurance(shell));lv_timer_enable(true);advance(240);surface(WRISTFLOW_SURFACE_HOME);
    mode(true);++s.power.input_observation.insertions;s.power.input_observation.good=false;
    wristflow_ui_shell_charging_event(shell,&s);advance(240);
    assert(!wristflow_ui_shell_endurance(shell)); /* Merged low/high/low. */
    s.power.input_observation.good=true;wristflow_ui_shell_update(shell,&s);
    mode(true);wristflow_ui_shell_sync_settings_save(shell);
    uint8_t record[WRISTFLOW_SETTINGS_BYTES];assert(wristflow_settings_encode(&saved,record));
    wristflow_settings_t restored;assert(wristflow_settings_decode(&restored,record,sizeof record));
    wristflow_ui_shell_destroy(shell);create(&s,&restored);
    assert(wristflow_ui_shell_endurance(shell));surface(WRISTFLOW_SURFACE_ENDURANCE);
    assert(!wristflow_ui_shell_keep_minutes(shell));
    s.time_unavailable=true;s.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].total_valid=false;
    wristflow_ui_shell_update(shell,&s);assert(!strcmp(label("endurance_time"),"--:--"));
    assert(!strcmp(label("endurance_date"),"--/--") && !strcmp(label("endurance_steps"),"步数 --"));
    s.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].total_valid=true;
    s.activity.metrics[WRISTFLOW_ACTIVITY_STEPS].total=0;wristflow_ui_shell_update(shell,&s);
    assert(!strcmp(label("endurance_steps"),"步数 0"));
    active=restored;active.endurance_mode=false;assert(!wristflow_ui_shell_configure(shell,&active));
    wristflow_ui_shell_destroy(shell);lv_deinit();return 0;
}
