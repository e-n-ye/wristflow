#include "product_ui.h"
#include "activity_view.h"
#include "daily_steps.h"
#include "app_registry.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static uint8_t pixels[390*40*4];
static lv_indev_data_t pointer;
static wristflow_ui_shell_t *filter_shell;
static const char *directory;
static uint32_t tick(void) { return ticks; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input; *data=pointer;
    if(filter_shell && !wristflow_ui_shell_filter_touch(filter_shell,
        data->state==LV_INDEV_STATE_PRESSED))
        data->state=LV_INDEV_STATE_RELEASED;
}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void advance(unsigned duration)
{ for(unsigned i=0;i<duration;i+=16) { ticks+=16; lv_timer_handler(); } lv_obj_update_layout(lv_screen_active()); }
static void sample(int x,int y,bool down)
{ pointer.point=(lv_point_t){x,y}; pointer.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; advance(16); }
static lv_obj_t *named(lv_obj_t *root,const char *name)
{ lv_obj_t *object=lv_obj_find_by_name(root,name); assert(object); return object; }
static void click(lv_obj_t *object)
{
    lv_area_t a; lv_obj_get_coords(object,&a); int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    assert(x>=0 && x<390 && y>=0 && y<450);
    sample(x,y,true); sample(x,y,false); advance(400);
}
static void swipe(int x1,int y1,int x2,int y2)
{
    sample(x1,y1,true);
    for(int i=1;i<=12;++i) sample(x1+(x2-x1)*i/12,y1+(y2-y1)*i/12,true);
    sample(x2,y2,false); advance(800);
}
static void text(lv_obj_t *root,const char *name,const char *expected)
{ assert(!strcmp(lv_label_get_text(named(root,name)),expected)); }
static void snapshot(const char *name)
{
    lv_draw_buf_t *buf=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888); assert(buf);
    char path[1024]; snprintf(path,sizeof path,"%s/%s.ppm",directory,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n390 450\n255\n");
    for(unsigned y=0;y<450;++y) for(unsigned x=0;x<390;++x) {
        const uint8_t *p=buf->data+y*buf->header.stride+3*x; uint8_t rgb[]={p[2],p[1],p[0]};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f)); lv_draw_buf_destroy(buf);
}
static void glyphs(lv_obj_t *object)
{
    if(lv_obj_check_type(object,&lv_label_class)) {
        const lv_font_t *font=lv_obj_get_style_text_font(object,0);
        const unsigned char *value=(const unsigned char *)lv_label_get_text(object);
        while(*value) {
            uint32_t cp=*value++;
            if(cp>=0xe0) { cp=((cp&15)<<12)|((value[0]&63)<<6)|(value[1]&63); value+=2; }
            else if(cp>=0xc0) { cp=((cp&31)<<6)|(*value++&63); }
            lv_font_glyph_dsc_t glyph;
            assert(lv_font_get_glyph_dsc(font,&glyph,cp,0) && !glyph.is_placeholder);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(object);++i) glyphs(lv_obj_get_child(object,i));
}
int main(int argc,char **argv)
{
    assert(argc==2); directory=argv[1];
    lv_init(); lv_tick_set_cb(tick); lv_display_t *display=lv_display_create(390,450);
    lv_display_set_buffers(display,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush); wristflow_ui_init("");
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input,display); lv_indev_set_read_cb(input,read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input),16);
    wristflow_watch_snapshot_t state=wristflow_product_snapshot(false,0);
    wristflow_settings_t settings=wristflow_settings_default();
    wristflow_layout_t layout=wristflow_layout_default();
    assert(wristflow_layout_replace(&layout,0,0,"calories",0));
    assert(wristflow_layout_replace(&layout,0,1,"active_minutes",1));
    wristflow_ui_shell_t *shell=wristflow_product_ui_create_with_layout(&state,&settings,NULL,NULL,&layout,NULL,NULL,NULL);
    assert(shell); advance(400);
    const char *const ids[]={"activity","steps","standing","calories","active_minutes"};
    for(unsigned i=0;i<5;++i) {
        const wristflow_app_descriptor_t *app=wristflow_app_find(ids[i]);
        assert(app && app->surface==WRISTFLOW_SURFACE_ACTIVITY && app->capability==WRISTFLOW_CAPABILITY_READY);
        wristflow_app_data_t data; assert(wristflow_app_read(app,&state,&data)); assert(!strcmp(data.value,"--"));
    }
    /* Every external variant reaches the same actual page at its overview. */
    lv_obj_t *home=lv_screen_active(), *carousel=named(home,"demo_carousel");
    for(unsigned page=0;page<2;++page) {
        swipe(320,125,60,125);
        lv_obj_t *panel=lv_obj_get_child(carousel,page+2);
        unsigned count=page?3:2;
        for(unsigned slot=0;slot<count;++slot) {
            char name[20]; snprintf(name,sizeof name,"slot_%u",slot); click(named(panel,name));
            assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_ACTIVITY);
            lv_obj_t *root=lv_screen_active(), *scroll=named(root,"activity_scroll");
            assert(lv_obj_get_scroll_y(scroll)==0 && lv_obj_get_scroll_snap_y(scroll)==LV_SCROLL_SNAP_NONE);
            assert(lv_obj_get_scroll_dir(scroll)==LV_DIR_VER);
            text(root,"overview_steps","--"); text(root,"activity_time","--:--"); glyphs(root);
            if(!page && !slot) snapshot("activity-empty-overview");
            assert(wristflow_ui_shell_back(shell)); advance(400); assert(lv_screen_active()==home);
        }
    }
    assert(wristflow_ui_shell_home(shell)); advance(400);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    lv_obj_t *launcher=lv_screen_active();
    assert(lv_obj_find_by_name(launcher,"launch_activity"));
    assert(!lv_obj_find_by_name(launcher,"launch_steps") && !lv_obj_find_by_name(launcher,"launch_calories"));
    assert(!lv_obj_find_by_name(launcher,"launch_standing") && !lv_obj_find_by_name(launcher,"launch_active_minutes"));
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_STEPS)); advance(400);
    lv_obj_t *root=lv_screen_active(), *scroll=named(root,"activity_scroll");
    /* Real pointer scroll can stop between cards; no full-screen paging. */
    swipe(195,365,195,185);
    for(unsigned elapsed=0;lv_obj_is_scrolling(scroll) && elapsed<4000;elapsed+=16) advance(16);
    assert(!lv_obj_is_scrolling(scroll));
    int offset=lv_obj_get_scroll_y(scroll); assert(offset>40);
    advance(600); assert(lv_obj_get_scroll_y(scroll)==offset);
    assert(offset!=450 && offset!=876 && offset!=1302 && offset!=1728);
    snapshot("activity-between-cards");
    swipe(105,210,310,210); assert(lv_screen_active()==root); /* Non-edge horizontal swipe stays. */
    /* Actual native-daily model output updates Product's detail and external
     * steps component and real hour bucket without unrelated metric/goal fixtures. */
    wristflow_daily_steps_t daily;
    wristflow_daily_steps_init(&daily);
    wristflow_steps_clock_t clock={.utc=1792411200,.monotonic_ms=1000,.valid=true};
    assert(wristflow_daily_steps_observe(&daily,0,clock));
    clock.utc+=30; clock.monotonic_ms+=30000;
    assert(wristflow_daily_steps_observe(&daily,48,clock));
    state.activity.metrics[WRISTFLOW_ACTIVITY_STEPS]=daily.snapshot.metric;
    state.activity.steps_observation=daily.snapshot.observation;
    assert(wristflow_ui_shell_update(shell,&state)); advance(32);
    text(root,"overview_steps","48"); text(root,"overview_calories","--"); text(root,"overview_minutes","--");
    text(named(root,"activity_steps"),"metric_value","48");
    text(named(root,"activity_steps"),"metric_goal","目标 -- 步");
    assert(lv_obj_has_flag(named(named(root,"activity_steps"),"chart_empty"),LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(named(named(root,"activity_steps"),"hour_20"),LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(named(named(root,"activity_steps"),"hour_19"),LV_OBJ_FLAG_HIDDEN));
    text(named(root,"activity_steps"),"chart_max","48");
    text(named(lv_obj_get_child(carousel,3),"slot_1"),"value_label","48");
    lv_obj_scroll_to_y(scroll,856,LV_ANIM_OFF); advance(32); snapshot("native-steps-live");
    /* A migrated total has no guessed columns. Its next measured interval
     * creates only a new partial bucket, while the total keeps the old48. */
    wristflow_steps_checkpoint_t legacy={.local_day=daily.snapshot.observation.local_day,
        .total=48,.coverage_start_utc=1792411200,.saved_utc=clock.utc};
    wristflow_daily_steps_init(&daily); assert(wristflow_daily_steps_seed(&daily,&legacy));
    assert(wristflow_daily_steps_observe(&daily,0,clock));
    state.activity.metrics[WRISTFLOW_ACTIVITY_STEPS]=daily.snapshot.metric;
    assert(wristflow_ui_shell_update(shell,&state));
    text(named(root,"activity_steps"),"metric_value","48");
    assert(!lv_obj_has_flag(named(named(root,"activity_steps"),"chart_empty"),LV_OBJ_FLAG_HIDDEN));
    ++clock.utc; clock.monotonic_ms+=1000;
    assert(wristflow_daily_steps_observe(&daily,5,clock));
    state.activity.metrics[WRISTFLOW_ACTIVITY_STEPS]=daily.snapshot.metric;
    assert(wristflow_ui_shell_update(shell,&state));
    text(named(root,"activity_steps"),"metric_value","53");
    text(named(root,"activity_steps"),"chart_max","5");
    assert(!lv_obj_has_flag(named(named(root,"activity_steps"),"hour_20"),LV_OBJ_FLAG_HIDDEN));
    advance(32); snapshot("restored-steps-hourly");
    wristflow_daily_steps_invalidate(&daily);
    state.activity.metrics[WRISTFLOW_ACTIVITY_STEPS]=daily.snapshot.metric;
    assert(wristflow_ui_shell_update(shell,&state)); text(root,"overview_steps","--");
    assert(!lv_obj_has_flag(named(named(root,"activity_steps"),"chart_empty"),LV_OBJ_FLAG_HIDDEN));
    lv_obj_scroll_to_y(scroll,offset,LV_ANIM_OFF); advance(32);
    /* Independent validity, real zero, incomplete hourly data and >100% progress. */
    state.time_unavailable=false; state.hour_24=18; state.minute=56;
    for(unsigned i=0;i<4;++i) {
        wristflow_activity_metric_t *metric=&state.activity.metrics[i];
        metric->total_valid=metric->goal_valid=true; metric->goal=i==1?6000:i==0?400:i==2?30:12;
        metric->total=i==1?4286:i==0?186:i==2?18:7;
        metric->hours_valid=(1U<<8)|(1U<<12)|(1U<<18);
        metric->hourly[8]=i==1?1280:2; metric->hourly[12]=i==1?1632:5; metric->hourly[18]=i==1?1374:1;
    }
    assert(wristflow_ui_shell_update(shell,&state)); advance(32); assert(lv_obj_get_scroll_y(scroll)==offset);
    lv_obj_scroll_to_y(scroll,0,LV_ANIM_OFF); advance(32); snapshot("activity-overview");
    const char *const cards[]={"activity_calories","activity_steps","activity_minutes","activity_standing"};
    for(unsigned i=0;i<4;++i) {
        lv_obj_t *card=named(root,cards[i]); lv_obj_scroll_to_y(scroll,lv_obj_get_y(card)-20,LV_ANIM_OFF); advance(32);
        assert(lv_obj_get_height(card)==410); assert(lv_obj_get_width(card)==342);
        assert(!lv_obj_has_flag(named(card,"hour_8"),LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_has_flag(named(card,"hour_9"),LV_OBJ_FLAG_HIDDEN));
        glyphs(card); char name[40]; snprintf(name,sizeof name,"activity-metric-%u",i); snapshot(name);
    }
    wristflow_activity_metric_t *steps=&state.activity.metrics[WRISTFLOW_ACTIVITY_STEPS];
    /* Total, target and hourly validity are independent, including a zero target. */
    steps->goal_valid=false;
    steps->hourly[9]=UINT32_MAX;
    assert(wristflow_ui_shell_update(shell,&state));
    lv_obj_t *steps_card=named(root,"activity_steps");
    text(steps_card,"metric_value","4286"); text(steps_card,"metric_goal","目标 -- 步");
    text(steps_card,"chart_max","1632");
    assert(lv_arc_get_value(named(root,"activity_ring_1"))==0);
    assert(lv_obj_has_flag(named(steps_card,"hour_9"),LV_OBJ_FLAG_HIDDEN));
    steps->goal_valid=true; steps->total_valid=false;
    assert(wristflow_ui_shell_update(shell,&state));
    text(steps_card,"metric_value","--"); text(steps_card,"metric_goal","目标 6000 步");
    assert(lv_arc_get_value(named(root,"activity_ring_1"))==0);
    steps->total_valid=true; steps->goal=0;
    assert(wristflow_ui_shell_update(shell,&state));
    text(steps_card,"metric_value","4286"); text(steps_card,"metric_goal","目标 -- 步");
    assert(lv_arc_get_value(named(root,"activity_ring_1"))==0);
    steps->total=0; steps->hours_valid=1; memset(steps->hourly,0,sizeof steps->hourly);
    assert(wristflow_ui_shell_update(shell,&state));
    text(steps_card,"metric_value","0"); text(steps_card,"chart_max","0");
    assert(lv_obj_has_flag(named(steps_card,"chart_empty"),LV_OBJ_FLAG_HIDDEN));
    steps->total=UINT32_MAX; steps->goal=1; steps->hourly[0]=UINT32_MAX;
    assert(wristflow_ui_shell_update(shell,&state));
    text(steps_card,"metric_value","4294967295"); assert(lv_arc_get_value(named(root,"activity_ring_1"))==100);
    lv_obj_scroll_to_y(scroll,0,LV_ANIM_OFF); advance(32); snapshot("activity-large-overview");
    assert(lv_obj_get_height(named(root,"overview_steps"))<=48);
    lv_obj_scroll_to_y(scroll,856,LV_ANIM_OFF); advance(32);
    lv_obj_t *maximum=named(steps_card,"chart_max"), *plot=named(steps_card,"metric_plot");
    assert(lv_obj_get_y(maximum)+lv_obj_get_height(maximum)<=lv_obj_get_y(plot));
    assert(lv_obj_get_x(maximum)+lv_obj_get_width(maximum)<=322);
    snapshot("activity-large-value");
    *steps=(wristflow_activity_metric_t){0}; assert(wristflow_ui_shell_update(shell,&state));
    text(steps_card,"metric_value","--"); text(steps_card,"chart_max","--");
    assert(!lv_obj_has_flag(named(steps_card,"chart_empty"),LV_OBJ_FLAG_HIDDEN));
    assert(lv_arc_get_value(named(root,"activity_ring_1"))==0);
    lv_obj_scroll_to_view(named(root,"activity_info"),LV_ANIM_OFF); advance(32);
    int saved=lv_obj_get_scroll_y(scroll); snapshot("activity-footer"); click(named(root,"activity_info"));
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_ACTIVITY_INFO);
    text(lv_screen_active(),"activity_time","18:56"); glyphs(lv_screen_active()); snapshot("activity-info-top");
    lv_obj_t *info=named(lv_screen_active(),"activity_info_scroll"); swipe(195,360,195,150);
    assert(lv_obj_get_scroll_y(info)>0); lv_obj_scroll_to_y(info,2000,LV_ANIM_OFF); advance(32); snapshot("activity-info-bottom");
    swipe(15,215,230,215); assert(lv_screen_active()==root); assert(lv_obj_get_scroll_y(scroll)==saved);
    swipe(15,215,230,215); assert(lv_screen_active()==launcher);
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_STANDING)); advance(400);
    assert(lv_obj_get_scroll_y(named(lv_screen_active(),"activity_scroll"))==0);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_ACTIVITY)); advance(400);
    root=lv_screen_active(); scroll=named(root,"activity_scroll");
    lv_obj_scroll_to_y(scroll,920,LV_ANIM_OFF); advance(32); offset=lv_obj_get_scroll_y(scroll);
    lv_obj_t *reminder=lv_obj_create(lv_layer_top());
    assert(wristflow_ui_shell_present_reminder(shell,reminder));
    swipe(195,360,195,150); wristflow_ui_shell_close_reminder(shell); advance(400);
    assert(lv_screen_active()==root && lv_obj_get_scroll_y(scroll)==offset); lv_obj_delete(reminder);
    settings.screen_timeout=5; assert(wristflow_ui_shell_configure(shell,&settings));
    wristflow_ui_shell_enable_display_policy(shell); filter_shell=shell;
    advance(4050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_DIM);
    swipe(195,360,195,150); assert(lv_obj_get_scroll_y(scroll)==offset);
    assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_ACTIVE);
    advance(5050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    advance(60000); assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(lv_screen_active()==root && lv_obj_get_scroll_y(scroll)==offset);
    advance(5050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    advance(120000); assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_ACTIVITY)); advance(400);
    assert(lv_obj_get_scroll_y(named(lv_screen_active(),"activity_scroll"))==0);
    filter_shell=NULL; wristflow_ui_shell_destroy(shell); advance(400); lv_display_delete(display);
    return 0;
}
