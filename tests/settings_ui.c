#include "product_ui.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static uint8_t pixels[390 * 40 * 4];
static unsigned brightness;
static lv_indev_data_t pointer;
static wristflow_ui_shell_t *shell;
static const char *renders;
static bool check_wake_home;
static bool store_bound, reject_request;
static unsigned store_requests, store_queries;
static wristflow_save_state_t store_state;
static wristflow_settings_t submitted;
static unsigned previews, cancellations;
static uint32_t preview_id;
static wristflow_vibration_mode_t preview_mode;
static bool reject_preview;
static uint32_t preview(wristflow_vibration_mode_t mode, void *context)
{
    assert(context == &preview_id && mode != WRISTFLOW_VIBRATION_OFF);
    ++previews; preview_mode = mode;
    return reject_preview ? 0 : ++preview_id;
}
static void cancel_preview(uint32_t id, void *context)
{ assert(context == &preview_id && id == preview_id); ++cancellations; }
static uint32_t request_settings(const wristflow_settings_t *value, void *context)
{
    (void)context; ++store_requests; submitted=*value; store_state=WRISTFLOW_SAVE_PENDING;
    return reject_request ? 0 : store_requests;
}
static wristflow_save_state_t settings_status(uint32_t id, void *context)
{ (void)context; assert(id==store_requests); ++store_queries; return store_state; }
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) { (void)a; (void)p; lv_display_flush_ready(d); }
static void read_pointer(lv_indev_t *i, lv_indev_data_t *d)
{
    (void)i; *d = pointer;
    if (!wristflow_ui_shell_filter_touch(shell, d->state == LV_INDEV_STATE_PRESSED)) d->state = LV_INDEV_STATE_RELEASED;
}
static void set_brightness(uint8_t value, void *context)
{
    (void)context;
    if (check_wake_home && value) {
        assert(wristflow_ui_shell_navigation(shell)->surface == WRISTFLOW_SURFACE_HOME);
        assert(lv_display_get_screen_prev(NULL) == NULL);
    }
    brightness = value;
}
static void advance(unsigned ms)
{ for (unsigned i = 0; i < ms; i += 16) { ticks += 16; lv_timer_handler(); if(store_bound) wristflow_ui_shell_sync_settings_save(shell); } lv_obj_update_layout(lv_screen_active()); }
static void jump(unsigned ms) { ticks += ms; lv_timer_handler(); lv_obj_update_layout(lv_screen_active()); }
static void sample(int x, int y, bool down)
{ pointer.point = (lv_point_t){x,y}; pointer.state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED; advance(16); }
static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *o = lv_obj_find_by_name(root,name); if(!o) fprintf(stderr,"missing %s\n",name); assert(o); return o; }
static void tap(int x, int y) { sample(x,y,true); sample(x,y,false); advance(240); }
static void press(const char *name)
{
    lv_obj_t *o = named(lv_screen_active(),name);
    lv_obj_scroll_to_view(o,LV_ANIM_OFF); advance(32);
    lv_area_t a; lv_obj_get_coords(o,&a);
    int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    assert(x>=0 && x<390 && y>=0 && y<450); tap(x,y);
}
static void hold(const char *name)
{
    lv_obj_t *o=named(lv_screen_active(),name); lv_area_t a;
    lv_obj_scroll_to_view(o,LV_ANIM_OFF); advance(32); lv_obj_get_coords(o,&a);
    int x=(a.x1+a.x2)/2, y=(a.y1+a.y2)/2;
    sample(x,y,true); advance(800); sample(x,y,false); advance(240);
}
static void swipe(int x1,int y1,int x2,int y2)
{
    sample(x1,y1,true);
    for(int i=1;i<=12;++i) sample(x1+(x2-x1)*i/12,y1+(y2-y1)*i/12,true);
    sample(x2,y2,false); advance(480);
}
static void surface(wristflow_surface_t s) {
    if (wristflow_ui_shell_navigation(shell)->surface!=s)
        fprintf(stderr,"surface expected %u, actual %u at tick %u\n",s,wristflow_ui_shell_navigation(shell)->surface,ticks);
    assert(wristflow_ui_shell_navigation(shell)->surface==s);
}
static wristflow_settings_t settings(void)
{ wristflow_settings_t s; assert(wristflow_ui_shell_get_settings(shell,&s)); return s; }
static void back(void) { assert(wristflow_ui_shell_back(shell)); advance(240); }
static void key(void) { assert(wristflow_ui_shell_key(shell)); advance(240); }
static void home(void) { assert(wristflow_ui_shell_home(shell)); advance(240); }
static void open(wristflow_surface_t s) { assert(wristflow_ui_shell_open(shell,s)); advance(240); }
static void glyphs(lv_obj_t *o)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)) return;
    if(lv_obj_check_type(o,&lv_label_class)) {
        const unsigned char *c=(const unsigned char *)lv_label_get_text(o);
        const lv_font_t *font=lv_obj_get_style_text_font(o,0);
        while(*c) {
            uint32_t cp=*c++;
            if(cp>=0xe0) { cp=((cp&15)<<12)|((c[0]&63)<<6)|(c[1]&63); c+=2; }
            else if(cp>=0xc0) cp=((cp&31)<<6)|(*c++&63);
            if(cp=='\n') continue;
            lv_font_glyph_dsc_t g;
            if(!lv_font_get_glyph_dsc(font,&g,cp,0)||g.is_placeholder) { fprintf(stderr,"missing glyph U+%04x: %s\n",cp,lv_label_get_text(o)); assert(false); }
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);++i) glyphs(lv_obj_get_child(o,i));
}
static void snapshot(const char *name)
{
    glyphs(lv_screen_active());
    lv_draw_buf_t *b=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888); assert(b);
    char path[1024]; snprintf(path,sizeof path,"%s/settings_%s.ppm",renders,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n390 450\n255\n");
    for(unsigned y=0;y<450;++y) for(unsigned x=0;x<390;++x) {
        const uint8_t *p=b->data+y*b->header.stride+x*3; uint8_t rgb[]={p[2],p[1],p[0]}; assert(fwrite(rgb,1,3,f)==3);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
int main(int argc,char **argv)
{
    assert(argc==2); renders=argv[1]; lv_init(); lv_tick_set_cb(tick);
    lv_display_t *d=lv_display_create(390,450); lv_display_set_buffers(d,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL); lv_display_set_flush_cb(d,flush);
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER); lv_indev_set_display(input,d);
    lv_indev_set_read_cb(input,read_pointer); lv_timer_set_period(lv_indev_get_read_timer(input),16);
    wristflow_ui_init("");
    wristflow_settings_t initial=wristflow_settings_default(); initial.brightness=27;
    wristflow_watch_snapshot_t state=wristflow_product_snapshot(false,0);
    shell=wristflow_product_ui_create(&state,&initial,set_brightness,NULL); advance(240);
    wristflow_ui_shell_bind_vibration_preview(shell,preview,cancel_preview,&preview_id);
    assert(!previews && !cancellations);
    key(); open(WRISTFLOW_SURFACE_SETTINGS); snapshot("root");
    lv_obj_t *root=lv_screen_active(),*list=named(root,"settings_scroll");
    assert(lv_obj_get_child_count(list)==10);
    press("settings_faces"); snapshot("faces"); surface(WRISTFLOW_SURFACE_FACE_MANAGEMENT);
    press("row_switch"); assert(!settings().face_long_press);
    home(); sample(195,200,true); advance(900); sample(195,200,false); advance(240); surface(WRISTFLOW_SURFACE_HOME);
    key(); open(WRISTFLOW_SURFACE_SETTINGS); press("settings_faces"); press("face_change"); surface(WRISTFLOW_SURFACE_FACE_PICKER);
    back(); surface(WRISTFLOW_SURFACE_FACE_MANAGEMENT);
    press("face_change"); swipe(310,210,70,210); tap(195,210); surface(WRISTFLOW_SURFACE_HOME);
    assert(!strcmp(settings().face_id,"simple"));
    key(); open(WRISTFLOW_SURFACE_SETTINGS); root=lv_screen_active(); list=named(root,"settings_scroll");
    swipe(195,355,195,140); assert(lv_obj_get_scroll_y(list)>0); snapshot("root_middle");
    press("settings_about"); snapshot("about");
    lv_obj_scroll_to_y(named(lv_screen_active(),"settings_scroll"),1000,LV_ANIM_OFF); advance(32); snapshot("about_bottom");
    back(); snapshot("root_bottom");
    const char *entries[]={"settings_keys","settings_system"};
    const char *disabled[]={"assistant_disabled","reset_disabled"};
    for(unsigned i=0;i<2;++i) {
        press(entries[i]); assert(lv_obj_has_state(named(lv_screen_active(),disabled[i]),LV_STATE_DISABLED)); snapshot(entries[i]); back();
    }
    press("settings_sound"); snapshot("sound");
    assert(lv_obj_has_state(named(lv_screen_active(),"sound_disabled"),LV_STATE_DISABLED));
    press("vibration_mode"); surface(WRISTFLOW_SURFACE_VIBRATION_SETTINGS); snapshot("vibration_standard");
    assert(!previews && !lv_obj_has_flag(named(named(lv_screen_active(),"vibration_1"),"row_check"),LV_OBJ_FLAG_HIDDEN));
    press("vibration_1"); assert(!previews); /* Same value and ordinary page refresh never preview. */
    press("vibration_0"); assert(!previews && settings().vibration_mode==WRISTFLOW_VIBRATION_OFF);
    snapshot("vibration_off");
    press("vibration_1"); assert(previews==1 && preview_mode==WRISTFLOW_VIBRATION_STANDARD);
    press("vibration_2"); assert(previews==2 && cancellations==1 && preview_mode==WRISTFLOW_VIBRATION_ENHANCED);
    snapshot("vibration_enhanced");
    lv_obj_scroll_to_y(named(lv_screen_active(),"settings_scroll"),1000,LV_ANIM_OFF);
    advance(32); snapshot("vibration_bottom");
    back(); assert(cancellations==2); surface(WRISTFLOW_SURFACE_SOUND_SETTINGS);
    assert(!strcmp(lv_label_get_text(named(named(lv_screen_active(),"vibration_mode"),"row_detail")),"增强"));
    press("vibration_mode"); assert(previews==2); press("vibration_0");
    reject_preview=true; press("vibration_1"); assert(previews==3 && cancellations==2);
    advance(1000); assert(previews==3); /* Rejected busy preview is not queued/retried. */
    reject_preview=false; press("vibration_2"); assert(previews==4);
    press("vibration_0"); assert(cancellations==3 && previews==4); /* Global OFF cancels owned request. */
    back(); back();
    press("settings_dnd"); press("dnd_mode_1"); assert(settings().dnd_mode==WRISTFLOW_DND_ALL_DAY); snapshot("dnd_on");
    press("dnd_mode_2"); assert(settings().dnd_mode==WRISTFLOW_DND_SCHEDULED && !wristflow_ui_shell_dnd(shell));
    snapshot("dnd_waiting");
    state=wristflow_product_snapshot(true,WRISTFLOW_TIME_MIN+15*3600); /* local23:00 */
    wristflow_ui_shell_update(shell,&state); assert(wristflow_ui_shell_dnd(shell));
    press("dnd_end"); surface(WRISTFLOW_SURFACE_DND_END);
    lv_roller_set_selected(named(lv_screen_active(),"time_hours"),22,LV_ANIM_OFF);
    lv_roller_set_selected(named(lv_screen_active(),"time_minutes"),0,LV_ANIM_OFF);
    press("time_accept"); surface(WRISTFLOW_SURFACE_DND_END);
    assert(!lv_obj_has_flag(named(lv_screen_active(),"dnd_time_error"),LV_OBJ_FLAG_HIDDEN));
    assert(settings().dnd_end_minute==480); snapshot("dnd_equal_rejected");
    lv_roller_set_selected(named(lv_screen_active(),"time_hours"),8,LV_ANIM_OFF);
    press("time_accept"); surface(WRISTFLOW_SURFACE_DND_SETTINGS);
    press("dnd_start"); surface(WRISTFLOW_SURFACE_DND_START); snapshot("dnd_time");
    lv_roller_set_selected(named(lv_screen_active(),"time_hours"),21,LV_ANIM_OFF);
    back(); assert(settings().dnd_start_minute==1320); /* Return cancels the wheel draft. */
    snapshot("dnd_scheduled"); press("dnd_mode_1"); back();
    press("settings_notifications"); snapshot("notifications"); back();
    press("settings_display"); snapshot("display");
    assert(!lv_obj_find_by_name(lv_screen_active(),"display_tap"));
    press("display_wrist"); snapshot("wrist");
    assert(!settings().wrist_wake);
    press("row_switch"); assert(settings().wrist_wake); snapshot("wrist_on");
    press("wrist_row"); assert(!settings().wrist_wake);
    press("wrist_row"); assert(settings().wrist_wake); back();
    assert(!strcmp(lv_label_get_text(named(named(lv_screen_active(),"display_wrist"),"row_detail")),"开启"));
    press("display_brightness"); snapshot("brightness");
    assert(lv_obj_has_state(named(lv_screen_active(),"auto_brightness"),LV_STATE_DISABLED));
    press("custom_brightness"); snapshot("adjust27");
    lv_obj_t *slider=named(lv_screen_active(),"settings_brightness"); assert(lv_slider_get_value(slider)==27 && lv_obj_get_height(slider)>lv_obj_get_width(slider));
    swipe(195,330,195,220); assert(settings().brightness>27 && brightness==settings().brightness); snapshot("adjust_drag");
    back(); back(); press("display_timeout"); snapshot("timeout");
    press("choice_60"); snapshot("timeout_confirm"); assert(settings().screen_timeout==10);
    press("confirm_cancel"); assert(settings().screen_timeout==10);
    press("choice_60"); press("confirm_accept"); assert(settings().screen_timeout==60);
    press("choice_5"); assert(settings().screen_timeout==5); snapshot("timeout5");
    back(); press("display_keep"); snapshot("keep");
    press("choice_10"); snapshot("keep_confirm"); press("confirm_cancel"); assert(!wristflow_ui_shell_keep_minutes(shell));
    press("choice_10"); press("confirm_accept"); assert(wristflow_ui_shell_keep_minutes(shell)==10); snapshot("keep10");
    /* Controls start directly; settings retain their separate confirmation. */
    home(); assert(wristflow_ui_shell_open_controls(shell)); advance(240);
    lv_obj_t *controls=lv_screen_active();
    const char *control_buttons[]={"dnd_button","silent_button","wrist_button","settings_button",
        "flashlight_button","phone_button","drain_button","endurance_button","keep_awake_button"};
    for(unsigned i=0;i<9;++i) {
        lv_obj_t *image=lv_obj_get_child(named(controls,control_buttons[i]),0);
        assert(lv_obj_check_type(image,&lv_image_class));
        assert(lv_image_get_src_width(image)==64 && lv_image_get_src_height(image)==64);
    }
    snapshot("controls_grid");
    state.battery_unavailable=false; state.battery_percent=100; state.power.estimate_valid=true;
    wristflow_ui_shell_update(shell,&state); advance(32);
    assert(!strcmp(lv_label_get_text(named(controls,"control_battery_value")),"~100%"));
    snapshot("controls_battery100");
    state.battery_unavailable=true; state.power.estimate_valid=false;
    wristflow_ui_shell_update(shell,&state); advance(32);
    press("keep_awake_button"); assert(!wristflow_ui_shell_keep_minutes(shell));
    press("keep_awake_button"); assert(wristflow_ui_shell_keep_minutes(shell)==5);
    assert(!lv_obj_find_by_name(controls,"settings_confirm")); snapshot("control_active");
    assert(!strcmp(lv_label_get_text(named(controls,"toast_text")),"持续亮屏5分钟"));
    press("dnd_button"); assert(settings().dnd_mode==WRISTFLOW_DND_OFF);
    press("silent_button"); assert(settings().silent_mode && settings().vibration_mode==WRISTFLOW_VIBRATION_OFF);
    assert(!strcmp(lv_label_get_text(named(controls,"toast_text")),"静音模式已开启"));
    snapshot("control_silent"); advance(1000);
    press("wrist_button"); assert(!settings().wrist_wake);
    advance(1000); assert(!lv_obj_has_flag(named(controls,"control_toast"),LV_OBJ_FLAG_HIDDEN));
    advance(1000); assert(lv_obj_has_flag(named(controls,"control_toast"),LV_OBJ_FLAG_HIDDEN));
    press("silent_button"); assert(!settings().silent_mode);
    lv_obj_t *control_reminder=lv_obj_create(lv_layer_top());
    assert(wristflow_ui_shell_present_reminder(shell,control_reminder));
    assert(lv_obj_has_flag(named(controls,"control_toast"),LV_OBJ_FLAG_HIDDEN));
    wristflow_ui_shell_close_reminder(shell);
    assert(lv_obj_has_flag(named(controls,"control_toast"),LV_OBJ_FLAG_HIDDEN));
    press("silent_button"); assert(settings().silent_mode);
    hold("wrist_button"); surface(WRISTFLOW_SURFACE_WAKE_SETTINGS);
    assert(!settings().wrist_wake); /* A long press never performs the short toggle. */
    press("wrist_row"); assert(settings().wrist_wake); back(); surface(WRISTFLOW_SURFACE_CONTROLS);
    hold("silent_button"); surface(WRISTFLOW_SURFACE_SOUND_SETTINGS); assert(settings().silent_mode); back();
    hold("dnd_button"); surface(WRISTFLOW_SURFACE_DND_SETTINGS); assert(settings().dnd_mode==WRISTFLOW_DND_OFF); back();
    hold("keep_awake_button"); surface(WRISTFLOW_SURFACE_KEEP_AWAKE);
    press("choice_10"); assert(wristflow_ui_shell_keep_minutes(shell)==5);
    assert(lv_obj_find_by_name(lv_screen_active(),"settings_confirm")); press("confirm_cancel"); back();
    press("phone_button"); press("drain_button"); surface(WRISTFLOW_SURFACE_CONTROLS);
    hold("endurance_button"); surface(WRISTFLOW_SURFACE_ENDURANCE_SETTINGS); snapshot("endurance_pending"); back();
    press("endurance_button"); snapshot("endurance_confirm"); press("confirm_accept");
    assert(!strcmp(lv_label_get_text(named(controls,"toast_text")),"长续航模式待实现"));
    key(); surface(WRISTFLOW_SURFACE_HOME);
    assert(lv_obj_has_flag(named(controls,"control_toast"),LV_OBJ_FLAG_HIDDEN));
    wristflow_ui_shell_enable_display_policy(shell);
    assert(wristflow_ui_shell_keep_awake(shell,0));
    home(); key(); open(WRISTFLOW_SURFACE_SETTINGS); press("settings_sound"); press("vibration_mode");
    press("vibration_1"); unsigned before_cancel=cancellations;
    jump(10050); assert(cancellations==before_cancel+1 && previews==5);
    key(); assert(previews==5); /* Wake/re-entry does not replay the selection. */
    home();
    /* Enabling the policy occurs once at startup; establish a fresh fixed countdown. */
    assert(wristflow_ui_shell_keep_awake(shell,5));
    jump(299000); tap(20,20); jump(1000);
    assert(!wristflow_ui_shell_keep_minutes(shell) && wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_ACTIVE);
    home(); key(); open(WRISTFLOW_SURFACE_SETTINGS);
    wristflow_settings_t chosen=settings(); chosen.brightness=27; chosen.screen_timeout=5;
    assert(wristflow_ui_shell_configure(shell,&chosen));
    lv_obj_t *wake_page=lv_screen_active();
    jump(1000);
    assert(!wristflow_ui_shell_wrist_wake(shell)); /* ACTIVE motion does not reset timeout. */
    jump(3050); assert(brightness==5 && wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_DIM);
    assert(wristflow_ui_shell_wrist_wake(shell) && brightness==27 && lv_screen_active()==wake_page);
    jump(5050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    assert(wristflow_ui_shell_wrist_wake(shell) && lv_screen_active()==wake_page);
    chosen.wrist_wake=false; assert(wristflow_ui_shell_configure(shell,&chosen));
    jump(5050); assert(!wristflow_ui_shell_wrist_wake(shell));
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    chosen.wrist_wake=true; assert(wristflow_ui_shell_configure(shell,&chosen));
    jump(5050);
    assert(wristflow_ui_shell_wrist_wake(shell));
    jump(4050);
    /* A dim touch over an actionable row consumes down, drag, and release. */
    sample(195,125,true); assert(brightness==27); sample(195,130,true); sample(195,125,false); advance(240); surface(WRISTFLOW_SURFACE_SETTINGS);
    press("settings_faces"); surface(WRISTFLOW_SURFACE_FACE_MANAGEMENT);
    jump(4050); key(); surface(WRISTFLOW_SURFACE_FACE_MANAGEMENT); assert(brightness==27);
    jump(5050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF); tap(195,125);
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    jump(119000); key(); surface(WRISTFLOW_SURFACE_FACE_MANAGEMENT);
    jump(5050); jump(120000); check_wake_home=true; key(); check_wake_home=false; surface(WRISTFLOW_SURFACE_HOME);
    key(); open(WRISTFLOW_SURFACE_FLASHLIGHT); jump(600000); assert(brightness==100 && wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_ACTIVE);
    key(); assert(brightness==27); jump(3000); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_ACTIVE);
    key(); open(WRISTFLOW_SURFACE_STOPWATCH); press("stopwatch_toggle"); jump(5050); jump(120000); key(); surface(WRISTFLOW_SURFACE_STOPWATCH);
    assert(strcmp(lv_label_get_text(named(lv_screen_active(),"stopwatch_time")),"00:00")); snapshot("stopwatch_wake");
    press("stopwatch_toggle");
    char paused[32]; snprintf(paused,sizeof paused,"%s",lv_label_get_text(named(lv_screen_active(),"stopwatch_time")));
    jump(5050); jump(120000); key(); surface(WRISTFLOW_SURFACE_STOPWATCH);
    assert(!strcmp(paused,lv_label_get_text(named(lv_screen_active(),"stopwatch_time"))));
    /* Paused data, running data, and all exits obey the same confirmation. */
    press("app_back"); snapshot("stopwatch_exit"); surface(WRISTFLOW_SURFACE_STOPWATCH);
    jump(5050); assert(wristflow_ui_shell_wrist_wake(shell));
    assert(lv_obj_find_by_name(lv_screen_active(),"stopwatch_exit_confirm"));
    press("confirm_cancel"); surface(WRISTFLOW_SURFACE_STOPWATCH);
    swipe(10,220,280,220); assert(lv_obj_find_by_name(lv_screen_active(),"stopwatch_exit_confirm"));
    key(); assert(!lv_obj_find_by_name(lv_screen_active(),"stopwatch_exit_confirm")); surface(WRISTFLOW_SURFACE_STOPWATCH);
    key(); press("confirm_accept"); surface(WRISTFLOW_SURFACE_HOME);
    key(); open(WRISTFLOW_SURFACE_STOPWATCH);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"stopwatch_time")),"00:00"));
    press("stopwatch_toggle"); advance(1200);
    press("app_back"); press("confirm_cancel"); advance(1200);
    assert(strcmp(lv_label_get_text(named(lv_screen_active(),"stopwatch_time")),"00:00"));
    press("app_back"); press("confirm_accept"); surface(WRISTFLOW_SURFACE_LAUNCHER);
    open(WRISTFLOW_SURFACE_STOPWATCH);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"stopwatch_time")),"00:00"));
    press("stopwatch_toggle");
    open(WRISTFLOW_SURFACE_SETTINGS); surface(WRISTFLOW_SURFACE_STOPWATCH); press("confirm_accept"); surface(WRISTFLOW_SURFACE_SETTINGS);
    home();
    /* A populated but uncommitted page survives arbitrary sleep without requesting save. */
    swipe(320,125,60,125); sample(100,110,true); advance(736); sample(100,110,false); advance(240); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    press("edit_right"); press("full"); press("slot_0"); press("choose_activity_0"); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    lv_obj_t *draft=lv_screen_active(); assert(wristflow_ui_shell_layout(shell)->count==3);
    jump(5050); jump(600000); assert(wristflow_ui_shell_wrist_wake(shell)); assert(lv_screen_active()==draft); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    assert(!lv_obj_has_state(named(draft,"edit_action"),LV_STATE_DISABLED)); assert(wristflow_ui_shell_layout(shell)->count==3); snapshot("draft_wake");
    uint8_t record[WRISTFLOW_SETTINGS_BYTES]; chosen=settings(); assert(wristflow_settings_encode(&chosen,record));
    assert(wristflow_ui_shell_keep_awake(shell,20)); wristflow_ui_shell_destroy(shell); shell=NULL;
    assert(wristflow_settings_decode(&initial,record,sizeof record));
    shell=wristflow_product_ui_create(&state,&initial,set_brightness,NULL); advance(240);
    assert(!wristflow_ui_shell_keep_minutes(shell) && !settings().face_long_press && settings().screen_timeout==5 && brightness==27);
    assert(settings().vibration_mode==WRISTFLOW_VIBRATION_STANDARD && previews==5);
    assert(settings().wrist_wake && settings().silent_mode);
    /* Same UI with a real asynchronous contract: never infer persistence from
     * changing an in-memory toggle or from re-entering the screen. */
    wristflow_ui_shell_bind_settings_store(shell,request_settings,settings_status,NULL);
    store_bound=true;
    key(); open(WRISTFLOW_SURFACE_SETTINGS);
    assert(!store_requests && lv_obj_has_flag(named(lv_screen_active(),"settings_save_note"),LV_OBJ_FLAG_HIDDEN));
    press("settings_faces"); press("row_switch");
    assert(store_requests==1 && submitted.face_long_press);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"settings_save_note")),"保存中")); snapshot("save_pending");
    store_state=WRISTFLOW_SAVE_DONE; advance(240);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"settings_save_note")),"已保存")); snapshot("save_done");
    unsigned queries=store_queries;
    advance(1000); assert(store_queries==queries && store_requests==1);
    back(); press("settings_display"); press("display_timeout"); press("choice_10");
    assert(store_requests==2 && submitted.screen_timeout==10);
    store_state=WRISTFLOW_SAVE_FAILED; advance(240);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"settings_save_note")),"保存失败，点击重试")); snapshot("save_failed");
    queries=store_queries; advance(1000); assert(store_queries==queries && store_requests==2);
    back(); back();
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"settings_save_note")),"保存失败，点击重试"));
    press("settings_save_note"); assert(store_requests==3);
    store_state=WRISTFLOW_SAVE_DONE; advance(240);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"settings_save_note")),"已保存"));
    reject_request=true; press("settings_dnd"); press("dnd_mode_1");
    assert(store_requests==4 && wristflow_ui_shell_settings_save_state(shell)==WRISTFLOW_SAVE_FAILED);
    reject_request=false; press("settings_save_note"); assert(store_requests==5);
    /* A newer value supersedes the pending UI request; status is queried only
     * for the most recently accepted ID. */
    chosen=settings(); chosen.brightness=42;
    assert(wristflow_ui_shell_configure(shell,&chosen)); advance(32);
    assert(store_requests==6 && submitted.brightness==42);
    assert(wristflow_ui_shell_set_breathing_minutes(shell,3)); advance(32);
    assert(store_requests==7 && submitted.breathing_minutes==3);
    store_state=WRISTFLOW_SAVE_DONE; advance(240);
    assert(wristflow_ui_shell_keep_awake(shell,5)); advance(32);
    assert(store_requests==7); /* Temporary display policy is not persisted. */
    back(); press("settings_layout"); press("layout_grid");
    assert(store_requests==8 && submitted.menu_layout==WRISTFLOW_MENU_GRID);
    home();
    assert(wristflow_ui_shell_set_watchface(shell,&wristflow_default_watchface)); advance(240);
    assert(store_requests==9 && !strcmp(submitted.face_id,"diffusion"));
    key(); open(WRISTFLOW_SURFACE_SETTINGS); press("settings_sound"); press("vibration_mode");
    press("vibration_2");
    assert(store_requests==10 && submitted.vibration_mode==WRISTFLOW_VIBRATION_ENHANCED);
    assert(previews==5); /* Unbound backend still persists the real preference without fake feedback. */
    store_state=WRISTFLOW_SAVE_DONE; advance(240);
    store_bound=false;
    wristflow_ui_shell_bind_vibration_preview(shell,preview,cancel_preview,&preview_id);
    before_cancel=cancellations;
    press("vibration_1"); assert(previews==6);
    chosen=settings(); chosen.vibration_mode=WRISTFLOW_VIBRATION_ENHANCED;
    assert(wristflow_ui_shell_configure(shell,&chosen)); advance(32);
    assert(previews==6 && cancellations==before_cancel+1); /* Restore/configure only cancels. */
    press("vibration_1"); assert(previews==7);
    lv_obj_t *reminder=lv_obj_create(lv_layer_top());
    wristflow_ui_shell_enable_display_policy(shell); /* This recreated shell has its own policy. */
    assert(wristflow_ui_shell_present_reminder(shell,reminder));
    assert(cancellations==before_cancel+2 && !wristflow_ui_shell_set_vibration_mode(shell,WRISTFLOW_VIBRATION_ENHANCED));
    jump(30050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    assert(wristflow_ui_shell_wrist_wake(shell) && wristflow_ui_shell_reminder_active(shell) && brightness);
    wristflow_ui_shell_close_reminder(shell); advance(240); assert(previews==7);
    wristflow_ui_shell_bind_reminder_vibration(shell,preview,cancel_preview,&preview_id);
    assert(previews==7); /* Binding adds capability text without starting an occurrence. */
    home();key();open(WRISTFLOW_SURFACE_SETTINGS);press("settings_sound");snapshot("sound_reminder");
    press("vibration_mode");snapshot("vibration_reminder");
    lv_obj_t *mode_list=named(lv_screen_active(),"settings_scroll");
    lv_obj_scroll_to_y(mode_list,LV_COORD_MAX,LV_ANIM_OFF);advance(32);snapshot("vibration_reminder_bottom");
    lv_obj_scroll_to_y(mode_list,0,LV_ANIM_OFF);advance(32);
    press("vibration_2"); assert(previews==8);
    back(); back(); press("settings_display"); press("display_wrist");
    store_bound=true; unsigned saves=store_requests;
    press("row_switch"); assert(!settings().wrist_wake && store_requests==saves+1 && !submitted.wrist_wake);
    store_state=WRISTFLOW_SAVE_DONE; advance(240); store_bound=false;
    wristflow_ui_shell_destroy(shell); shell=NULL;
    assert(cancellations==before_cancel+3); lv_deinit(); return 0;
}
