#include "product_ui.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static wristflow_ui_shell_t *shell;
static wristflow_notifications_snapshot_t phone;
static uint32_t ticks;
static unsigned brightness, deletes;
static uint8_t draw[390*40*3], frame[390*450*3];
static lv_indev_data_t pointer;
static const char *renders;
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{
    unsigned width = (unsigned)(a->x2-a->x1+1);
    for (int y=a->y1;y<=a->y2;++y) memcpy(frame+(y*390+a->x1)*3,p+(y-a->y1)*width*3,width*3);
    lv_display_flush_ready(d);
}
static void read_pointer(lv_indev_t *i, lv_indev_data_t *d)
{ (void)i; *d=pointer; if(!wristflow_ui_shell_filter_touch(shell,d->state==LV_INDEV_STATE_PRESSED)) d->state=LV_INDEV_STATE_RELEASED; }
static void set_brightness(uint8_t b, void *c) { (void)c; brightness=b; }
static void advance(unsigned ms)
{ for(unsigned i=0;i<ms;i+=16) { ticks+=16; lv_timer_handler(); } lv_obj_update_layout(lv_screen_active()); }
static void jump(unsigned ms) { ticks+=ms; lv_timer_handler(); lv_obj_update_layout(lv_screen_active()); }
static lv_obj_t *named(lv_obj_t *r,const char *n)
{ lv_obj_t *o=lv_obj_find_by_name(r,n); if(!o) fprintf(stderr,"missing %s\n",n); assert(o); return o; }
static void sample(int x,int y,bool down)
{ pointer.point=(lv_point_t){x,y}; pointer.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; advance(16); }
static void tap(int x,int y) { sample(x,y,true); sample(x,y,false); advance(240); }
static void press_in(lv_obj_t *root,const char *name)
{
    lv_obj_t *o=named(root,name); lv_obj_scroll_to_view(o,LV_ANIM_OFF); advance(32);
    lv_area_t a; lv_obj_get_coords(o,&a); int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    assert(x>=0&&x<390&&y>=0&&y<450); tap(x,y);
}
static void press(const char *name) { press_in(lv_screen_active(),name); }
static void swipe(int x1,int y1,int x2,int y2)
{
    sample(x1,y1,true);
    for(int i=1;i<=12;++i) sample(x1+(x2-x1)*i/12,y1+(y2-y1)*i/12,true);
    sample(x2,y2,false); advance(480);
}
static void surface(wristflow_surface_t s)
{
    wristflow_surface_t actual=wristflow_ui_shell_navigation(shell)->surface;
    if(actual!=s) fprintf(stderr,"surface at %u ms: expected %u, got %u\n",ticks,(unsigned)s,(unsigned)actual);
    assert(actual==s);
}
static void home(void) { assert(wristflow_ui_shell_home(shell)); advance(240); }
static void key(void) { assert(wristflow_ui_shell_key(shell)); advance(240); }
static void open(wristflow_surface_t s) { assert(wristflow_ui_shell_open(shell,s)); advance(240); }
static bool visible(const char *n)
{ lv_obj_t *o=lv_obj_find_by_name(lv_layer_top(),n); return o&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN); }
static void capture(const char *name)
{
    lv_obj_invalidate(lv_screen_active()); lv_obj_invalidate(lv_layer_top()); lv_refr_now(NULL);
    char path[1024]; snprintf(path,sizeof path,"%s/notify_%s.ppm",renders,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n390 450\n255\n");
    for(unsigned i=0;i<390*450;++i) { uint8_t rgb[]={frame[i*3+2],frame[i*3+1],frame[i*3]}; assert(fwrite(rgb,1,3,f)==3); }
    fclose(f);
}
static void remove_local(bool all,int32_t id,void *context)
{
    (void)context; ++deletes;
    if(all) phone.count=0;
    else for(unsigned i=0;i<phone.count;++i) if(phone.messages[i].id==id) {
        memmove(&phone.messages[i],&phone.messages[i+1],(--phone.count-i)*sizeof phone.messages[0]); break;
    }
    ++phone.revision; wristflow_ui_shell_notifications(shell,&phone);
}
static bool receive(int32_t id,const char *title,const char *body)
{
    unsigned at=phone.count;
    for(unsigned i=0;i<phone.count;++i) if(phone.messages[i].id==id) { at=i; break; }
    if(at==phone.count&&phone.count<WF_PHONE_MESSAGES) ++phone.count;
    if(at>=WF_PHONE_MESSAGES) at=WF_PHONE_MESSAGES-1;
    memmove(&phone.messages[1],&phone.messages[0],at*sizeof phone.messages[0]);
    phone.messages[0]=(wf_phone_message_t){.id=id}; strcpy(phone.messages[0].source,"QQ");
    strcpy(phone.messages[0].title,title); strcpy(phone.messages[0].body,body);
    phone.alert_id=id; ++phone.alert_sequence; ++phone.revision;
    bool wake=wristflow_ui_shell_notifications(shell,&phone); advance(32); return wake;
}
static void sleep_screen(void)
{ jump(10050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF); assert(brightness==0); }
int main(int argc,char **argv)
{
    assert(argc==2); renders=argv[1]; lv_init(); lv_tick_set_cb(tick);
    lv_display_t *d=lv_display_create(390,450); lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d,draw,NULL,sizeof draw,LV_DISPLAY_RENDER_MODE_PARTIAL); lv_display_set_flush_cb(d,flush);
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input,d); lv_indev_set_read_cb(input,read_pointer); lv_timer_set_period(lv_indev_get_read_timer(input),16);
    wristflow_ui_init(""); wristflow_settings_t settings=wristflow_settings_default();
    wristflow_watch_snapshot_t state=wristflow_product_snapshot(false,0);
    shell=wristflow_product_ui_create(&state,&settings,set_brightness,NULL);
    assert(notification_22->line_height<=34);
    const uint32_t glyphs[]={0x4e2d,0x9ad4,0x9f98,0x3400,0x3002,0xff1a,0x25a1};
    for(unsigned i=0;i<sizeof glyphs/sizeof glyphs[0];++i) {
        lv_font_glyph_dsc_t glyph; bool found=lv_font_get_glyph_dsc(notification_22,&glyph,glyphs[i],0);
        if(!found) fprintf(stderr,"missing exported glyph U+%04x\n",(unsigned)glyphs[i]);
        assert(found);
    }
    wristflow_ui_shell_bind_notification_delete(shell,remove_local,NULL);
    wristflow_ui_shell_enable_display_policy(shell); advance(240);
    phone.connected=true; phone.subscribed=true; wristflow_ui_shell_notifications(shell,&phone);
    assert(wristflow_ui_shell_phone_connected(shell));
    assert(!receive(1,"中文测试456","明天下午一起去图书馆，记得带上笔记本。"));
    assert(visible("notification_banner")); surface(WRISTFLOW_SURFACE_HOME); capture("banner");
    jump(2900); assert(visible("notification_banner")); jump(120); assert(!visible("notification_banner"));
    /* A DIM screen close to normal timeout still presents a full three seconds. */
    jump(6500); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_DIM);
    assert(!receive(1,"中文测试456","明天下午一起去图书馆，记得带上笔记本。"));
    assert(brightness==60); jump(2900); assert(visible("notification_banner")&&brightness==60);
    jump(120); assert(!visible("notification_banner"));
    swipe(195,100,195,310); surface(WRISTFLOW_SURFACE_NOTIFICATIONS); capture("list");
    press("notification_0"); surface(WRISTFLOW_SURFACE_NOTIFICATION_DETAIL); capture("detail");
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"message_title")),"中文测试456"));
    /* Same-ID update changes the open view; phone removal cannot leave stale text. */
    receive(1,"更新后的标题","第二条正文"); assert(phone.count==1);
    wristflow_ui_shell_notification_dismiss(shell,true);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"message_body")),"第二条正文"));
    phone.count=0; ++phone.revision; wristflow_ui_shell_notifications(shell,&phone);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"message_title")),"消息已移除")); capture("withdrawn");
    home(); sleep_screen(); jump(120000);
    assert(receive(2,"熄屏中文通知","预览五秒后恢复息屏")); assert(visible("notification_preview"));
    surface(WRISTFLOW_SURFACE_HOME); assert(brightness==60); capture("preview");
    jump(4850); assert(visible("notification_preview")); jump(180);
    assert(!visible("notification_preview")&&brightness==0);
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    assert(receive(3,"点击预览","应进入此条详情")); press_in(lv_layer_top(),"preview_open");
    surface(WRISTFLOW_SURFACE_NOTIFICATION_DETAIL);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"message_title")),"点击预览"));
    wristflow_ui_shell_back(shell); advance(240); surface(WRISTFLOW_SURFACE_NOTIFICATIONS);
    lv_obj_t *first_row=named(lv_screen_active(),"notification_0");
    swipe(320,155,100,155); surface(WRISTFLOW_SURFACE_NOTIFICATIONS);
    assert(lv_obj_get_scroll_x(first_row)>100&&deletes==0); capture("swipe_delete");
    swipe(100,155,320,155); assert(lv_obj_get_scroll_x(first_row)==0);
    swipe(320,155,100,155); tap(300,155);
    assert(deletes==1&&phone.count==1); surface(WRISTFLOW_SURFACE_NOTIFICATIONS);
    assert(lv_obj_get_scroll_x(first_row)==0); capture("after_delete");
    press("notification_clear"); assert(deletes==2&&phone.count==0); capture("empty");
    home(); assert(wristflow_ui_shell_open_controls(shell)); advance(240);
    press("dnd_button"); assert(wristflow_ui_shell_dnd(shell)); capture("controls");
    sleep_screen(); assert(!receive(4,"勿扰消息","继续入库，不亮屏")); assert(brightness==0&&!visible("notification_preview"));
    key(); assert(phone.count==1); receive(5,"勿扰亮屏","不显示顶部浮窗"); assert(!visible("notification_banner"));
    press("dnd_button"); assert(!wristflow_ui_shell_dnd(shell));
    home(); key(); open(WRISTFLOW_SURFACE_STOPWATCH); press("stopwatch_toggle"); advance(1100);
    lv_obj_t *stopwatch=lv_screen_active(); sleep_screen();
    assert(receive(6,"秒表期间","预览不停止秒表")); jump(5050); assert(lv_screen_active()==stopwatch);
    key(); surface(WRISTFLOW_SURFACE_STOPWATCH);
    assert(strcmp(lv_label_get_text(named(stopwatch,"stopwatch_time")),"00:00"));
    receive(7,"打开通知","退出秒表先确认"); press_in(lv_layer_top(),"notification_banner");
    surface(WRISTFLOW_SURFACE_STOPWATCH); assert(!visible("notification_banner")); capture("stopwatch_confirm");
    press("confirm_cancel"); assert(lv_screen_active()==stopwatch);
    receive(8,"打开通知","确认后清零"); press_in(lv_layer_top(),"notification_banner"); press("confirm_accept");
    surface(WRISTFLOW_SURFACE_NOTIFICATIONS);
    home(); key(); open(WRISTFLOW_SURFACE_STOPWATCH);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"stopwatch_time")),"00:00")); home();
    /* A populated, uncommitted component page survives preview timeout/cancel. */
    swipe(320,125,60,125); sample(100,110,true); advance(736); sample(100,110,false); advance(240);
    surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR); press("edit_right"); press("full"); press("slot_0"); press("choose_activity_0");
    lv_obj_t *draft=lv_screen_active(); sleep_screen(); jump(120000);
    receive(9,"草稿消息","不自动放弃草稿"); jump(5050); assert(lv_screen_active()==draft);
    receive(10,"草稿详情","取消仍保留草稿"); press_in(lv_layer_top(),"preview_open");
    capture("draft_confirm"); press_in(lv_layer_top(),"confirm_cancel"); assert(lv_screen_active()==draft);
    assert(wristflow_ui_shell_layout(shell)->count==3&&!lv_obj_has_state(named(draft,"edit_action"),LV_STATE_DISABLED));
    sleep_screen(); receive(11,"草稿目标","确认放弃后打开此条"); press_in(lv_layer_top(),"preview_open"); press_in(lv_layer_top(),"confirm_accept");
    surface(WRISTFLOW_SURFACE_NOTIFICATION_DETAIL);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"message_title")),"草稿目标"));
    assert(wristflow_ui_shell_layout(shell)->count==3);
    home(); key(); open(WRISTFLOW_SURFACE_SETTINGS); sleep_screen(); jump(120000);
    receive(12,"长睡后消息","超时不重置原来的息屏时间"); jump(5050); key(); surface(WRISTFLOW_SURFACE_HOME);
    sleep_screen(); receive(13,"上滑收起","收起后保持亮屏"); swipe(195,350,195,100);
    assert(!visible("notification_preview")&&brightness==60); surface(WRISTFLOW_SURFACE_HOME);
    home(); wristflow_ui_shell_set_dnd(shell,true);
    for(int i=20;i<32;++i) receive(i,"中文长标题：订单已送达，请及时查看详细信息","第一行内容\n第二行内容：消息支持换行与滚动。\n第三行内容\n第四行内容\n第五行内容\n第六行内容\n第七行内容\n第八行内容\n第九行内容\n第十行内容");
    assert(phone.count==10);
    wristflow_ui_shell_open_notification(shell,false,0); advance(240); capture("ten_messages");
    unsigned before_delete=deletes;
    swipe(195,360,195,100); assert(lv_obj_get_scroll_y(named(lv_screen_active(),"notification_list"))>0);
    assert(deletes==before_delete); surface(WRISTFLOW_SURFACE_NOTIFICATIONS); capture("list_scroll");
    lv_obj_scroll_to_y(named(lv_screen_active(),"notification_list"),0,LV_ANIM_OFF); advance(32);
    swipe(320,155,100,155); assert(lv_obj_get_scroll_x(first_row)>100);
    swipe(320,310,100,310); assert(lv_obj_get_scroll_x(first_row)==0);
    assert(lv_obj_get_scroll_x(named(lv_screen_active(),"notification_1"))>100);
    swipe(320,155,100,155); sample(300,155,true);
    receive(32,"重排期间","按下删除后新消息到达，松手不得删错消息");
    sample(300,155,false); advance(240);
    assert(deletes==before_delete&&phone.messages[0].id==32); surface(WRISTFLOW_SURFACE_NOTIFICATIONS);
    wristflow_ui_shell_open_notification(shell,true,31); advance(240); capture("long_detail");
    swipe(195,360,195,110); assert(lv_obj_get_scroll_y(named(lv_screen_active(),"notification_scroll"))>0); capture("detail_scroll");
    /* Late disconnect updates status without reopening an alert. */
    phone.connected=false; phone.subscribed=false; ++phone.revision; wristflow_ui_shell_notifications(shell,&phone);
    assert(!wristflow_ui_shell_phone_connected(shell));
    home(); key(); open(WRISTFLOW_SURFACE_SETTINGS); press("settings_notifications");
    surface(WRISTFLOW_SURFACE_NOTIFICATION_SETTINGS); lv_obj_t *settings_page=lv_screen_active();
    press("notifications_open"); surface(WRISTFLOW_SURFACE_NOTIFICATIONS);
    press("app_back"); surface(WRISTFLOW_SURFACE_NOTIFICATION_SETTINGS); assert(lv_screen_active()==settings_page);
    wristflow_ui_shell_destroy(shell); puts("notifications_ui: passed"); return 0;
}
