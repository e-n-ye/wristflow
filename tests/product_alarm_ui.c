#include "product_ui.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static wristflow_ui_shell_t *shell;
static wristflow_alarm_schedule_t alarms;
static wristflow_alarm_clock_t clock_value;
static wristflow_countdown_t countdown;
static wristflow_alarm_command_t requested;
static uint32_t request_id, pending_id, ticks;
static wristflow_save_state_t status_value;
static lv_indev_data_t pointer;
static uint8_t draw[390*40*3], frame[390*450*3];
static const char *directory;
static unsigned brightness;
static uint32_t held_alarm;
static bool cross_sample, reveal_on_release, reveal_pressed;
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d,const lv_area_t*a,uint8_t*p) {
    unsigned w=a->x2-a->x1+1;
    for(int y=a->y1;y<=a->y2;++y)memcpy(frame+(y*390+a->x1)*3,p+(y-a->y1)*w*3,w*3);
    lv_display_flush_ready(d);
}
static void read_pointer(lv_indev_t*i,lv_indev_data_t*d) { (void)i;*d=pointer;
    if(reveal_on_release&&d->state==LV_INDEV_STATE_PRESSED)reveal_pressed=true;
    if(reveal_on_release&&reveal_pressed&&d->state==LV_INDEV_STATE_RELEASED) { held_alarm=0;reveal_on_release=false; }
    if(!wristflow_ui_shell_filter_touch(shell,d->state==LV_INDEV_STATE_PRESSED))d->state=LV_INDEV_STATE_RELEASED;
}
static void bright(uint8_t v,void*c) { (void)c;brightness=v; }
static uint32_t request(void*c,const wristflow_alarm_command_t*cmd) { (void)c;assert(!pending_id);
    requested=*cmd;pending_id=++request_id;status_value=WRISTFLOW_SAVE_PENDING;return pending_id;
}
static wristflow_save_state_t status(void*c,uint32_t id) { (void)c;assert(id==request_id);return status_value; }
static void read_alarm(void*c,wristflow_alarm_snapshot_t*s) { (void)c;memset(s,0,sizeof *s);
    s->alarms=alarms.data.alarms;memcpy(s->pending,alarms.pending,sizeof s->pending);s->time_valid=alarms.time_valid;
    for(unsigned i=0;i<5;++i)if(s->pending[i].alarm.id==held_alarm)s->pending[i].active=false;
}
static bool ack(void*c,uint32_t id,uint32_t seq) { (void)c;return wristflow_alarm_schedule_ack(&alarms,id,seq); }
static void sync_alarm(void*c) { (void)c;wristflow_alarm_schedule_update(&alarms,clock_value); }
static void read_countdown(void*c,wristflow_countdown_t*t) { (void)c;*t=countdown; }
static uint32_t now(void*c) { (void)c;return (uint32_t)clock_value.monotonic_ms; }
static uint64_t now64(void*c) { (void)c;return clock_value.monotonic_ms; }
static void sync_countdown(void*c) {
    if(cross_sample) { cross_sample=false;++ticks;++clock_value.monotonic_ms;++clock_value.utc;clock_value.subsecond_ms=0; }
    wristflow_countdown_update(&countdown,now(c));
}
static bool command(void*c,wristflow_countdown_command_t op,uint32_t arg) {
    switch(op) {
    case WF_COUNTDOWN_START:return wristflow_countdown_start(&countdown,arg,now(c));
    case WF_COUNTDOWN_ACK:return arg==countdown.generation&&wristflow_countdown_take_expiry(&countdown);
    case WF_COUNTDOWN_DISMISS:case WF_COUNTDOWN_CANCEL:wristflow_countdown_cancel(&countdown);return true;
    case WF_COUNTDOWN_REPEAT:return wristflow_countdown_repeat(&countdown,now(c));
    default:return false;
    }
}
static const wristflow_alarm_port_t alarm_port={.request=request,.status=status,.read=read_alarm,.ack=ack,.sync=sync_alarm,.now64=now64};
static const wristflow_countdown_port_t countdown_port={.command=command,.read=read_countdown,.now=now,.now64=now64,.sync=sync_countdown};
static void advance(unsigned ms) {
    for(unsigned i=0;i<ms;i+=16) { ticks+=16;clock_value.monotonic_ms+=16;
        clock_value.utc=1791673140+(uint32_t)(clock_value.monotonic_ms/1000);
        clock_value.subsecond_ms=(uint16_t)(clock_value.monotonic_ms%1000);
        wristflow_ui_shell_reminder_event(shell);lv_timer_handler();
    }
    lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top());
}
static lv_obj_t *named(lv_obj_t*r,const char*n) { lv_obj_t*o=lv_obj_find_by_name(r,n);assert(o);return o; }
static void sample(int x,int y,bool down) { pointer.point=(lv_point_t){x,y};pointer.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;advance(48); }
static void click(lv_obj_t*r,const char*n) {
    lv_obj_t*o=named(r,n);lv_obj_scroll_to_view(o,LV_ANIM_OFF);advance(32);lv_area_t a;lv_obj_get_coords(o,&a);
    if(!lv_obj_is_visible(o))fprintf(stderr,"hidden click %s at %u\n",n,ticks);
    assert(lv_obj_is_visible(o));sample((a.x1+a.x2)/2,(a.y1+a.y2)/2,true);sample((a.x1+a.x2)/2,(a.y1+a.y2)/2,false);advance(240);
}
static void press(const char*n) { click(lv_screen_active(),n); }
static void finish_save(bool success) {
    assert(pending_id);if(success)assert(wristflow_alarm_schedule_command(&alarms,&requested,clock_value));
    pending_id=0;status_value=success?WRISTFLOW_SAVE_DONE:WRISTFLOW_SAVE_FAILED;advance(160);
}
static void text(lv_obj_t*r,const char*n,const char*t) { assert(!strcmp(lv_label_get_text(named(r,n)),t)); }
static void capture(const char*n) {
    lv_obj_invalidate(lv_layer_top());lv_obj_invalidate(lv_screen_active());lv_refr_now(NULL);
    char path[1024];snprintf(path,sizeof path,"%s/alarm_product_%s.ppm",directory,n);
    FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n390 450\n255\n");
    for(unsigned i=0;i<390*450;++i) { uint8_t rgb[]={frame[i*3+2],frame[i*3+1],frame[i*3]};assert(fwrite(rgb,1,3,f)==3); }assert(!fclose(f));
}
static lv_obj_t *current_alarm(void) {
    lv_obj_t*host=named(lv_layer_top(),"reminder_host");
    for(unsigned i=0;i<lv_obj_get_child_count(host);++i) { lv_obj_t*c=lv_obj_get_child(host,i);
        if(!lv_obj_has_flag(c,LV_OBJ_FLAG_HIDDEN)&&lv_obj_find_by_name(c,"alarm_reminder_close"))return c; }
    return NULL;
}
static lv_obj_t *first_hidden_alarm(void) {
    for(unsigned i=0;i<lv_obj_get_child_count(lv_layer_top());++i) {
        lv_obj_t*c=lv_obj_get_child(lv_layer_top(),i);
        if(lv_obj_has_flag(c,LV_OBJ_FLAG_HIDDEN)&&lv_obj_find_by_name(c,"alarm_reminder_close"))return c;
    }
    assert(false);return NULL;
}
static unsigned timers(void) { unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n; }
int main(int argc,char**argv) {
    assert(argc==2);directory=argv[1];lv_init();lv_tick_set_cb(tick);
    lv_display_t*d=lv_display_create(390,450);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d,draw,NULL,sizeof draw,LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
    wristflow_ui_init("");wristflow_alarm_schedule_init(&alarms);wristflow_countdown_init(&countdown);
    const uint32_t chinese[]={0x7b49,0x5f85,0x6821,0x65f6,0x4fdd,0x5b58,0x4e2d,0x5931,0x8d25,0xff0c,0x7a0d,0x540e,0x91cd,0x8bd5};
    for(unsigned i=0;i<sizeof chinese/sizeof chinese[0];++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(body_20,&glyph,chinese[i],0)&&!glyph.is_placeholder);
    }
    clock_value=(wristflow_alarm_clock_t){.utc=1791673140,.valid=false};
    wristflow_settings_t settings=wristflow_settings_default();wristflow_watch_snapshot_t s={.time_unavailable=true,.battery_unavailable=true};
    unsigned initial=timers();shell=wristflow_product_ui_create_with_timers(&s,&settings,bright,NULL,NULL,NULL,NULL,NULL,&countdown_port,&alarm_port);assert(shell);
    lv_indev_t*input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_pointer);advance(400);
    wristflow_ui_shell_enable_display_policy(shell);assert(wristflow_ui_shell_key(shell));advance(240);
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_ALARM));advance(240);
    text(lv_screen_active(),"alarm_status","等待校时");capture("waiting");
    press("alarm_add");press("alarm_time_accept");press("alarm_save");assert(alarms.data.alarms.count==0);
    text(lv_screen_active(),"alarm_status","保存中");capture("saving");
    assert(wristflow_ui_shell_back(shell));text(lv_screen_active(),"alarm_title","设置");
    finish_save(false);assert(alarms.data.alarms.count==0);text(lv_screen_active(),"alarm_status","保存失败，稍后重试");capture("failed");
    press("alarm_save");finish_save(true);assert(alarms.data.alarms.count==1);text(lv_screen_active(),"alarm_title","闹钟");
    lv_obj_t*row=named(lv_screen_active(),"alarm_row_0");click(row,"alarm_enabled");assert(alarms.data.alarms.entries[0].enabled);
    finish_save(false);assert(lv_obj_has_state(named(row,"alarm_enabled"),LV_STATE_CHECKED));
    click(row,"alarm_enabled");finish_save(true);assert(!alarms.data.alarms.entries[0].enabled);
    click(row,"alarm_open");press("alarm_delete");press("confirm_cancel");assert(alarms.data.alarms.count==1);
    press("alarm_delete");press("confirm_accept");finish_save(false);assert(alarms.data.alarms.count==1);
    press("alarm_delete");press("confirm_accept");finish_save(true);assert(alarms.data.alarms.count==0);
    clock_value.valid=true;++clock_value.revision;sync_alarm(NULL);
    for(unsigned i=0;i<5;++i) {
        wristflow_alarm_command_t cmd={.operation=WF_ALARM_ADD,.draft={.hour=7,.enabled=true,.weekdays=WRISTFLOW_ALARM_DAILY}};
        assert(wristflow_alarm_schedule_command(&alarms,&cmd,clock_value));
    }
    /* An uncommitted editor survives the entire six-reminder sequence. */
    wristflow_ui_shell_reminder_event(shell);click(named(lv_screen_active(),"alarm_row_0"),"alarm_open");lv_obj_t*original=lv_screen_active();
    wristflow_ui_shell_set_dnd(shell,true);
    uint32_t until=60000-(uint32_t)clock_value.monotonic_ms;
    assert(wristflow_countdown_start(&countdown,1,now(NULL)));countdown.remaining_ms=until;countdown.anchor_ms=now(NULL);
    /* The common deadline falls between producer samples. A frozen cutoff
     * withholds both until the next batch, which must select the alarm first. */
    lv_timer_enable(false);ticks=59999;clock_value.monotonic_ms=59999;clock_value.utc=1791673199;clock_value.subsecond_ms=999;
    held_alarm=alarms.data.alarms.entries[1].id;cross_sample=true;
    assert(!wristflow_ui_shell_reminder_event(shell));assert(!wristflow_ui_shell_reminder_active(shell));
    assert(wristflow_ui_shell_reminder_event(shell));assert(!wristflow_ui_shell_reminder_event(shell));lv_timer_enable(true);advance(32);
    assert(wristflow_ui_shell_reminder_active(shell));assert(current_alarm());text(current_alarm(),"alarm_reminder_time","07:00");capture("first");
    /* B is pending only in the backend when A's release is dispatched. The
     * already-queued C must remain unseen until B has been collected/sorted. */
    lv_obj_t*c=first_hidden_alarm();reveal_on_release=true;click(current_alarm(),"alarm_reminder_close");
    assert(current_alarm()&&current_alarm()!=c);click(current_alarm(),"alarm_reminder_close");assert(current_alarm()==c);
    wristflow_notifications_snapshot_t phone={.connected=true,.revision=1,.alert_sequence=1,.alert_id=42,.count=1};phone.messages[0].id=42;
    strcpy(phone.messages[0].source,"QQ");strcpy(phone.messages[0].title,"期间消息");assert(!wristflow_ui_shell_notifications(shell,&phone));
    advance(30100);assert(brightness==0&&wristflow_ui_shell_reminder_active(shell));
    /* New same-record arrivals coalesce and renew light without preemption. */
    for(unsigned i=0;i<5;++i) { alarms.pending[i]=(wristflow_alarm_pending_t){.alarm=alarms.data.alarms.entries[i],.due_ms=clock_value.monotonic_ms,.sequence=100+i,.active=true}; }
    assert(wristflow_ui_shell_reminder_event(shell));assert(brightness>0);assert(current_alarm());
    /* Old C/D/E precede countdown; newly repeated A/B have later deadlines. */
    for(unsigned i=0;i<3;++i) { lv_obj_t*alert=current_alarm();assert(alert);click(alert,"alarm_reminder_close");assert(lv_screen_active()==original); }
    assert(!current_alarm()&&wristflow_ui_shell_reminder_active(shell));capture("countdown_last");
    click(named(lv_layer_top(),"countdown_reminder"),"countdown_close");assert(wristflow_ui_shell_reminder_active(shell));
    for(unsigned i=0;i<2;++i) { assert(current_alarm());click(current_alarm(),"alarm_reminder_close"); }
    assert(!wristflow_ui_shell_reminder_active(shell));
    assert(lv_screen_active()==original);text(original,"alarm_title","设置");
    assert(lv_obj_has_flag(named(lv_layer_top(),"notification_banner"),LV_OBJ_FLAG_HIDDEN));
    wristflow_ui_shell_destroy(shell);shell=NULL;lv_indev_delete(input);assert(timers()==initial);
    lv_display_delete(d);lv_deinit();puts("Product alarm UI: save/failure/delete/six-source queue/coalesce/context passed");return 0;
}
