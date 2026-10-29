#include "resource_probe.h"
#include "wristflow_ui.h"
#include <assert.h>

static uint32_t ticks;
static uint8_t pixels[390 * 40 * 4];
static uint32_t tick(void) { return ticks; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }
static void expired(void *owner) { (void)owner; }

int main(void)
{
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_buffers(display, pixels, NULL, sizeof pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    wristflow_ui_init("");
    wristflow_ui_shell_config_t config = {.watchface=&wristflow_default_watchface,
        .controls=screen_control_center_create, .initial_snapshot={10, 48, 50}};
    wristflow_ui_shell_t *shell = wristflow_ui_shell_create(&config);
    assert(shell);
    wristflow_ui_shell_enable_display_policy(shell);
    wf_resource_probe_t probe = {0};
    const void *images[] = {hour_1, hour_7, hour_unknown, NULL,
        hour_2, hour_3, hour_4, hour_5, hour_6, hour_8, hour_9, hour_10, hour_11, hour_12};
    for (unsigned i = 0; i < 14; ++i) {
        if (i == 3) continue;
        assert(wf_resource_probe_start(&probe, shell, i, i+1));
        assert(lv_image_get_src(lv_obj_find_by_name(probe.root, "hour_artwork")) == images[i]);
        assert(!wf_resource_probe_start(&probe, shell, i, i+1));
        wf_resource_probe_stop(&probe, 2);
        assert(!probe.active && !probe.root && probe.reason == 2);
        wf_resource_probe_stop(&probe, 2);
    }
    int owner;
    assert(wristflow_ui_shell_acquire_awake(shell, &owner, "other", 1000, expired));
    assert(!wf_resource_probe_start(&probe, shell, 3, 100));
    wristflow_ui_shell_release_awake(shell, &owner);
    assert(!wf_resource_probe_start(&probe, shell, 15, 100));
    assert(wf_resource_probe_start(&probe, shell, 14, 100));
    lv_obj_t *thumb = lv_obj_find_by_name(probe.root, "thumb_hour");
    assert(thumb && lv_image_get_src(thumb) == hour_10);
    assert(lv_image_get_inner_align(thumb) == LV_IMAGE_ALIGN_STRETCH);
    wf_resource_probe_stop(&probe, 2);
    assert(!wf_resource_probe_start(&probe, shell, 3, 0));
    assert(wf_resource_probe_start(&probe, shell, 3, 100));
    assert(lv_obj_get_style_text_font(lv_obj_find_by_name(probe.root, "message_body"), 0) == notification_22);
    ticks += 59999;
    wf_resource_probe_tick(&probe, true);
    assert(probe.active);
    ++ticks;
    wf_resource_probe_tick(&probe, true);
    assert(!probe.active && probe.reason == 1 && probe.elapsed == 60000);
    assert(wristflow_ui_shell_acquire_awake(shell, &owner, "released", 1000, expired));
    wristflow_ui_shell_release_awake(shell, &owner);
    assert(wf_resource_probe_start(&probe, shell, 0, 101));
    wf_resource_probe_tick(&probe, false);
    assert(!probe.active && probe.reason == 4 && !probe.root);
    assert(wf_resource_probe_start(&probe, shell, 0, 102));
    ticks += 60000;
    lv_timer_handler();
    assert(!probe.active && probe.reason == 1); /* Shell expiry callback, not explicit tick. */
    wristflow_ui_shell_destroy(shell);
    lv_deinit();
    return 0;
}
