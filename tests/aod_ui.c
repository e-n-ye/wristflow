#include "product_ui.h"
#include "aod_view.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks, requests;
static uint8_t pixels[390*40*4];
static lv_indev_data_t pointer;
static wristflow_ui_shell_t *shell;
static wristflow_settings_t submitted;
static const char *renders;
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{ (void)a;(void)p;lv_display_flush_ready(d); }
static void light(uint8_t value,void *context) { (void)value;(void)context; }
static void read_pointer(lv_indev_t *input,lv_indev_data_t *data)
{ (void)input;*data=pointer; }
static uint32_t save(const wristflow_settings_t *settings,void *context)
{ (void)context;submitted=*settings;return ++requests; }
static wristflow_save_state_t status(uint32_t id,void *context)
{ (void)context;assert(id<=requests);return WRISTFLOW_SAVE_DONE; }
static void advance(unsigned ms)
{
    for(unsigned i=0;i<ms;i+=16) { ticks+=16;lv_timer_handler();if(shell) wristflow_ui_shell_sync_settings_save(shell); }
    lv_obj_update_layout(lv_screen_active());
}
static lv_obj_t *named(lv_obj_t *root,const char *name)
{ lv_obj_t *obj=lv_obj_find_by_name(root,name);if(!obj)fprintf(stderr,"missing %s\n",name);assert(obj);return obj; }
static wristflow_settings_t settings(void)
{ wristflow_settings_t result;assert(wristflow_ui_shell_get_settings(shell,&result));return result; }
static void press(const char *name)
{
    lv_obj_t *obj=named(lv_screen_active(),name);
    lv_obj_scroll_to_view(obj,LV_ANIM_OFF);advance(32);
    lv_area_t area;lv_obj_get_coords(obj,&area);
    pointer.point=(lv_point_t){(area.x1+area.x2)/2,(area.y1+area.y2)/2};
    assert(pointer.point.x>=0 && pointer.point.x<390 && pointer.point.y>=0 && pointer.point.y<450);
    pointer.state=LV_INDEV_STATE_PRESSED;advance(32);
    pointer.state=LV_INDEV_STATE_RELEASED;advance(240);
}
static void back(void) { assert(wristflow_ui_shell_back(shell));advance(240); }
static void open(wristflow_surface_t surface) { assert(wristflow_ui_shell_open(shell,surface));advance(240); }
static void glyphs(lv_obj_t *obj)
{
    if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return;
    if(lv_obj_check_type(obj,&lv_label_class)) {
        const unsigned char *p=(const unsigned char *)lv_label_get_text(obj);
        const lv_font_t *font=lv_obj_get_style_text_font(obj,0);
        while(*p) {
            uint32_t cp=*p++;
            if(cp>=0xe0){cp=((cp&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;}
            else if(cp>=0xc0)cp=((cp&31)<<6)|(*p++&63);
            if(cp=='\n')continue;
            lv_font_glyph_dsc_t glyph;
            if(!lv_font_get_glyph_dsc(font,&glyph,cp,0)||glyph.is_placeholder) {
                fprintf(stderr,"missing glyph U+%04x in %s\n",cp,lv_label_get_text(obj));assert(false);
            }
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(obj);++i)glyphs(lv_obj_get_child(obj,i));
}
static void snapshot(const char *name)
{
    glyphs(lv_screen_active());
    lv_draw_buf_t *b=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888);assert(b);
    char path[1024];snprintf(path,sizeof path,"%s/aod_%s.ppm",renders,name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n390 450\n255\n");
    for(unsigned y=0;y<450;++y)for(unsigned x=0;x<390;++x){
        const uint8_t *p=b->data+y*b->header.stride+x*3;uint8_t rgb[]={p[2],p[1],p[0]};assert(fwrite(rgb,1,3,f)==3);
    }
    fclose(f);lv_draw_buf_destroy(b);
}
int main(int argc,char **argv)
{
    assert(argc==2);renders=argv[1];lv_init();lv_tick_set_cb(tick);
    lv_display_t *d=lv_display_create(390,450);lv_display_set_buffers(d,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
    lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_display(input,d);
    lv_indev_set_read_cb(input,read_pointer);lv_timer_set_period(lv_indev_get_read_timer(input),16);
    wristflow_ui_init("");
    wristflow_settings_t prefs=wristflow_settings_default();prefs.wrist_wake=true;
    wristflow_watch_snapshot_t clock=wristflow_product_snapshot(true,1792718880U);
    shell=wristflow_product_ui_create(&clock,&prefs,light,NULL);assert(shell);advance(240);
    wristflow_ui_shell_bind_settings_store(shell,save,status,NULL);
    assert(wristflow_ui_shell_key(shell));advance(240);open(WRISTFLOW_SURFACE_SETTINGS);
    press("settings_display");press("display_aod");snapshot("off");
    assert(lv_obj_has_state(named(lv_screen_active(),"aod_smart"),LV_STATE_DISABLED));
    unsigned before=requests;
    press("aod_scheduled");snapshot("power_confirmation");
    assert(settings().aod_mode==WRISTFLOW_AOD_OFF && settings().wrist_wake && requests==before);
    press("confirm_cancel");assert(requests==before);
    press("aod_scheduled");press("confirm_accept");snapshot("wrist_confirmation");
    assert(settings().aod_mode==WRISTFLOW_AOD_OFF && settings().wrist_wake && requests==before);
    press("choice_back");assert(settings().aod_mode==WRISTFLOW_AOD_OFF && requests==before);
    press("aod_scheduled");press("confirm_accept");press("choice_first");
    assert(settings().aod_mode==WRISTFLOW_AOD_SCHEDULED && !settings().wrist_wake);
    assert(submitted.aod_mode==WRISTFLOW_AOD_SCHEDULED && !submitted.wrist_wake && requests==before+1);
    snapshot("scheduled");
    press("aod_start");lv_roller_set_selected(named(lv_screen_active(),"time_hours"),22,LV_ANIM_OFF);
    press("time_accept");snapshot("same_time_rejected");
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_AOD_START);
    assert(!lv_obj_has_flag(named(lv_screen_active(),"aod_time_error"),LV_OBJ_FLAG_HIDDEN));
    assert(settings().aod_start_minute==480);back();
    press("aod_start");lv_roller_set_selected(named(lv_screen_active(),"time_hours"),23,LV_ANIM_OFF);press("time_accept");
    assert(settings().aod_start_minute==1380);snapshot("overnight");
    press("aod_end");lv_roller_set_selected(named(lv_screen_active(),"time_hours"),7,LV_ANIM_OFF);back();
    assert(settings().aod_end_minute==1320);
    press("aod_info");snapshot("info");back();
    press("aod_off");prefs=settings();prefs.wrist_wake=true;assert(wristflow_ui_shell_configure(shell,&prefs));advance(32);
    press("aod_scheduled");press("confirm_accept");press("choice_second");
    assert(settings().aod_mode==WRISTFLOW_AOD_SCHEDULED && settings().wrist_wake);
    back();press("display_aod_styles");snapshot("styles_center");
    /* Actual pointer drag must browse without applying the unselected style. */
    pointer.point=(lv_point_t){300,230};pointer.state=LV_INDEV_STATE_PRESSED;advance(32);
    for(int x=280;x>=50;x-=20){pointer.point.x=x;advance(32);}
    pointer.state=LV_INDEV_STATE_RELEASED;advance(480);
    assert(lv_obj_get_scroll_x(named(lv_screen_active(),"aod_carousel"))==266);
    lv_area_t card;lv_obj_get_coords(named(lv_screen_active(),"aod_style_1"),&card);
    assert(card.x1==78 && card.x2==311);
    assert(settings().aod_style==WRISTFLOW_AOD_CENTER);snapshot("styles_corner");
    press("aod_style_1");assert(settings().aod_style==WRISTFLOW_AOD_CORNER);
    assert(submitted.aod_style==WRISTFLOW_AOD_CORNER);back();
    press("display_aod_styles");assert(lv_obj_get_scroll_x(named(lv_screen_active(),"aod_carousel"))==266);
    assert(strstr(lv_label_get_text(named(lv_screen_active(),"aod_style_name")),"已选"));
    uint8_t bytes[WRISTFLOW_SETTINGS_BYTES];prefs=settings();assert(wristflow_settings_encode(&prefs,bytes));
    wristflow_settings_t restored=wristflow_settings_default();assert(wristflow_settings_decode(&restored,bytes,sizeof bytes));
    assert(wristflow_settings_equal(&prefs,&restored));
    wristflow_ui_shell_destroy(shell);shell=NULL;
    lv_obj_t *root=screen_aod_create();lv_screen_load(root);
    lv_obj_t *content=named(root,"aod_display_content");
    clock.battery_unavailable=false;clock.battery_percent=83;clock.power.estimate_valid=true;
    wristflow_aod_render(content,&clock,WRISTFLOW_AOD_CENTER,0);advance(32);snapshot("resident_center");
    wristflow_aod_render(content,&clock,WRISTFLOW_AOD_CORNER,1);advance(32);snapshot("resident_corner");
    clock=wristflow_product_snapshot(true,WRISTFLOW_TIME_MAX);
    wristflow_aod_render(content,&clock,WRISTFLOW_AOD_CORNER,2);advance(32);snapshot("resident_2099");
    assert(!strcmp(lv_label_get_text(named(content,"aod_corner_date")),"1/1"));
    clock.time_unavailable=true;clock.battery_unavailable=true;
    wristflow_aod_render(content,&clock,WRISTFLOW_AOD_CENTER,3);advance(32);snapshot("resident_unknown");
    assert(!strcmp(lv_label_get_text(named(content,"aod_center_time")),"--:--"));
    wristflow_aod_render(content,&clock,WRISTFLOW_AOD_CORNER,0);advance(32);snapshot("corner_unknown");
    assert(!strcmp(lv_label_get_text(named(content,"aod_corner_battery")),"--%"));
    lv_deinit();return 0;
}
