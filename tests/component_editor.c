#include "product_ui.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks, requests;
static uint8_t pixels[390 * 40 * 4], saved[WRISTFLOW_LAYOUT_BYTES];
static wristflow_save_state_t save_state;
static lv_indev_data_t pointer;
static wristflow_ui_shell_t *shell;
static const char *renders;
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) { (void)a; (void)p; lv_display_flush_ready(d); }
static void read_pointer(lv_indev_t *i, lv_indev_data_t *d) { (void)i; *d = pointer; }
static uint32_t request(const wristflow_layout_t *layout, void *context)
{
    (void)context; ++requests; save_state = WRISTFLOW_SAVE_PENDING;
    assert(wristflow_layout_encode(layout, requests, saved)); return requests;
}
static wristflow_save_state_t status(uint32_t id, void *context)
{ (void)context; assert(id == requests); return save_state; }
static void advance(unsigned ms)
{ for (unsigned i = 0; i < ms; i += 16) { ticks += 16; lv_timer_handler(); } lv_obj_update_layout(lv_screen_active()); }
static void sample(int x, int y, bool down)
{ pointer.point = (lv_point_t){x,y}; pointer.state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED; advance(16); }
static void tap(int x, int y)
{ sample(x,y,true); sample(x,y,false); advance(480); }
static lv_obj_t *named(lv_obj_t *root, const char *name)
{ lv_obj_t *obj = lv_obj_find_by_name(root, name); assert(obj); return obj; }
static void click(lv_obj_t *obj)
{
    lv_area_t a; lv_obj_get_coords(obj, &a); int x = (a.x1+a.x2)/2, y = (a.y1+a.y2)/2;
    assert(x >= 0 && x < 390 && y >= 0 && y < 450);
    sample(x,y,true); sample(x,y,false); advance(480);
}
static void press(const char *name) { click(named(lv_screen_active(), name)); }
static void swipe(int x1, int y1, int x2, int y2)
{
    sample(x1,y1,true);
    for (int i = 1; i <= 12; ++i) sample(x1+(x2-x1)*i/12,y1+(y2-y1)*i/12,true);
    sample(x2,y2,false); advance(800);
}
static void surface(wristflow_surface_t value)
{
    wristflow_surface_t actual = wristflow_ui_shell_navigation(shell)->surface;
    if (actual != value) fprintf(stderr, "surface expected=%d actual=%d pointer=%d,%d\n",
        value, actual, pointer.point.x, pointer.point.y);
    assert(actual == value);
}
static void dots(void)
{
    lv_obj_t *root=lv_screen_active();
    unsigned visible=0, selected=wristflow_ui_shell_navigation(shell)->page_index;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i) {
        lv_obj_t *obj=lv_obj_get_child(root,i);
        if(obj==named(root,"demo_carousel") || lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) continue;
        ++visible; assert(lv_obj_get_child_count(obj)==wristflow_ui_shell_layout(shell)->count);
        for(unsigned j=0;j<lv_obj_get_child_count(obj);++j)
            assert(lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_child(obj,j),0),
                lv_color_hex(j+1==selected ? 0xffffff : 0x334155)));
    }
    assert(visible==1);
}
static void snapshot(const char *name)
{
    lv_draw_buf_t *b = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888); assert(b);
    char path[1024]; snprintf(path,sizeof path,"%s/%s.ppm",renders,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n390 450\n255\n");
    for(unsigned y=0;y<450;++y) for(unsigned x=0;x<390;++x) {
        const uint8_t *p=b->data+y*b->header.stride+x*3; uint8_t rgb[]={p[2],p[1],p[0]}; fwrite(rgb,1,3,f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
static void enter(void)
{
    surface(WRISTFLOW_SURFACE_HOME);
    sample(100,110,true); advance(736); sample(100,110,false); advance(480);
    surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
}
static void select(const char *name)
{
    lv_obj_t *obj=named(lv_screen_active(),name);
    lv_obj_scroll_to_view(obj, LV_ANIM_OFF); advance(32); click(obj);
    surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
}
static void confirm(bool accept)
{ click(named(lv_layer_top(), accept ? "confirm_accept" : "confirm_cancel")); }
static void begin(const char *side, const char *type)
{
    press(side); surface(WRISTFLOW_SURFACE_COMPONENT_TEMPLATES);
    press(type); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
}
static void fill(wristflow_template_t type)
{
    unsigned before=requests;
    for(unsigned s=0;s<wristflow_template_slots(type);++s) {
        char name[16]; snprintf(name,sizeof name,"slot_%u",s); press(name);
        surface(WRISTFLOW_SURFACE_COMPONENT_PICKER);
        if(wristflow_template_size(type,s)==WRISTFLOW_CARD_QUARTER) {
            assert(!lv_obj_find_by_name(lv_screen_active(),"choose_weather_0"));
            select("choose_heart_rate_1");
        } else {
            assert(!lv_obj_find_by_name(lv_screen_active(),"choose_heart_rate_0"));
            select("choose_activity_0");
        }
        assert(requests==before);
    }
    assert(!lv_obj_has_state(named(lv_screen_active(),"edit_action"),LV_STATE_DISABLED));
}

int main(int argc,char **argv)
{
    assert(argc==2); renders=argv[1]; lv_init(); lv_tick_set_cb(tick);
    lv_display_t *d=lv_display_create(390,450);
    lv_display_set_buffers(d,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL); lv_display_set_flush_cb(d,flush);
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input,d); lv_indev_set_read_cb(input,read_pointer); lv_timer_set_period(lv_indev_get_read_timer(input),16);
    wristflow_ui_init("");
    wristflow_settings_t settings=wristflow_settings_default();
    wristflow_watch_snapshot_t state=wristflow_product_snapshot(false,0);
    shell=wristflow_product_ui_create_with_layout(&state,&settings,NULL,NULL,NULL,request,status,NULL); advance(480);
    swipe(320,125,60,125);
    /* Blank long press and movement beyond 10 px do not enter the editor. */
    sample(195,405,true); advance(800); sample(195,405,false); advance(480); surface(WRISTFLOW_SURFACE_HOME);
    sample(100,110,true); sample(100,122,true); advance(800); sample(100,122,false); advance(480); surface(WRISTFLOW_SURFACE_HOME);
    sample(100,110,true); advance(608); surface(WRISTFLOW_SURFACE_HOME); sample(100,110,false); advance(480);
    enter(); snapshot("component_editor");
    tap(5,164); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    swipe(320,140,60,140); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    assert(wristflow_ui_shell_navigation(shell)->page_index==1);
    press("slot_1"); surface(WRISTFLOW_SURFACE_COMPONENT_PICKER); snapshot("component_quarter_picker");
    swipe(10,200,185,200); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    press("slot_1"); surface(WRISTFLOW_SURFACE_COMPONENT_PICKER);
    select("choose_heart_rate_0"); assert(requests==1);
    const wristflow_layout_t *layout=wristflow_ui_shell_layout(shell);
    assert(!strcmp(layout->pages[0].slots[1].app_id,"heart_rate"));
    assert(layout->pages[0].slots[1].instance_id!=layout->pages[0].slots[0].instance_id);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"edit_save")),"保存中"));
    save_state=WRISTFLOW_SAVE_FAILED; advance(240);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"edit_save")),"保存失败，稍后重试"));
    save_state=WRISTFLOW_SAVE_DONE; advance(240);
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"edit_save")),"已保存"));
    /* Existing edits exit without a dialog; a tap routes the replacement. */
    tap(195,75); surface(WRISTFLOW_SURFACE_HOME);
    sample(280,110,true); sample(280,110,false); advance(480); surface(WRISTFLOW_SURFACE_HEART);
    assert(wristflow_ui_shell_back(shell)); advance(480); enter();
    /* Incomplete and complete new pages remain drafts until the check mark. */
    tap(4,175); surface(WRISTFLOW_SURFACE_COMPONENT_TEMPLATES);
    press("full"); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    assert(lv_obj_has_state(named(lv_screen_active(),"edit_action"),LV_STATE_DISABLED));
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"edit_save")),""));
    snapshot("component_new_empty");
    sample(195,75,true); sample(195,75,false); advance(480);
    assert(!lv_obj_has_flag(lv_obj_get_parent(named(lv_layer_top(),"confirm_accept")),LV_OBJ_FLAG_HIDDEN));
    confirm(false); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    fill(WRISTFLOW_LAYOUT_FULL); snapshot("component_new_full");
    assert(!strcmp(lv_label_get_text(named(lv_screen_active(),"edit_save")),""));
    assert(requests==1 && layout->count==3);
    assert(wristflow_ui_shell_key(shell)); advance(480); confirm(false); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    assert(wristflow_ui_shell_key(shell)); advance(480); confirm(true); surface(WRISTFLOW_SURFACE_HOME);
    assert(wristflow_ui_shell_navigation(shell)->page_index==0 && layout->count==3 && requests==1);
    swipe(320,125,60,125); enter();
    tap(385,269); surface(WRISTFLOW_SURFACE_COMPONENT_TEMPLATES);
    press("full"); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    fill(WRISTFLOW_LAYOUT_FULL);
    press("edit_action"); surface(WRISTFLOW_SURFACE_HOME);
    assert(layout->count==4 && requests==2 && wristflow_ui_shell_navigation(shell)->page_index==2);
    dots();
    assert(layout->pages[1].template_id==WRISTFLOW_LAYOUT_FULL); snapshot("component_full_page");
    enter(); tap(246,418); confirm(false); surface(WRISTFLOW_SURFACE_COMPONENT_EDITOR);
    assert(layout->count==4 && requests==2);
    press("edit_action"); confirm(true); surface(WRISTFLOW_SURFACE_HOME);
    assert(layout->count==3 && requests==3 && wristflow_ui_shell_navigation(shell)->page_index==1);
    dots();
    const char *types[]={"quarters","top_half","bottom_half"};
    for(unsigned t=0;t<3;++t) {
        enter(); begin(t%2 ? "edit_right" : "edit_left",types[t]); fill((wristflow_template_t)t);
        unsigned before=requests; press("edit_action"); surface(WRISTFLOW_SURFACE_HOME);
        assert(layout->count==4+t && requests==before+1 && wristflow_layout_valid(layout));
        dots();
    }
    enter(); assert(lv_obj_has_state(named(lv_screen_active(),"edit_left"),LV_STATE_DISABLED));
    assert(lv_obj_has_state(named(lv_screen_active(),"edit_right"),LV_STATE_DISABLED)); snapshot("component_six_pages");
    while(layout->count>1) {
        press("edit_action"); confirm(true); surface(WRISTFLOW_SURFACE_HOME); dots(); enter();
    }
    assert(lv_obj_has_state(named(lv_screen_active(),"edit_action"),LV_STATE_DISABLED));
    save_state=WRISTFLOW_SAVE_DONE; advance(240); press("app_back");
    assert(wristflow_ui_shell_navigation(shell)->page_index==1);
    wristflow_layout_t reboot; uint32_t generation;
    assert(wristflow_layout_decode(&reboot,&generation,saved,sizeof saved) && reboot.count==1);
    wristflow_ui_shell_destroy(shell);
    shell=wristflow_product_ui_create_with_layout(&state,&settings,NULL,NULL,&reboot,request,status,NULL); advance(480);
    assert(wristflow_ui_shell_layout(shell)->count==1 && wristflow_ui_shell_navigation(shell)->page_count==2);
    swipe(320,125,60,125); snapshot("component_restored");
    wristflow_ui_shell_destroy(shell); lv_deinit(); return 0;
}
