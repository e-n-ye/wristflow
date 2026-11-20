#include "ota_trial.h"
#include <string.h>

wf_ota_ab_target_t wf_ota_trial_select(wf_ota_journal_t *journal, const wf_ota_ab_state_t *state,
                                      const wf_ota_ab_image_t checked[2], uint32_t received,
                                      uint32_t *trial_word, uint32_t *result_word)
{
    wf_ota_ab_plan_t plan;
    *trial_word = 0;
    *result_word = 0;
    if (!wf_ota_ab_plan(&plan, state, checked)) return WF_OTA_AB_RECOVERY;
    if (!plan.persist) return plan.target;

    uint8_t readback[WF_OTA_AB_BYTES];
    if (state->phase == WF_OTA_AB_TRIED &&
        received == (WF_OTA_TRIAL_HEALTHY | wf_ota_trial_cookie(state))) {
        wf_ota_ab_state_t confirmed;
        if (wf_ota_ab_confirm(&confirmed, state, state->pending, &checked[state->pending])) {
            if (wf_ota_journal_commit(journal, state, &confirmed, readback) == WF_OTA_JOURNAL_OK) {
                *result_word = WF_OTA_RESULT_SUCCESS | wf_ota_trial_cookie(state);
                return (wf_ota_ab_target_t)confirmed.stable;
            }
            return plan.fallback;
        }
    }
    if (wf_ota_journal_commit(journal, state, &plan.next, readback) != WF_OTA_JOURNAL_OK)
        return plan.fallback;
    wf_ota_ab_target_t target = wf_ota_ab_resolve(&plan, readback, sizeof readback);
    if (target != plan.fallback)
        *trial_word = WF_OTA_TRIAL_PENDING | wf_ota_trial_cookie(&plan.next);
    else if (state->phase == WF_OTA_AB_TRIED)
        *result_word = WF_OTA_RESULT_ROLLBACK | wf_ota_trial_cookie(state);
    return target;
}

uint32_t wf_ota_trial_result(uint32_t word, const wf_ota_ab_state_t *current,
                             const wf_ota_ab_state_t *previous)
{
    if (current->phase != WF_OTA_AB_STABLE || previous->phase != WF_OTA_AB_TRIED ||
        previous->pending > WF_OTA_AB_SLOT_B || current->generation <= previous->generation ||
        current->generation - previous->generation != 1 ||
        memcmp(current->image, previous->image, sizeof current->image)) return 0;

    uint32_t kind = current->stable == previous->pending ? WF_OTA_RESULT_SUCCESS : WF_OTA_RESULT_ROLLBACK;
    return word == (kind | wf_ota_trial_cookie(previous)) ? kind : 0;
}
