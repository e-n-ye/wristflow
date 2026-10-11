#include "ui_shell.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks;
static lv_indev_data_t pointer;
static uint8_t pixels[390 * 40 * 4];
static const char *directory;
static uint32_t tick(void) { return ticks; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data) { (void)input; *data = pointer; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void advance(unsigned ms)
{
    for (unsigned i = 0; i < ms; i += 16) { ticks += 16; lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active());
}
static void sample(int x, int y, bool pressed)
{
    pointer.point = (lv_point_t){x, y};
    pointer.state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    advance(16);
}
static lv_obj_t *named(const char *name)
{ lv_obj_t *obj = lv_obj_find_by_name(lv_screen_active(), name); if (!obj) fprintf(stderr,"missing UI object: %s\n",name); assert(obj); return obj; }
static void click_obj(lv_obj_t *obj)
{
    assert(lv_obj_is_visible(obj));
    lv_area_t a; lv_obj_get_coords(obj, &a);
    int x = (a.x1 + a.x2) / 2, y = (a.y1 + a.y2) / 2;
    assert(x >= 0 && x < 390 && y >= 0 && y < 450);
    sample(x, y, true); sample(x, y, false); advance(400);
}
static void click(const char *name)
{ lv_obj_scroll_to_view(named(name), LV_ANIM_OFF); advance(400); click_obj(named(name)); }
static lv_obj_t *row(unsigned index, const char *child)
{
    char name[32]; snprintf(name, sizeof name, "alarm_row_%u", index);
    lv_obj_t *parent = named(name);
    return child ? lv_obj_find_by_name(parent, child) : parent;
}
static void swipe(int x1, int y1, int x2, int y2)
{
    sample(x1, y1, true);
    for (int i = 1; i <= 12; ++i) sample(x1 + (x2-x1)*i/12, y1 + (y2-y1)*i/12, true);
    sample(x2, y2, false); advance(600);
}
static void snapshot(const char *name)
{
    lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    assert(buf && buf->header.w == 390 && buf->header.h == 450);
    char path[1024]; snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb"); assert(file); fprintf(file, "P6\n390 450\n255\n");
    for (unsigned y = 0; y < 450; ++y) {
        const unsigned char *p = buf->data + y * buf->header.stride;
        for (unsigned x = 0; x < 390; ++x) {
            unsigned char rgb[] = {p[3*x+2], p[3*x+1], p[3*x]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(fclose(file) == 0); lv_draw_buf_destroy(buf);
}
static void visible(const char *name, bool show)
{ bool actual=lv_obj_is_visible(named(name)); if (actual!=show) {
    lv_area_t a;lv_obj_get_coords(named(name),&a);
    fprintf(stderr,"UI visibility %s: expected %u, got %u; y=%ld..%ld scroll=%ld bottom=%ld\n",name,show,actual,(long)a.y1,(long)a.y2,(long)lv_obj_get_scroll_y(named("alarm_list")),(long)lv_obj_get_scroll_bottom(named("alarm_list")));
} assert(actual==show); }
static void wheel(unsigned h, unsigned m)
{
    lv_roller_set_selected(named("alarm_hours"), h, LV_ANIM_OFF);
    lv_roller_set_selected(named("alarm_minutes"), m, LV_ANIM_OFF); advance(32);
}
static void reveal(const char *name)
{ lv_obj_scroll_to_view(named(name), LV_ANIM_OFF); advance(400); }
static void back(wristflow_ui_shell_t *shell)
{ assert(wristflow_ui_shell_back(shell)); advance(400); }
static void open(wristflow_ui_shell_t *shell)
{
    if (wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME) {
        assert(wristflow_ui_shell_key(shell)); advance(400);
    }
    assert(wristflow_ui_shell_open(shell, WRISTFLOW_SURFACE_ALARM)); advance(400);
}
static unsigned timers(void)
{ unsigned n=0; for (lv_timer_t *t=lv_timer_get_next(NULL); t; t=lv_timer_get_next(t)) ++n; return n; }
static wristflow_ui_shell_t *create(bool product)
{
    wristflow_ui_shell_config_t config = {.watchface=&wristflow_default_watchface,
        .controls=screen_control_center_create, .initial_snapshot={.hour_24=13,.minute=40,.battery_percent=53},
        .enable_apps=true, .product_apps=product};
    wristflow_ui_shell_t *shell=wristflow_ui_shell_create(&config); assert(shell); advance(400); return shell;
}
static void text(lv_obj_t *label, const char *expected) { assert(!strcmp(lv_label_get_text(label), expected)); }

int main(int argc, char **argv)
{
    assert(argc == 2); directory=argv[1];
    lv_init(); lv_tick_set_cb(tick); lv_display_t *display=lv_display_create(390,450);
    lv_display_set_buffers(display,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush); wristflow_ui_init("");
    const uint32_t chinese[]={0x91cd,0x590d,0x5468,0x672b,0x5de5,0x4f5c,0x81f3,0xff08,0xff09,0x3001,0x4e09,0x56db,0x4e94,0x516d,0x5df2,0x8fbe,0x4e0a,0x9650,0x4e2a};
    for (unsigned i=0;i<sizeof chinese/sizeof chinese[0];++i) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(body_20,&glyph,chinese[i],0) && !glyph.is_placeholder);
        assert(lv_font_get_glyph_dsc(title_24,&glyph,chinese[i],0) && !glyph.is_placeholder);
    }
    const uint32_t icons[]={0xf067,0xf0f3,0xf1f8,0xf00c};
    for (unsigned i=0;i<4;++i) { lv_font_glyph_dsc_t glyph; assert(lv_font_get_glyph_dsc(icons_44,&glyph,icons[i],0) && !glyph.is_placeholder); }
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input,display); lv_indev_set_read_cb(input,read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input),16);
    unsigned baseline_timers=timers(); wristflow_ui_shell_t *shell=create(false);
    assert(wristflow_ui_shell_key(shell)); advance(400); assert(named("launch_alarm"));
    open(shell); visible("alarm_empty",true); text(named("alarm_clock"),"13:40"); snapshot("alarm_empty");
    click("alarm_add"); visible("alarm_time_panel",true);
    swipe(115,200,115,260); assert(lv_roller_get_selected(named("alarm_hours")) != 13);
    wheel(0,0); snapshot("alarm_time_min");
    lv_area_t a,b; lv_obj_get_coords(named("alarm_hours"),&a); lv_obj_get_coords(named("alarm_time_separator"),&b);
    assert(LV_ABS(a.y1+a.y2-b.y1-b.y2)<=2);
    assert(lv_obj_get_style_text_align(named("alarm_hours"),0)==LV_TEXT_ALIGN_CENTER);
    click("alarm_time_accept"); visible("alarm_delete",false); text(named("alarm_draft_time"),"00:00");
    click("alarm_edit_repeat"); snapshot("alarm_repeat"); click("alarm_repeat_2"); click("alarm_repeat_accept");
    text(named("alarm_draft_rule"),"工作日（周一至五）"); back(shell); visible("alarm_empty",true);
    click("alarm_add"); wheel(23,59); snapshot("alarm_time_max"); click("alarm_time_accept");
    click("alarm_save"); lv_obj_send_event(named("alarm_save"),LV_EVENT_SHORT_CLICKED,NULL);
    visible("alarm_row_1",false); text(row(0,"alarm_time"),"23:59");
    assert(lv_obj_has_state(row(0,"alarm_enabled"),LV_STATE_CHECKED));
    assert(lv_obj_get_style_bg_opa(row(0,"alarm_enabled"),LV_PART_INDICATOR)==LV_OPA_COVER);
    click_obj(row(0,"alarm_enabled")); assert(!lv_obj_has_state(row(0,"alarm_enabled"),LV_STATE_CHECKED));
    click_obj(row(0,"alarm_open")); click("alarm_edit_time"); wheel(0,0); click("alarm_time_accept");
    click("alarm_edit_repeat"); swipe(175,292,175,112);
    assert(lv_obj_get_scroll_y(named("alarm_repeat_scroll"))>0); reveal("alarm_repeat_4"); snapshot("alarm_repeat_more");
    click("alarm_repeat_4"); visible("alarm_weekdays_panel",true);
    assert(lv_obj_has_state(named("alarm_days_accept"),LV_STATE_DISABLED)); click("alarm_days_accept"); visible("alarm_weekdays_panel",true);
    click("alarm_day_0"); click("alarm_day_2"); snapshot("alarm_weekdays"); reveal("alarm_day_6"); click("alarm_day_6"); snapshot("alarm_weekdays_more");
    click("alarm_days_accept"); click("alarm_repeat_accept"); text(named("alarm_draft_rule"),"周一、三、日"); snapshot("alarm_settings");
    click("alarm_save"); text(row(0,"alarm_rule"),"周一、三、日"); assert(!lv_obj_has_state(row(0,"alarm_enabled"),LV_STATE_CHECKED)); snapshot("alarm_list");
    click_obj(row(0,"alarm_open")); click("alarm_edit_time"); wheel(17,42); back(shell); text(named("alarm_draft_time"),"00:00");
    click("alarm_edit_repeat"); click("alarm_repeat_1"); back(shell); text(named("alarm_draft_rule"),"周一、三、日");
    click("alarm_delete"); snapshot("alarm_delete_confirm");
    assert(!lv_obj_has_flag(named("alarm_delete_confirmation"),LV_OBJ_FLAG_GESTURE_BUBBLE));
    lv_obj_send_event(named("alarm_save"),LV_EVENT_SHORT_CLICKED,NULL); visible("alarm_delete_confirmation",true);
    swipe(15,90,260,90); visible("alarm_delete_confirmation",true);
    click("confirm_cancel"); visible("alarm_settings_panel",true); assert(!lv_obj_find_by_name(lv_screen_active(),"alarm_delete_confirmation"));
    click("alarm_delete"); back(shell); visible("alarm_settings_panel",true);
    click("alarm_delete"); click("confirm_accept"); visible("alarm_empty",true);
    const char *rules[]={"仅一次","每天","工作日（周一至五）","周末（周六、日）","仅一次"};
    for (unsigned i=0;i<5;++i) {
        click("alarm_add"); wheel(i,i); click("alarm_time_accept");
        if (i>0 && i<4) {
            click("alarm_edit_repeat"); char preset[32]; snprintf(preset,sizeof preset,"alarm_repeat_%u",i);
            reveal(preset); click(preset); click("alarm_repeat_accept");
        }
        click("alarm_save"); text(row(i,"alarm_rule"),rules[i]);
    }
    visible("alarm_add",false); lv_obj_scroll_to_y(named("alarm_list"),0,LV_ANIM_OFF); advance(32);
    visible("alarm_limit",false);
    text(named("alarm_limit"),"最多只能设置5个闹钟"); snapshot("alarm_full");
    lv_obj_send_event(named("alarm_add"),LV_EVENT_SHORT_CLICKED,NULL); visible("alarm_list_panel",true);
    lv_area_t limit_top,limit_bottom,last_card,viewport;
    lv_obj_get_coords(named("alarm_limit"),&limit_top); lv_obj_get_coords(named("alarm_list"),&viewport);
    assert(limit_top.y1>viewport.y2);
    swipe(180,390,180,110); assert(lv_obj_get_scroll_y(named("alarm_list"))>0); visible("alarm_limit",true);
    lv_obj_get_coords(named("alarm_limit"),&limit_bottom); lv_obj_get_coords(row(4,NULL),&last_card);
    assert(last_card.y2<limit_bottom.y1&&limit_bottom.y2<=viewport.y2);
    assert(limit_top.y1-limit_bottom.y1==lv_obj_get_scroll_y(named("alarm_list")));
    snapshot("alarm_full_more");
    /* Compacted rows must keep operating on the same committed record. */
    reveal("alarm_row_2"); click_obj(row(2,"alarm_open")); click("alarm_delete"); click("confirm_accept");
    text(row(2,"alarm_time"),"03:03"); text(row(2,"alarm_rule"),"周末（周六、日）");
    text(row(3,"alarm_time"),"04:04"); reveal("alarm_add"); visible("alarm_add",true); visible("alarm_limit",false);
    reveal("alarm_row_2"); click_obj(row(2,"alarm_enabled")); click_obj(row(2,"alarm_open"));
    click("alarm_edit_time"); wheel(6,30); click("alarm_time_accept"); click("alarm_save");
    text(row(2,"alarm_time"),"06:30"); assert(!lv_obj_has_state(row(2,"alarm_enabled"),LV_STATE_CHECKED));
    reveal("alarm_row_3"); click_obj(row(3,"alarm_open")); click("alarm_delete"); click("confirm_accept");
    for (unsigned i=3;i<5;++i) {
        click("alarm_add"); wheel(i+4,i+4); click("alarm_time_accept"); click("alarm_save");
    }
    visible("alarm_add",false); reveal("alarm_limit"); visible("alarm_limit",true);
    text(row(2,"alarm_time"),"06:30"); text(row(4,"alarm_time"),"08:08"); reveal("alarm_row_4");
    click_obj(row(4,"alarm_open")); click("alarm_edit_time"); wheel(22,22); click("alarm_time_accept");
    assert(wristflow_ui_shell_key(shell)); advance(400); open(shell);
    text(row(4,"alarm_time"),"08:08"); /* Screen collection discards draft, retains committed model. */
    visible("alarm_add",false); visible("alarm_limit",false);
    click_obj(row(0,"alarm_open"));
    wristflow_settings_t settings; assert(wristflow_ui_shell_get_settings(shell,&settings)); settings.screen_timeout=5;
    assert(wristflow_ui_shell_configure(shell,&settings)); wristflow_ui_shell_enable_display_policy(shell);
    click("alarm_edit_time"); wheel(17,42);
    advance(5050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF); advance(60000);
    assert(wristflow_ui_shell_key(shell)); advance(400); visible("alarm_time_panel",true);
    assert(lv_roller_get_selected(named("alarm_hours"))==17 && lv_roller_get_selected(named("alarm_minutes"))==42);
    back(shell); text(named("alarm_draft_time"),"00:00");
    click("alarm_edit_repeat"); reveal("alarm_repeat_4"); click("alarm_repeat_4"); click("alarm_day_0");
    advance(5050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF); advance(60000);
    assert(wristflow_ui_shell_key(shell)); advance(400); visible("alarm_weekdays_panel",true);
    assert(!lv_obj_has_flag(lv_obj_find_by_name(named("alarm_day_0"),"option_check"),LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_state(named("alarm_days_accept"),LV_STATE_DISABLED)); back(shell); back(shell);
    text(named("alarm_draft_rule"),"仅一次");
    advance(5050); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF); advance(120000);
    assert(wristflow_ui_shell_key(shell)); advance(400); visible("alarm_settings_panel",true);
    back(shell); advance(5050); advance(120000); assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);
    open(shell); reveal("alarm_row_4"); visible("alarm_row_4",true);
    wristflow_watch_snapshot_t clock={.time_unavailable=true}; assert(wristflow_ui_shell_update(shell,&clock)); text(named("alarm_clock"),"--:--");
    wristflow_ui_shell_destroy(shell); advance(400); assert(timers()==baseline_timers);
    shell=create(false); open(shell); visible("alarm_empty",true); wristflow_ui_shell_destroy(shell); advance(400);
    shell=create(true); assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(!wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_ALARM));
    assert(!lv_obj_find_by_name(lv_screen_active(),"launch_alarm"));
    wristflow_ui_shell_destroy(shell); advance(400); assert(timers()==baseline_timers);
    puts("alarm UI: real pointer, draft commits/cancel, repeat masks, confirmation, capacity, lifetime and Product gate passed");
    return 0;
}
