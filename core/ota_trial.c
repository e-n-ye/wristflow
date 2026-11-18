#include "ota_trial.h"

wf_ota_ab_target_t wf_ota_trial_select(wf_ota_journal_t *journal, const wf_ota_ab_state_t *state,
                                      const wf_ota_ab_image_t checked[2], uint32_t received,
                                      uint32_t *trial_word)
{
    wf_ota_ab_plan_t plan;
    *trial_word = 0;
    if (!wf_ota_ab_plan(&plan, state, checked)) return WF_OTA_AB_RECOVERY;
    if (!plan.persist) return plan.target;

    uint8_t readback[WF_OTA_AB_BYTES];
    if (state->phase == WF_OTA_AB_TRIED &&
        received == (WF_OTA_TRIAL_HEALTHY | wf_ota_trial_cookie(state))) {
        wf_ota_ab_state_t confirmed;
        if (wf_ota_ab_confirm(&confirmed, state, state->pending, &checked[state->pending])) {
            if (wf_ota_journal_commit(journal, state, &confirmed, readback) == WF_OTA_JOURNAL_OK)
                return (wf_ota_ab_target_t)confirmed.stable;
            return plan.fallback;
        }
    }
    if (wf_ota_journal_commit(journal, state, &plan.next, readback) != WF_OTA_JOURNAL_OK)
        return plan.fallback;
    wf_ota_ab_target_t target = wf_ota_ab_resolve(&plan, readback, sizeof readback);
    if (target != plan.fallback)
        *trial_word = WF_OTA_TRIAL_PENDING | wf_ota_trial_cookie(&plan.next);
    return target;
}
