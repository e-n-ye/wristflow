#include "game_2048_view.h"
#include "game_2048.h"
#include "wristflow_ui.h"
#include <stdio.h>

struct wristflow_2048_view {
    wristflow_ui_shell_t *shell;
    lv_obj_t *root;
    wristflow_2048_t game;
    bool started, restarting;
};

static lv_obj_t *named(wristflow_2048_view_t *v, const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(v->root, name);
    LV_ASSERT(obj);
    return obj;
}

static void render(wristflow_2048_view_t *v)
{
    if (!v->root) return;
    static const uint32_t colors[] = {0x191c20, 0xede4d6, 0xead6ac, 0xeea365,
        0xec8354, 0xe8664d, 0xdf4c42, 0xe3bf58, 0xd8ad3e, 0xc99b2b, 0xbc8820, 0xefd068};
    for (unsigned i = 0; i < 16; ++i) {
        char name[24], text[16];
        snprintf(name, sizeof name, "game_tile_%u", i);
        lv_obj_t *tile = named(v, name);
        lv_obj_t *number = lv_obj_find_by_name(tile, "game_number");
        unsigned e = v->game.cells[i];
        if (!e) text[0] = '\0';
        else if (e <= 15) snprintf(text, sizeof text, "%lu", (unsigned long)(UINT32_C(1) << e));
        else snprintf(text, sizeof text, "2^%u", e);
        lv_label_set_text(number, text);
        lv_obj_set_style_text_font(number, e < 7 ? value_36 : title_24, 0);
        lv_obj_set_style_text_color(number, lv_color_hex(e >= 3 && e <= 6 ? 0xffffff : 0x40382f), 0);
        lv_obj_set_style_bg_color(tile, lv_color_hex(colors[e < 12 ? e : 11]), 0);
    }
    lv_label_set_text_fmt(named(v, "game_score"), v->game.score >= 1000000 ? "得分 %luK" : "得分 %lu",
        (unsigned long)(v->game.score >= 1000000 ? v->game.score / 1000 : v->game.score));
    lv_label_set_text_fmt(named(v, "game_best"), v->game.best >= 1000000 ? "最高 %luK" : "最高 %lu",
        (unsigned long)(v->game.best >= 1000000 ? v->game.best / 1000 : v->game.best));
    bool win = v->game.won && !v->game.continued;
    bool show = v->restarting || win || v->game.over;
    lv_obj_set_flag(named(v, "game_overlay"), LV_OBJ_FLAG_HIDDEN, !show);
    if (!show) return;
    lv_label_set_text(named(v, "game_message"), v->restarting ? "重新开始？" : win ? "达成 2048！" : "游戏结束");
    lv_label_set_text(named(v, "game_detail"), v->restarting ? "本局进度将清空" : win ? "继续挑战更大的数字" : "没有可以合并的方块了");
    lv_label_set_text(named(v, "game_cancel_text"), v->restarting || win ? "继续" : "返回");
    lv_label_set_text(named(v, "game_confirm_text"), "重来");
}

static void action(lv_event_t *event)
{
    wristflow_2048_view_t *v = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_current_target_obj(event);
    if (target == named(v, "game_restart")) v->restarting = true;
    else if (target == named(v, "game_confirm")) {
        wristflow_2048_restart(&v->game, v->game.random ^ lv_tick_get());
        v->restarting = false;
    } else if (v->restarting) v->restarting = false;
    else if (v->game.won && !v->game.continued) wristflow_2048_continue(&v->game);
    else { wristflow_ui_shell_back(v->shell); return; }
    render(v);
}

static void pop(void *object, int32_t size)
{ lv_obj_set_style_transform_scale(object, size, 0); }

wristflow_2048_view_t *wristflow_2048_view_create(wristflow_ui_shell_t *shell)
{
    wristflow_2048_view_t *v = lv_malloc_zeroed(sizeof *v);
    LV_ASSERT_MALLOC(v);
    v->shell = shell;
    return v;
}

lv_obj_t *wristflow_2048_view_screen(wristflow_2048_view_t *v)
{
    if (!v) return NULL;
    if (!v->started) {
        wristflow_2048_restart(&v->game, lv_tick_get() ^ UINT32_C(0xa53147bd));
        v->started = true;
    }
    if (!v->root) {
        v->root = screen_game_2048_create();
        const char *buttons[] = {"game_restart", "game_cancel", "game_confirm"};
        for (unsigned i = 0; i < 3; ++i)
            lv_obj_add_event_cb(named(v, buttons[i]), action, LV_EVENT_SHORT_CLICKED, v);
    }
    render(v);
    return v->root;
}

bool wristflow_2048_view_gesture(wristflow_2048_view_t *v, lv_dir_t direction, bool edge, lv_point_t origin)
{
    if (!v || !v->root || (direction == LV_DIR_RIGHT && edge)) return false;
    lv_area_t board;
    lv_obj_get_coords(named(v, "game_board"), &board);
    if (origin.x < board.x1 || origin.x > board.x2 || origin.y < board.y1 || origin.y > board.y2) return false;
    wristflow_2048_direction_t move;
    switch (direction) {
    case LV_DIR_LEFT: move = WF_2048_LEFT; break;
    case LV_DIR_RIGHT: move = WF_2048_RIGHT; break;
    case LV_DIR_TOP: move = WF_2048_UP; break;
    case LV_DIR_BOTTOM: move = WF_2048_DOWN; break;
    default: return false;
    }
    if (v->restarting) return true;
    wristflow_2048_change_t change;
    if (!wristflow_2048_move(&v->game, move, &change)) return true;
    render(v);
    for (unsigned i = 0; i < 16; ++i) {
        if (!(change.merged & (1U << i)) && change.spawned != i) continue;
        char name[24]; snprintf(name, sizeof name, "game_tile_%u", i);
        lv_obj_t *tile = named(v, name);
        lv_anim_delete(tile, pop);
        lv_obj_set_style_transform_pivot_x(tile, 37, 0);
        lv_obj_set_style_transform_pivot_y(tile, 37, 0);
        lv_anim_t anim; lv_anim_init(&anim);
        lv_anim_set_var(&anim, tile);
        lv_anim_set_exec_cb(&anim, pop);
        lv_anim_set_values(&anim, change.spawned == i ? 208 : 280, 256);
        lv_anim_set_duration(&anim, 130);
        lv_anim_start(&anim);
    }
    return true;
}

bool wristflow_2048_view_back(wristflow_2048_view_t *v)
{
    if (!v || !v->restarting) return false;
    v->restarting = false;
    render(v);
    return true;
}

void wristflow_2048_view_detach(wristflow_2048_view_t *v)
{ if (v) { v->root = NULL; v->restarting = false; } }
void wristflow_2048_view_destroy(wristflow_2048_view_t *v)
{ if (v) lv_free(v); }
