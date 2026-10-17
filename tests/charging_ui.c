#include "product_ui.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static wristflow_ui_shell_t *shell;
static wristflow_watch_snapshot_t state;
static uint32_t ticks;
static unsigned brightness;
static uint8_t draw[390*40*3], frame[390*450*3];
static lv_indev_data_t pointer;
static const char *renders;
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{
    unsigned width = a->x2-a->x1+1;
    for (int y=a->y1;y<=a->y2;++y) memcpy(frame+(y*390+a->x1)*3,p+(y-a->y1)*width*3,width*3);
    lv_display_flush_ready(d);
}
static void read_pointer(lv_indev_t *i, lv_indev_data_t *d)
{ (void)i; *d=pointer; if (!wristflow_ui_shell_filter_touch(shell,d->state==LV_INDEV_STATE_PRESSED)) d->state=LV_INDEV_STATE_RELEASED; }
static void set_brightness(uint8_t b, void *c) { (void)c; brightness=b; }
static void advance(unsigned ms)
{ for (unsigned i=0;i<ms;i+=16) { ticks+=16; lv_timer_handler(); } lv_obj_update_layout(lv_screen_active()); }
static void jump(unsigned ms) { ticks+=ms; lv_timer_handler(); lv_obj_update_layout(lv_screen_active()); }
static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *o=lv_obj_find_by_name(root,name); assert(o); return o; }
static bool visible(void)
{ lv_obj_t *o=lv_obj_find_by_name(lv_layer_top(),"charging_panel"); return o&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN); }
static void sample(bool down)
{ pointer.point=(lv_point_t){195,360}; pointer.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; advance(32); }
static void tap(void) { sample(true); sample(false); advance(240); }
static bool sync(void)
{ bool opened=wristflow_ui_shell_charging_event(shell,&state); assert(wristflow_ui_shell_update(shell,&state)); advance(32); return opened; }
static void capture(const char *name)
{
    lv_obj_invalidate(lv_screen_active()); lv_obj_invalidate(lv_layer_top()); lv_refr_now(NULL);
    char path[1024]; snprintf(path,sizeof path,"%s/charging_%s.ppm",renders,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n390 450\n255\n");
    for (unsigned i=0;i<390*450;++i) { uint8_t rgb[]={frame[i*3+2],frame[i*3+1],frame[i*3]}; assert(fwrite(rgb,1,3,f)==3); }
    fclose(f);
}
int main(int argc,char **argv)
{
    assert(argc==2); renders=argv[1]; lv_init(); lv_tick_set_cb(tick);
    lv_display_t *d=lv_display_create(390,450); lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d,draw,NULL,sizeof draw,LV_DISPLAY_RENDER_MODE_PARTIAL); lv_display_set_flush_cb(d,flush);
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input,d); lv_indev_set_read_cb(input,read_pointer); lv_timer_set_period(lv_indev_get_read_timer(input),16);
    wristflow_ui_init(""); wristflow_settings_t settings=wristflow_settings_default();
    state=wristflow_product_snapshot(false,0); state.time_unavailable=false; state.hour_24=21; state.minute=10;
    shell=wristflow_product_ui_create(&state,&settings,set_brightness,NULL);
    wristflow_ui_shell_enable_display_policy(shell); advance(240);
    assert(wristflow_ui_shell_key(shell)); advance(240);
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_STOPWATCH)); advance(240);
    lv_obj_t *original=lv_screen_active();
    state.power.charge=WF_POWER_CHARGE_ACTIVE; state.power.charge_cycle=1;
    sample(true); /* Insertion while held cannot turn the eventual release into a click. */
    assert(sync()&&visible()); assert(lv_screen_active()==original);
    sample(false); assert(visible());
    lv_obj_t *ring=named(lv_layer_top(),"charging_ring"); lv_area_t area; lv_obj_get_coords(ring,&area);
    assert(area.x1==24&&area.y1==24&&area.x2==365&&area.y2==425);
    assert(lv_color_eq(lv_obj_get_style_border_color(ring,0),lv_color_hex(0x163b38)));
    assert(!strcmp(lv_label_get_text(named(lv_layer_top(),"charging_clock")),"21:10"));
    lv_font_glyph_dsc_t glyph;
    assert(lv_font_get_glyph_dsc(icons_44,&glyph,0xf0e7,0));
    const uint32_t words[]={0x5145,0x7535,0x4e2d,0x5df2,0x6ee1,0x5f02,0x5e38,0x672a,0x77e5};
    for (unsigned i=0;i<sizeof words/sizeof *words;++i) assert(lv_font_get_glyph_dsc(notification_22,&glyph,words[i],0));
    capture("unknown_soc"); state.minute=11; assert(!sync());
    assert(!strcmp(lv_label_get_text(named(lv_layer_top(),"charging_clock")),"21:11"));
    tap(); assert(!visible()&&!wristflow_ui_shell_charging_active(shell));
    assert(lv_screen_active()==original&&!strcmp(lv_label_get_text(named(original,"stopwatch_time")),"00:00"));
    assert(!sync()&&!visible()); /* No re-presentation after exit, even on wake. */
    jump(11000); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    assert(wristflow_ui_shell_key(shell)); advance(32); assert(!sync()&&!visible());
    state.power.charge_cycle=2; assert(sync()&&visible());
    jump(6500); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_DIM);
    tap(); assert(visible()&&brightness==60); /* First DIM touch only brightens. */
    tap(); assert(!visible()&&lv_screen_active()==original);
    state.power.charge_cycle=3; assert(sync()); jump(6500);
    assert(wristflow_ui_shell_key(shell)&&visible()&&brightness==60);
    assert(wristflow_ui_shell_key(shell)&&!visible()&&lv_screen_active()==original);
    state.power.charge_cycle=4; assert(sync());
    state.power.charge=WF_POWER_CHARGE_FULL; assert(!sync()&&visible());
    assert(lv_color_eq(lv_obj_get_style_border_color(ring,0),lv_color_hex(0x00dfc0)));
    assert(!strcmp(lv_label_get_text(named(lv_layer_top(),"charging_state")),"已充满")); capture("full");
    jump(11000); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF&&brightness==0);
    assert(!sync()&&brightness==0); /* FULL must not wake. */
    jump(125000); assert(wristflow_ui_shell_key(shell)); assert(visible()&&lv_screen_active()==original);
    assert(wristflow_ui_shell_key(shell)&&!visible()&&lv_screen_active()==original);
    assert(!sync()&&!visible());
    state.power.charge_cycle=5; state.power.charge=WF_POWER_CHARGE_ACTIVE; assert(sync());
    state.power.charge=WF_POWER_CHARGE_UNKNOWN; assert(!sync()&&visible());
    state.time_unavailable=true; assert(!sync());
    assert(!strcmp(lv_label_get_text(named(lv_layer_top(),"charging_clock")),"--:--")); capture("unknown_time");
    state.power.charge_cycle=0; state.power.charge=WF_POWER_CHARGE_STOPPED; assert(!sync()&&!visible());
    /* New charge cannot cover a reminder; latest authoritative cycle is retried. */
    lv_obj_t *reminder=lv_obj_create(lv_layer_top()); lv_obj_set_size(reminder,390,450);
    assert(wristflow_ui_shell_present_reminder(shell,reminder));
    state.power.charge_cycle=6; state.power.charge=WF_POWER_CHARGE_ACTIVE;
    assert(!sync()&&!visible()); wristflow_ui_shell_close_reminder(shell);
    assert(sync()&&visible()&&lv_screen_active()==original);
    assert(wristflow_ui_shell_present_reminder(shell,reminder)); assert(!visible());
    assert(wristflow_ui_shell_key(shell)&&wristflow_ui_shell_reminder_active(shell));
    wristflow_ui_shell_close_reminder(shell); assert(visible());
    state.power.charge=WF_POWER_CHARGE_FAULT; assert(!sync()&&visible()); capture("fault");
    assert(wristflow_ui_shell_key(shell)&&!visible());
    state.power.charge=WF_POWER_CHARGE_FULL; assert(!sync()&&!visible());
    /* A request withdrawn before consumption must never open an old cycle. */
    state.power.charge_cycle=0; assert(!sync());
    state.power.charge_cycle=7; state.power.charge=WF_POWER_CHARGE_ACTIVE; assert(sync());
    wristflow_notifications_snapshot_t phone={.connected=true,.subscribed=true,.count=1,
        .revision=1,.alert_sequence=1,.alert_id=1};
    phone.messages[0].id=1; strcpy(phone.messages[0].title,"消息"); strcpy(phone.messages[0].body,"充电期间");
    assert(!wristflow_ui_shell_notifications(shell,&phone)); advance(32);
    assert(!visible()&&wristflow_ui_shell_charging_active(shell));
    advance(3100); assert(visible()); /* Banner expiration restores charge, not a new cycle. */
    assert(wristflow_ui_shell_key(shell)&&!visible());
    assert(wristflow_ui_shell_home(shell)); advance(240);
    assert(wristflow_ui_shell_key(shell)); advance(240);
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_BREATHING)); advance(240);
    lv_obj_t *breathing=lv_screen_active();
    lv_obj_send_event(named(breathing,"breathing_start"),LV_EVENT_SHORT_CLICKED,NULL); advance(1000);
    state.power.charge_cycle=8; assert(sync());
    lv_obj_t *breathing_ring=named(breathing,"breathing_training_ring");
    int size=lv_obj_get_width(breathing_ring); advance(2200);
    assert(lv_obj_get_width(breathing_ring)==size); /* Covered training is paused. */
    assert(wristflow_ui_shell_key(shell)&&!visible()&&lv_screen_active()==breathing);
    advance(1000); assert(lv_obj_get_width(breathing_ring)>140);
    assert(wristflow_ui_shell_key(shell)); /* Pending exit confirmation is part of the original session. */
    lv_obj_t *dialog=named(breathing,"breathing_exit_confirm");
    state.power.charge_cycle=9; assert(sync()&&lv_obj_has_flag(dialog,LV_OBJ_FLAG_HIDDEN));
    jump(125000); assert(wristflow_ui_shell_key(shell)&&visible());
    assert(wristflow_ui_shell_key(shell)&&!visible()&&lv_screen_active()==breathing);
    assert(!lv_obj_has_flag(dialog,LV_OBJ_FLAG_HIDDEN));
    lv_obj_send_event(named(breathing,"confirm_cancel"),LV_EVENT_SHORT_CLICKED,NULL); advance(32);
    wristflow_ui_shell_destroy(shell); lv_deinit();
    puts("charging presentation, pointer consumption, cycle, sleep and reminder checks passed");
    return 0;
}
