#include "aod_display.h"

bool wf_aod_display_restore(wf_aod_display_t *state, const wf_aod_display_io_t *io, void *context)
{
    if (!state->owned) return true;
    bool mode = io->idle(false, context);
    bool brightness = io->brightness(0, context);
    wf_panel_observation_t observed = {0};
    bool valid = io->observe(&observed, context);
    if (!mode || !brightness || !valid || observed.mode != WF_PANEL_NORMAL || observed.brightness != 0) {
        state->phase = WF_AOD_PANEL_FAULT;
        state->failed = true;
        return false;
    }
    io->hide(context);
    state->owned = false;
    state->phase = WF_AOD_PANEL_NORMAL;
    return true;
}

bool wf_aod_display_frame(wf_aod_display_t *state, const wf_aod_display_io_t *io, void *context, uint32_t minute, bool content_changed)
{
    if (state->failed || state->phase == WF_AOD_PANEL_FAULT) return false;
    if (!content_changed && state->phase == WF_AOD_PANEL_RESIDENT && state->frame_minute == minute) return true;
    state->owned = true;
    wf_panel_observation_t observed = {0};
    /* Draw only in NORMAL. The panel keeps the completed GRAM frame in Idle
     * between minute updates; this does not assert a lower scan frequency. */
    /* The locked SDK's brightness command also invokes DisplayOn and resets
     * its mode cache. Enter Idle last, matching the validated A0 sequence. */
    if (!io->brightness(0, context) || !io->idle(false, context) || !io->draw(context) ||
        !io->brightness(WF_AOD_BRIGHTNESS, context) || !io->idle(true, context) ||
        !io->observe(&observed, context) || observed.mode != WF_PANEL_IDLE || observed.brightness != WF_AOD_BRIGHTNESS) {
        state->failed = true;
        state->phase = WF_AOD_PANEL_FAULT;
        wf_aod_display_restore(state, io, context);
        return false;
    }
    state->frame_minute = minute;
    state->phase = WF_AOD_PANEL_RESIDENT;
    return true;
}
