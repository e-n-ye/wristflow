#include "aod_panel_probe.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint32_t now, event, wait_elapsed, step_ms;
    wf_panel_observation_t state;
    unsigned calls, fail_call, observe_calls, publish_calls, mode_on, mode_off, brightness_calls;
    unsigned recovery_waits;
    bool cancel, ignore_mode, ignore_brightness, fail_restore_mode, fail_restore_brightness;
} fake_t;
static uint32_t now(void *ctx) { return ((fake_t *)ctx)->now; }
static bool succeeds(fake_t *f)
{
    f->now += f->step_ms;
    return ++f->calls != f->fail_call;
}
static bool observe(wf_panel_observation_t *out, void *ctx)
{
    fake_t *f = ctx; ++f->observe_calls;
    if (!succeeds(f)) return false;
    *out = f->state;
    return true;
}
static bool brightness(uint8_t value, void *ctx)
{
    fake_t *f = ctx; ++f->brightness_calls;
    /* Model a partial mutation even when the operation reports failure. */
    if (!f->ignore_brightness) f->state.brightness = value;
    bool ok = succeeds(f);
    return ok && !(f->fail_restore_brightness && value == 55);
}
static bool idle(bool enabled, void *ctx)
{
    fake_t *f = ctx;
    if (enabled) ++f->mode_on; else ++f->mode_off;
    if (!f->ignore_mode) f->state.mode = enabled ? WF_PANEL_IDLE : WF_PANEL_NORMAL;
    bool ok = succeeds(f);
    return ok && !(f->fail_restore_mode && !enabled);
}
static uint32_t wait_event(uint32_t remaining, bool *cancelled, void *ctx)
{
    fake_t *f = ctx;
    if (!remaining) {
        /* A service event alone must neither release quarantine nor retry I/O. */
        ++f->recovery_waits;
        if (f->recovery_waits == 1) {
            *cancelled = false;
            return 32;
        }
        assert(f->recovery_waits == 2);
        assert(f->mode_off == 1); /* Service event did not attempt recovery. */
        f->fail_restore_mode = f->fail_restore_brightness = false;
        *cancelled = true;
        return 0;
    }
    assert(remaining > 0 && remaining <= 60000);
    f->now += f->wait_elapsed ? f->wait_elapsed : remaining;
    *cancelled = f->cancel;
    return f->event;
}
static void publish(const wf_aod_probe_t *p, void *ctx)
{ fake_t *f = ctx; ++f->publish_calls; assert(p->elapsed_ms == f->now - p->started_ms); }
static const wf_aod_probe_io_t io = {now, observe, brightness, idle, wait_event, publish};
static fake_t normal(void)
{ return (fake_t){.state = {WF_PANEL_NORMAL, 55}, .step_ms = 1}; }

int main(void)
{
    wf_aod_probe_input_t key = {0};
    assert(wf_aod_probe_filter_key(&key, true, false));
    assert(wf_aod_probe_filter_key(&key, false, true));
    key.active = true;
    assert(!wf_aod_probe_filter_key(&key, true, false) && key.cancel && key.held);
    /* Restoration ends the window before the SDK emits CLICKED/RELEASED. */
    key.active = false; key.cancel = false;
    assert(!wf_aod_probe_filter_key(&key, false, false));
    assert(!wf_aod_probe_filter_key(&key, false, true) && !key.held);
    assert(wf_aod_probe_filter_key(&key, true, false));
    key = (wf_aod_probe_input_t){.active = true};
    assert(!wf_aod_probe_filter_key(&key, true, false));
    assert(!wf_aod_probe_filter_key(&key, false, false));
    assert(!wf_aod_probe_filter_key(&key, false, true));
    wf_aod_probe_t p = {0}; fake_t f = normal();
    assert(!wf_aod_probe_run(&p, &io, &f, 9) && !f.calls);
    assert(!wf_aod_probe_run(&p, &io, &f, 61) && !f.calls);
    assert(wf_aod_probe_run(&p, &io, &f, 30));
    assert(p.phase == WF_AOD_PROBE_DONE && p.valid == 7 && p.restored && !p.restore_pending);
    assert(p.entered.mode == WF_PANEL_IDLE && p.entered.brightness == 5);
    assert(p.after.mode == WF_PANEL_NORMAL && p.after.brightness == 55);
    assert(f.mode_on == 1 && f.mode_off == 1 && f.brightness_calls == 2);
    assert(p.elapsed_ms >= 30000 && p.elapsed_ms < 30010 && f.publish_calls >= 3);

    /* Unsuitable prior state must never be borrowed or "restored". */
    for (unsigned mode = WF_PANEL_OTHER; mode <= WF_PANEL_IDLE; ++mode) {
        if (mode == WF_PANEL_NORMAL) continue;
        f = normal(); f.state.mode = mode; p = (wf_aod_probe_t){0};
        assert(!wf_aod_probe_run(&p, &io, &f, 10));
        assert(p.phase == WF_AOD_PROBE_REJECTED && !p.restore_pending && !f.mode_off && !f.brightness_calls);
    }
    f = normal(); f.state.brightness = 0; p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 10) && !f.brightness_calls);

    /* Every I/O failure, including failures after a partial write, preserves
     * the failure and tries both restoration operations. */
    for (unsigned failure = 1; failure <= 7; ++failure) {
        f = normal(); f.fail_call = failure; p = (wf_aod_probe_t){0};
        assert(!wf_aod_probe_run(&p, &io, &f, 10));
        if (failure == 1) assert(p.phase == WF_AOD_PROBE_REJECTED && !f.mode_off);
        else {
            assert(p.phase == WF_AOD_PROBE_FAULT && f.mode_off == 1);
            assert(f.state.mode == WF_PANEL_NORMAL && f.state.brightness == 55);
            if (p.restore_pending) {
                unsigned calls = f.calls;
                assert(!wf_aod_probe_run(&p, &io, &f, 10) && calls == f.calls);
                f.fail_call = 0;
                assert(wf_aod_probe_restore(&p, &io, &f));
                assert(p.phase == WF_AOD_PROBE_FAULT && p.restored && !p.restore_pending);
            }
        }
    }
    f = normal(); f.ignore_mode = true; p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 10) && p.phase == WF_AOD_PROBE_FAULT && p.restored);
    f = normal(); f.ignore_brightness = true; p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 10) && p.phase == WF_AOD_PROBE_FAULT && p.restored);

    /* Event batches are returned intact for the caller to replay. */
    f = normal(); f.event = 2 | 16 | 32 | 64 | 128; f.wait_elapsed = 25;
    p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 60));
    assert(p.phase == WF_AOD_PROBE_CANCELLED && p.interrupted_events == f.event && p.restored);
    assert(p.elapsed_ms < 100);
    f = normal(); f.cancel = true; f.wait_elapsed = 1; p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 60) && p.phase == WF_AOD_PROBE_CANCELLED && p.restored);

    /* Setup consumes the SAME deadline. No late entry or renewed duration. */
    f = normal(); f.step_ms = 6000; p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 10) && !f.mode_on && p.restored);
    f = normal(); f.now = UINT32_MAX - 5000; p = (wf_aod_probe_t){0};
    assert(wf_aod_probe_run(&p, &io, &f, 10) && p.elapsed_ms >= 10000 && p.restored);

    /* A failed restore cannot become clean from cache values alone. */
    f = normal(); f.fail_restore_mode = f.fail_restore_brightness = true; p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 10) && p.restore_pending && !p.restored);
    assert(p.after.mode == WF_PANEL_NORMAL && p.after.brightness == 55);
    f.fail_restore_mode = false;
    assert(!wf_aod_probe_restore(&p, &io, &f) && p.restore_pending);
    f.fail_restore_brightness = false;
    assert(wf_aod_probe_restore(&p, &io, &f) && p.restored && p.phase == WF_AOD_PROBE_FAULT);
    f.fail_call = f.calls + 3;
    p.restore_pending = true;
    assert(!wf_aod_probe_restore(&p, &io, &f) && !(p.valid & 4) && p.restore_pending);
    f.fail_call = 0;
    assert(wf_aod_probe_restore(&p, &io, &f));
    f = normal(); f.fail_restore_mode = f.fail_restore_brightness = true; p = (wf_aod_probe_t){0};
    assert(!wf_aod_probe_run(&p, &io, &f, 10) && p.restore_pending);
    unsigned restores = f.mode_off;
    assert(wf_aod_probe_await_restore(&p, &io, &f));
    assert(f.recovery_waits == 2 && f.mode_off == restores + 1 && p.interrupted_events == 32);
    assert(p.phase == WF_AOD_PROBE_FAULT && p.restored && !p.restore_pending);
    return 0;
}
