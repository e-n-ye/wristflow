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
{ for (unsigned i = 0; i < ms; i += 16) { ticks += 16; lv_timer_handler(); } lv_obj_update_layout(lv_screen_active()); }
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
static void swipe(int x1,int y1,int x2,int y2)
{
    sample(x1,y1,true);
    for(int i=1;i<=12;++i) sample(x1+(x2-x1)*i/12,y1+(y2-y1)*i/12,true);
    sample(x2,y2,false); advance(480);
}
static void surface(wristflow_surface_t s) { assert(wristflow_ui_shell_navigation(shell)->surface==s); }
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
    const char *entries[]={"settings_sound","settings_dnd","settings_notifications","settings_keys","settings_system"};
    const char *disabled[]={"vibration_disabled","dnd_disabled","notifications_disabled","assistant_disabled","reset_disabled"};
    for(unsigned i=0;i<5;++i) {
        press(entries[i]); assert(lv_obj_has_state(named(lv_screen_active(),disabled[i]),LV_STATE_DISABLED)); snapshot(entries[i]); back();
    }
    press("settings_display"); snapshot("display");
    assert(!lv_obj_find_by_name(lv_screen_active(),"display_tap"));
    press("display_wrist"); snapshot("wrist"); back();
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
    /* Controls and settings use the same session state; pending proposals do not survive exit. */
    home(); assert(wristflow_ui_shell_open_controls(shell)); advance(240);
    press("keep_awake_button"); assert(!wristflow_ui_shell_keep_minutes(shell));
    press("keep_awake_button"); snapshot("control_confirm"); key(); surface(WRISTFLOW_SURFACE_HOME);
    assert(wristflow_ui_shell_open_controls(shell)); advance(240); assert(!lv_obj_find_by_name(lv_screen_active(),"settings_confirm"));
    press("keep_awake_button"); press("confirm_accept"); assert(wristflow_ui_shell_keep_minutes(shell)==5); snapshot("control_active");
    wristflow_ui_shell_enable_display_policy(shell);
    /* Enabling the policy occurs once at startup; establish a fresh fixed countdown. */
    assert(wristflow_ui_shell_keep_awake(shell,5));
    jump(299000); tap(20,20); jump(1000);
    assert(!wristflow_ui_shell_keep_minutes(shell) && wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_ACTIVE);
    home(); key(); open(WRISTFLOW_SURFACE_SETTINGS);
    wristflow_settings_t chosen=settings(); chosen.brightness=27; chosen.screen_timeout=5;
    assert(wristflow_ui_shell_configure(shell,&chosen));
    jump(4050); assert(brightness==5 && wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_DIM);
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
    jump(5050); jump(600000); key(); assert(lv_screen_active()==draft); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    assert(!lv_obj_has_state(named(draft,"edit_action"),LV_STATE_DISABLED)); assert(wristflow_ui_shell_layout(shell)->count==3); snapshot("draft_wake");
    uint8_t record[WRISTFLOW_SETTINGS_BYTES]; chosen=settings(); assert(wristflow_settings_encode(&chosen,record));
    assert(wristflow_ui_shell_keep_awake(shell,20)); wristflow_ui_shell_destroy(shell); shell=NULL;
    assert(wristflow_settings_decode(&initial,record,sizeof record));
    shell=wristflow_product_ui_create(&state,&initial,set_brightness,NULL); advance(240);
    assert(!wristflow_ui_shell_keep_minutes(shell) && !settings().face_long_press && settings().screen_timeout==5 && brightness==27);
    wristflow_ui_shell_destroy(shell); shell=NULL; lv_deinit(); return 0;
}
