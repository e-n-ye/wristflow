#include "product_ui.h"
#include "product_countdown.h"
#include "product_pm.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static wristflow_ui_shell_t *shell;
static const wristflow_countdown_port_t *port;
static lv_indev_data_t pointer;
static uint8_t draw[390*40*3], frame[390*450*3];
static unsigned brightness;
static const char *directory;
static uint32_t tick(void) { return rt_tick_get(); }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{
    unsigned width=(unsigned)(a->x2-a->x1+1);
    for (int y=a->y1; y<=a->y2; ++y)
        memcpy(frame+(y*390+a->x1)*3, p+(y-a->y1)*width*3, width*3);
    lv_display_flush_ready(d);
}
static void read_pointer(lv_indev_t *i, lv_indev_data_t *d)
{
    (void)i; *d=pointer;
    if (!wristflow_ui_shell_filter_touch(shell, d->state == LV_INDEV_STATE_PRESSED)) d->state=LV_INDEV_STATE_RELEASED;
}
static void set_brightness(uint8_t value, void *context) { (void)context; brightness=value; }
static void advance(unsigned ms)
{
    for (unsigned i=0; i<ms; i+=16) {
        test_countdown_advance(16, true);
        wristflow_ui_shell_countdown_event(shell);
        lv_timer_handler();
    }
    lv_obj_update_layout(lv_screen_active());
    lv_obj_update_layout(lv_layer_top());
}
static wristflow_countdown_t task(void) { wristflow_countdown_t t; port->read(NULL, &t); return t; }
static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *obj=lv_obj_find_by_name(root, name); if (!obj) fprintf(stderr,"missing %s\n",name); assert(obj); return obj; }
static void sample(int x, int y, bool down)
{ pointer.point=(lv_point_t){x,y}; pointer.state=down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED; advance(16); }
static void click(lv_obj_t *root, const char *name)
{
    lv_obj_t *obj=named(root,name);
    lv_obj_scroll_to_view(obj,LV_ANIM_OFF); advance(32);
    lv_area_t a; lv_obj_get_coords(obj,&a);
    int x=(a.x1+a.x2)/2, y=(a.y1+a.y2)/2;
    assert(x>=0 && x<390 && y>=0 && y<450 && lv_obj_is_visible(obj));
    sample(x,y,true); sample(x,y,false); advance(240);
}
static void press(const char *name) { click(lv_screen_active(),name); }
static void swipe(int x1, int y1, int x2, int y2)
{
    sample(x1,y1,true);
    for (int i=1; i<=12; ++i) sample(x1+(x2-x1)*i/12,y1+(y2-y1)*i/12,true);
    sample(x2,y2,false); advance(480);
}
static void alert_press(const char *name) { click(named(lv_layer_top(),"countdown_reminder"),name); }
static void open(wristflow_surface_t value)
{
    if (wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME &&
        value!=WRISTFLOW_SURFACE_COUNTDOWN) {
        assert(wristflow_ui_shell_key(shell)); advance(240);
    }
    bool accepted=wristflow_ui_shell_open(shell,value);
    if (!accepted) fprintf(stderr,"open target=%u from=%u phase=%u at=%u reminder=%u\n",value,
        wristflow_ui_shell_navigation(shell)->surface,wristflow_ui_shell_display_phase(shell),tick(),
        wristflow_ui_shell_reminder_active(shell));
    assert(accepted); advance(240); assert(wristflow_ui_shell_navigation(shell)->surface==value);
}
static void home(void) { assert(wristflow_ui_shell_home(shell)); advance(240); }
static void wake(void) { assert(wristflow_ui_shell_key(shell)); advance(240); }
static void sleep_for(unsigned ms)
{
    /* Actual backend callback executes while ALL LVGL timers are disabled. */
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    lv_timer_enable(false); test_countdown_advance(ms,true);
}
static void present(void)
{
    assert(task().phase == WRISTFLOW_COUNTDOWN_EXPIRED && task().expiry_pending);
    assert(wristflow_ui_shell_countdown_event(shell));
    assert(wristflow_ui_shell_reminder_active(shell) && !task().expiry_pending);
    assert(!wristflow_ui_shell_countdown_event(shell));
    lv_timer_enable(true); advance(32);
}
static void start(unsigned seconds)
{ assert(port->command(NULL,WF_COUNTDOWN_START,seconds)); wristflow_ui_shell_countdown_event(shell); }
static void capture(const char *name)
{
    lv_obj_invalidate(lv_screen_active()); lv_obj_invalidate(lv_layer_top()); lv_refr_now(NULL);
    char path[1024]; snprintf(path,sizeof path,"%s/countdown_product_%s.ppm",directory,name);
    FILE *file=fopen(path,"wb"); assert(file); fprintf(file,"P6\n390 450\n255\n");
    for (unsigned i=0; i<390*450; ++i) {
        unsigned char rgb[]={frame[i*3+2],frame[i*3+1],frame[i*3]}; assert(fwrite(rgb,1,3,file)==3);
    }
    assert(fclose(file)==0);
}
static unsigned timers(void)
{ unsigned n=0; for (lv_timer_t *t=lv_timer_get_next(NULL); t; t=lv_timer_get_next(t)) ++n; return n; }

int main(int argc, char **argv)
{
    assert(argc==2); directory=argv[1]; lv_init(); lv_tick_set_cb(tick);
    lv_display_t *display=lv_display_create(390,450);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display,draw,NULL,sizeof draw,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input,display); lv_indev_set_read_cb(input,read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input),16);
    wristflow_ui_init(""); unsigned initial_timers=timers();
    port=wristflow_product_countdown_start();
    wristflow_settings_t settings=wristflow_settings_default();
    wristflow_watch_snapshot_t snapshot=wristflow_product_snapshot(false,0);
    shell=wristflow_product_ui_create_with_countdown(&snapshot,&settings,set_brightness,NULL,NULL,NULL,NULL,NULL,port);
    assert(shell); wristflow_ui_shell_enable_display_policy(shell); advance(240);
    wake(); assert(lv_obj_find_by_name(lv_screen_active(),"launch_countdown"));
    open(WRISTFLOW_SURFACE_COUNTDOWN); press("countdown_preset_1");
    assert(task().phase==WRISTFLOW_COUNTDOWN_RUNNING);
    press("countdown_toggle"); uint32_t remaining=task().remaining_ms;
    advance(11000); sleep_for(80000); assert(task().phase==WRISTFLOW_COUNTDOWN_PAUSED && task().remaining_ms==remaining);
    lv_timer_enable(true); wake(); press("countdown_toggle");
    assert(task().phase==WRISTFLOW_COUNTDOWN_RUNNING);
    press("countdown_cancel"); assert(task().phase==WRISTFLOW_COUNTDOWN_IDLE);
    home(); start(20); open(WRISTFLOW_SURFACE_SETTINGS);
    lv_obj_t *original=lv_screen_active(); advance(11000);
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    wristflow_ui_shell_set_dnd(shell,true);
    unsigned sends=test_countdown_sends(); sleep_for(10000);
    assert(test_countdown_sends()==sends+1); present();
    assert(lv_screen_active()==original && brightness==60);
    assert(!wristflow_ui_shell_back(shell) && !wristflow_ui_shell_home(shell));
    assert(!wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_WEATHER));
    capture("expired_off_dnd");
    advance(30100); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF && brightness==0);
    sleep_for(10000); assert(wristflow_ui_shell_reminder_active(shell));
    lv_timer_enable(true); wake(); assert(lv_screen_active()==original && brightness==60);
    alert_press("countdown_close"); assert(!wristflow_ui_shell_reminder_active(shell) && lv_screen_active()==original);
    assert(task().phase==WRISTFLOW_COUNTDOWN_IDLE);

    /* Settings confirmation survives a valid covered session without approval. */
    open(WRISTFLOW_SURFACE_SCREEN_TIMEOUT); press("choice_60");
    lv_obj_t *dialog=named(lv_screen_active(),"settings_confirm"); original=lv_screen_active();
    start(2); advance(2200);
    assert(wristflow_ui_shell_reminder_active(shell) && lv_obj_is_valid(dialog));
    capture("settings_covered"); alert_press("countdown_close");
    assert(lv_screen_active()==original && named(original,"settings_confirm")==dialog);
    wristflow_ui_shell_get_settings(shell,&settings); assert(settings.screen_timeout==10);
    press("confirm_cancel");

    /* Original sleep time is not reset by reminder wake cycles or notifications. */
    press("choice_60"); original=lv_screen_active(); start(150); advance(11000);
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    sleep_for(140000); present();
    wristflow_ui_shell_set_dnd(shell,false);
    wristflow_notifications_snapshot_t phone={.connected=true,.subscribed=true,.revision=1,.alert_sequence=1,.alert_id=42,.count=1};
    phone.messages[0].id=42; strcpy(phone.messages[0].source,"QQ"); strcpy(phone.messages[0].title,"期间消息");
    assert(!wristflow_ui_shell_notifications(shell,&phone));
    assert(wristflow_ui_shell_phone_connected(shell));
    assert(lv_obj_has_flag(named(lv_layer_top(),"notification_banner"),LV_OBJ_FLAG_HIDDEN));
    assert(!wristflow_ui_shell_open_notification(shell,true,42));
    advance(30100); wake(); alert_press("countdown_close");
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);
    assert(!lv_obj_find_by_name(lv_screen_active(),"settings_confirm"));
    assert(wristflow_ui_shell_open_notification(shell,true,42)); advance(240);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"message_title")),"期间消息")); home();

    /* Reminder time counts toward a covered lit page's original sleep deadline. */
    open(WRISTFLOW_SURFACE_SETTINGS); start(1); advance(1200);
    assert(wristflow_ui_shell_reminder_active(shell));
    advance(140000); wake(); alert_press("countdown_close");
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);

    /* Top-layer draft confirmation is suspended, then resumes after long sleep. */
    swipe(320,125,60,125);
    sample(100,110,true); advance(736); sample(100,110,false); advance(240);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    press("edit_right"); press("quarters"); assert(wristflow_ui_shell_key(shell)); advance(240);
    dialog=lv_obj_get_parent(named(lv_layer_top(),"confirm_accept")); original=lv_screen_active();
    start(1); advance(1200); assert(wristflow_ui_shell_reminder_active(shell));
    assert(lv_obj_has_flag(dialog,LV_OBJ_FLAG_HIDDEN)); capture("draft_covered");
    advance(140000); wake(); alert_press("countdown_close");
    assert(lv_screen_active()==original && lv_obj_is_visible(dialog));
    click(dialog,"confirm_cancel"); assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    assert(wristflow_ui_shell_key(shell)); advance(240); click(dialog,"confirm_accept");
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);

    /* Stopwatch and its pending exit request continue unchanged under reminders. */
    open(WRISTFLOW_SURFACE_STOPWATCH); press("stopwatch_toggle"); advance(1100);
    assert(wristflow_ui_shell_home(shell)); advance(240);
    dialog=named(lv_screen_active(),"stopwatch_exit_confirm"); original=lv_screen_active();
    start(2); advance(2200); advance(140000); wake(); alert_press("countdown_close");
    assert(lv_screen_active()==original && lv_obj_is_visible(dialog));
    assert(strcmp(lv_label_get_text(named(original,"stopwatch_time")),"00:00"));
    press("confirm_accept"); assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);

    /* Repeat preserves original duration after pause, while retaining prior page. */
    start(3); advance(700); assert(port->command(NULL,WF_COUNTDOWN_PAUSE,0));
    advance(5000); assert(port->command(NULL,WF_COUNTDOWN_RESUME,0)); advance(2400);
    assert(wristflow_ui_shell_reminder_active(shell)); alert_press("countdown_repeat");
    assert(task().phase==WRISTFLOW_COUNTDOWN_RUNNING && task().duration_ms==3000);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);
    advance(3200); alert_press("countdown_close");
    wristflow_ui_shell_destroy(shell); shell=NULL; lv_timer_handler();
    assert(timers()==initial_timers && !test_countdown_wait());
    assert(!lv_obj_find_by_name(lv_layer_top(),"reminder_host"));
    puts("Product countdown UI: LVGL-off expiry, DND, pending wake, prior clock, prompts, notifications and repeat passed");
    return 0;
}
