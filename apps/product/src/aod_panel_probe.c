#include "aod_panel_probe.h"

bool wf_aod_probe_filter_key(wf_aod_probe_input_t *input, bool pressed, bool released)
{
    if (input->active && pressed) { input->held = true; input->cancel = true; }
    if (!input->held) return true;
    if (released) input->held = false;
    return false;
}

static void publish(wf_aod_probe_t *p, const wf_aod_probe_io_t *io, void *ctx)
{
    p->elapsed_ms = io->now(ctx) - p->started_ms;
    if (io->publish) io->publish(p, ctx);
}

bool wf_aod_probe_restore(wf_aod_probe_t *p, const wf_aod_probe_io_t *io, void *ctx)
{
    if (!p || !io || !io->now || !io->idle || !io->brightness || !io->observe) return false;
    if (!p->restore_pending) return p->restored;
    /* Both operations are attempted even after one fails. Never report clean
     * from an earlier cached observation or from request acceptance alone. */
    bool mode_ok = io->idle(false, ctx);
    bool brightness_ok = io->brightness(p->before.brightness, ctx);
    wf_panel_observation_t after = {0};
    p->valid &= (uint8_t)~4U;
    p->after = after;
    bool observed = io->observe(&after, ctx);
    if (observed) { p->after = after; p->valid |= 4; }
    p->restored = mode_ok && brightness_ok && observed &&
        after.mode == WF_PANEL_NORMAL && after.brightness == p->before.brightness;
    p->restore_pending = !p->restored;
    if (!p->restored) p->phase = WF_AOD_PROBE_FAULT;
    publish(p, io, ctx);
    return p->restored;
}

static bool within_deadline(wf_aod_probe_t *p, const wf_aod_probe_io_t *io, void *ctx)
{ return io->now(ctx) - p->started_ms < p->duration_ms; }

bool wf_aod_probe_await_restore(wf_aod_probe_t *p, const wf_aod_probe_io_t *io, void *ctx)
{
    if (!p || !io || !io->wait || !io->now || !io->idle || !io->brightness || !io->observe) return false;
    while (p->restore_pending) {
        bool retry = false;
        p->interrupted_events |= io->wait(0, &retry, ctx);
        if (retry) wf_aod_probe_restore(p, io, ctx);
        else publish(p, io, ctx);
    }
    return p->restored;
}

bool wf_aod_probe_run(wf_aod_probe_t *p, const wf_aod_probe_io_t *io, void *ctx, unsigned seconds)
{
    if (!p || p->restore_pending || !io || !io->now || !io->observe || !io->brightness ||
        !io->idle || !io->wait || seconds < 10 || seconds > 60) return false;
    *p = (wf_aod_probe_t){.phase = WF_AOD_PROBE_REJECTED, .started_ms = io->now(ctx),
        .duration_ms = seconds * 1000U};
    wf_panel_observation_t before = {0};
    if (!io->observe(&before, ctx) || before.mode != WF_PANEL_NORMAL ||
        before.brightness == 0 || before.brightness > 100) {
        publish(p, io, ctx);
        return false;
    }
    p->before = before; p->valid = 1;
    p->phase = WF_AOD_PROBE_RUNNING;
    /* Own restoration BEFORE the first possible partial hardware mutation. */
    p->restore_pending = true;
    publish(p, io, ctx);
    bool configured = within_deadline(p, io, ctx) && io->brightness(5, ctx) &&
        within_deadline(p, io, ctx) && io->idle(true, ctx) &&
        within_deadline(p, io, ctx);
    wf_panel_observation_t entered = {0};
    bool observed = configured && io->observe(&entered, ctx);
    if (observed) { p->entered = entered; p->valid |= 2; }
    if (!observed || entered.mode != WF_PANEL_IDLE || entered.brightness != 5) {
        p->phase = WF_AOD_PROBE_FAULT;
    } else {
        publish(p, io, ctx);
        while (within_deadline(p, io, ctx)) {
            bool cancelled = false;
            uint32_t remaining = p->duration_ms - (io->now(ctx) - p->started_ms);
            if (remaining > p->duration_ms) break; /* Clock advanced across deadline. */
            p->interrupted_events |= io->wait(remaining, &cancelled, ctx);
            if (cancelled || p->interrupted_events) {
                p->phase = WF_AOD_PROBE_CANCELLED;
                break;
            }
        }
        if (p->phase == WF_AOD_PROBE_RUNNING) p->phase = WF_AOD_PROBE_DONE;
    }
    bool restored = wf_aod_probe_restore(p, io, ctx);
    return restored && p->phase == WF_AOD_PROBE_DONE;
}
