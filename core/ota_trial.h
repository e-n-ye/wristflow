#ifndef WF_OTA_TRIAL_H
#define WF_OTA_TRIAL_H
#include <stdint.h>
#include "ota_journal.h"

/* The force-mode cookie is consumed on reset. The journal and verified
 * image stay authoritative. */
#define WF_OTA_ENTER_RECEIVER UINT32_C(0x57464F54)
#define WF_OTA_TRIAL_PENDING UINT32_C(0xA1000000)
#define WF_OTA_TRIAL_HEALTHY UINT32_C(0xA2000000)
#define WF_OTA_TRIAL_KIND    UINT32_C(0xff000000)
#define WF_OTA_TRIAL_PAYLOAD UINT32_C(0x00ffffff)
#define WF_OTA_RESULT_SUCCESS UINT32_C(0xA3000000)
#define WF_OTA_RESULT_ROLLBACK UINT32_C(0xA4000000)
static inline uint32_t wf_ota_trial_cookie(const wf_ota_ab_state_t *s)
{
    const uint8_t *hash = s->image[s->pending].sha256;
    uint32_t identity = hash[0] | ((uint32_t)hash[1] << 8) | ((uint32_t)hash[2] << 16);
    return ((identity ^ s->generation) & 0x007fffffu) | ((uint32_t)s->pending << 23);
}

/* Returns a trial marker only after a TRIED record has been read back.
 * The caller must arm the hardware deadline before handing off that image. */
wf_ota_ab_target_t wf_ota_trial_select(wf_ota_journal_t *journal, const wf_ota_ab_state_t *state,
                                      const wf_ota_ab_image_t checked[2], uint32_t received,
                                      uint32_t *trial_word, uint32_t *result_word);

/* Accept a boot result only against the adjacent TRIED and STABLE records. */
uint32_t wf_ota_trial_result(uint32_t word, const wf_ota_ab_state_t *current,
                             const wf_ota_ab_state_t *previous);
#endif
