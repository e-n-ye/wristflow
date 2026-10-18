/* Compile the actual adapter here to seed end-state fixtures without a product test API. */
#define wristflow_2048_view_create actual_view_create
#include "../ui/runtime/game_2048_view.c"
#undef wristflow_2048_view_create
#include "app_registry.h"
#include <assert.h>
#include <string.h>

static uint32_t ticks;
static lv_indev_data_t pointer;
static uint8_t pixels[390*40*4];
static wristflow_ui_shell_t *filter_shell;
static const char *directory;
static wristflow_2048_view_t *game_owner;
/* Observe the normal apps-owned instance, without changing the product API. */
wristflow_2048_view_t *wristflow_2048_view_create(wristflow_ui_shell_t *shell)
{ return game_owner=actual_view_create(shell); }
static uint32_t tick(void) { return ticks; }
static void read_pointer(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input; *data = pointer;
    if (filter_shell && !wristflow_ui_shell_filter_touch(filter_shell, data->state == LV_INDEV_STATE_PRESSED))
        data->state = LV_INDEV_STATE_RELEASED;
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void advance(unsigned duration)
{
    for (unsigned i=0; i<duration; i+=16) { ticks+=16; lv_timer_handler(); }
    lv_obj_update_layout(lv_screen_active());
}
static void sample(int x,int y,bool down)
{ pointer.point=(lv_point_t){x,y}; pointer.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; advance(16); }
static lv_obj_t *object(const char *name)
{ lv_obj_t *o=lv_obj_find_by_name(lv_screen_active(),name); assert(o); return o; }
static void press(lv_obj_t *target)
{
    lv_area_t a; lv_obj_get_coords(target,&a);
    int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    assert(x>=0 && x<390 && y>=0 && y<450);
    sample(x,y,true); sample(x,y,false); advance(400);
}
static void click(const char *name) { press(object(name)); }
static void swipe(int x1,int y1,int x2,int y2)
{
    sample(x1,y1,true);
    for (int i=1;i<=12;++i) sample(x1+(x2-x1)*i/12,y1+(y2-y1)*i/12,true);
    sample(x2,y2,false); advance(400);
}
static void board(char text[16][16])
{
    memset(text,0,16*16);
    for(unsigned i=0;i<16;++i) {
        char name[24]; snprintf(name,sizeof name,"game_tile_%u",i);
        snprintf(text[i],16,"%s",lv_label_get_text(lv_obj_find_by_name(object(name),"game_number")));
    }
}
static void snapshot(const char *name)
{
    lv_draw_buf_t *buf=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888);
    assert(buf && buf->header.w==390 && buf->header.h==450);
    char path[1024]; snprintf(path,sizeof path,"%s/%s.ppm",directory,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n390 450\n255\n");
    for(unsigned y=0;y<450;++y) for(unsigned x=0;x<390;++x) {
        const unsigned char *p=buf->data+y*buf->header.stride+3*x;
        unsigned char rgb[]={p[2],p[1],p[0]}; assert(fwrite(rgb,1,3,f)==3);
    }
    assert(fclose(f)==0); lv_draw_buf_destroy(buf);
}
static unsigned timers(void)
{ unsigned n=0; for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t)) ++n; return n; }
static void glyphs(const char *string)
{
    const unsigned char *c=(const unsigned char *)string;
    while(*c) {
        uint32_t cp=*c++;
        if(cp>=0xe0) { cp=((cp&15)<<12)|((c[0]&63)<<6)|(c[1]&63); c+=2; }
        else if(cp>=0xc0) { cp=((cp&31)<<6)|(*c++&63); }
        lv_font_glyph_dsc_t d;
        assert(lv_font_get_glyph_dsc(notification_22,&d,cp,0) && !d.is_placeholder);
    }
}
static void open_game(wristflow_ui_shell_t *shell)
{
    if(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME) {
        assert(wristflow_ui_shell_key(shell)); advance(400);
    }
    assert(wristflow_ui_shell_open(shell,WRISTFLOW_SURFACE_GAME_2048)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_GAME_2048);
}
int main(int argc,char **argv)
{
    assert(argc==2); directory=argv[1];
    lv_init(); lv_tick_set_cb(tick);
    lv_display_t *display=lv_display_create(390,450);
    lv_display_set_buffers(display,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush); wristflow_ui_init("");
    glyphs("得分最高重来重新开始？本局进度将清空达成！继续挑战更大的数字游戏结束没有可以合并方块了返回");
    lv_font_glyph_dsc_t icon;
    assert(lv_font_get_glyph_dsc(icons_44,&icon,0xf00a,0) && !icon.is_placeholder);
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input,display); lv_indev_set_read_cb(input,read_pointer);
    lv_timer_set_period(lv_indev_get_read_timer(input),16);
    const wristflow_app_descriptor_t *app=wristflow_app_find("game_2048");
    assert(app && app->surface==WRISTFLOW_SURFACE_GAME_2048 && !app->demo_only && !app->card_sizes);
    unsigned baseline=timers();
    wristflow_settings_t settings=wristflow_settings_default(); settings.screen_timeout=10;
    wristflow_ui_shell_config_t config={.watchface=&wristflow_default_watchface,.initial_settings=&settings,
        .controls=screen_control_center_create,.initial_snapshot={.hour_24=15,.minute=30,.battery_percent=53},
        .enable_apps=true,.product_apps=true};
    wristflow_ui_shell_t *shell=wristflow_ui_shell_create(&config); assert(shell); advance(400);
    unsigned shell_timers=timers();
    assert(wristflow_ui_shell_key(shell)); advance(400);
    lv_obj_scroll_to_view(object("launch_game_2048"),LV_ANIM_OFF); advance(64);
    snapshot("2048-menu-list"); click("launch_game_2048");
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_GAME_2048);
    assert(timers()==shell_timers); /* Game creates no periodic timer. */
    char before[16][16],after[16][16]; board(before); snapshot("2048-start");
    swipe(260,48,110,48); board(after); assert(!memcmp(before,after,sizeof before));
    const int swipes[4][4]={{280,250,95,250},{110,250,290,250},{195,350,195,165},{195,160,195,350}};
    for(unsigned i=0;i<4;++i) {
        game_owner->game=(wristflow_2048_t){.random=111};
        game_owner->game.cells[0]=1;
        game_owner->game.cells[i<2?1:4]=1;
        wristflow_2048_t expected=game_owner->game;
        assert(wristflow_2048_move(&expected,(wristflow_2048_direction_t)i,NULL));
        render(game_owner); advance(160);
        swipe(swipes[i][0],swipes[i][1],swipes[i][2],swipes[i][3]);
        assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_GAME_2048);
        assert(!memcmp(expected.cells,game_owner->game.cells,16));
        assert(expected.score==game_owner->game.score && expected.random==game_owner->game.random);
    }
    board(after); assert(memcmp(before,after,sizeof before));
    board(before); snapshot("2048-play");
    click("game_restart"); assert(lv_obj_is_visible(object("game_overlay")));
    snapshot("2048-restart"); click("game_cancel"); board(after);
    assert(!memcmp(before,after,sizeof before));
    click("game_restart"); assert(wristflow_ui_shell_back(shell)); advance(400);
    assert(!lv_obj_is_visible(object("game_overlay")));
    lv_obj_t *reminder=lv_obj_create(lv_layer_top());
    assert(wristflow_ui_shell_present_reminder(shell,reminder));
    swipe(260,250,100,250); wristflow_ui_shell_close_reminder(shell); advance(400);
    board(after); assert(!memcmp(before,after,sizeof before)); lv_obj_delete(reminder);
    swipe(15,240,180,240); assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_LAUNCHER);
    open_game(shell); board(after); assert(!memcmp(before,after,sizeof before));
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);
    open_game(shell); board(after); assert(!memcmp(before,after,sizeof before));
    settings.menu_layout=WRISTFLOW_MENU_GRID; assert(wristflow_ui_shell_configure(shell,&settings));
    assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_key(shell)); advance(400);
    lv_obj_t *grid_entry=lv_obj_find_by_name(object("launcher_grid"),"launch_game_2048"); assert(grid_entry);
    lv_obj_scroll_to_view(grid_entry,LV_ANIM_OFF); advance(64); snapshot("2048-menu-grid"); press(grid_entry);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_GAME_2048);
    board(after); assert(!memcmp(before,after,sizeof before));
    wristflow_ui_shell_enable_display_policy(shell); filter_shell=shell;
    advance(6000); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_DIM);
    swipe(260,250,100,250); board(after); assert(!memcmp(before,after,sizeof before));
    advance(16000); assert(wristflow_ui_shell_display_phase(shell)==WRISTFLOW_DISPLAY_OFF);
    assert(wristflow_ui_shell_key(shell)); advance(400); board(after); assert(!memcmp(before,after,sizeof before));
    advance(140000); assert(wristflow_ui_shell_key(shell)); advance(400);
    assert(wristflow_ui_shell_navigation(shell)->surface==WRISTFLOW_SURFACE_HOME);
    open_game(shell); board(after); assert(!memcmp(before,after,sizeof before));
    filter_shell=NULL; wristflow_ui_shell_destroy(shell); advance(400); assert(timers()==baseline);

    /* End-state and typography fixtures use the actual adapter, not replacement rendering. */
    wristflow_2048_view_t *v=wristflow_2048_view_create(NULL);
    lv_obj_t *root=wristflow_2048_view_screen(v); lv_screen_load(root);
    v->game=(wristflow_2048_t){.cells={1,2,3,4,5,6,7,8,9,10,11,15,16,20,30,31},.score=UINT32_MAX,.best=UINT32_MAX,.random=12};
    render(v); advance(200); snapshot("2048-values");
    const char *scores[]={"game_score","game_best"};
    for(unsigned i=0;i<2;++i) {
        lv_point_t size; lv_obj_t *label=object(scores[i]);
        lv_text_get_size(&size,lv_label_get_text(label),notification_22,0,0,1000,LV_TEXT_FLAG_NONE);
        assert(size.x<=lv_obj_get_width(label) && size.y<=lv_obj_get_height(label));
    }
    for(unsigned i=0;i<16;++i) {
        char name[24]; snprintf(name,sizeof name,"game_tile_%u",i);
        lv_obj_t *label=lv_obj_find_by_name(object(name),"game_number");
        assert(lv_obj_get_width(label)<=74 && lv_obj_get_height(label)<=74);
        lv_point_t size;
        lv_text_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),0,0,1000,LV_TEXT_FLAG_NONE);
        assert(size.x<=74 && size.y<=74);
    }
    v->game.won=true; render(v); advance(100); snapshot("2048-win");
    click("game_cancel"); assert(v->game.continued && !lv_obj_is_visible(object("game_overlay")));
    v->game.over=true; render(v); advance(100); snapshot("2048-over");
    click("game_confirm"); assert(!v->game.over && !v->game.score && v->game.best==UINT32_MAX);
    lv_obj_t *blank=lv_obj_create(NULL); lv_screen_load(blank);
    wristflow_2048_view_detach(v); lv_obj_delete(root); wristflow_2048_view_destroy(v);
    assert(timers()==baseline);
    lv_indev_delete(input); lv_display_delete(display); lv_deinit();
    puts("2048 UI: menu, real four-way pointer input, restart, edge/Home, retention, reminders, DIM/sleep, end-state renders");
    return 0;
}
