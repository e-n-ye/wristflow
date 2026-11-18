#include "release.h"
#include "bf0_hal.h"
#include "release_hash_registers.h"

#ifndef WF_OTA_HASH_RESUME_PROBE
bool wf_ota_release_hash(uint32_t state[8], uint32_t previous, uint8_t *block,
                         unsigned bytes, bool final)
{
    if (previous & 63u) return false;
    if (!previous) {
        HAL_RCC_EnableModule(RCC_MOD_AES);
        if (HAL_HASH_init(NULL, HASH_ALGO_SHA256, 0) != 0) return false;
    } else {
        /* The SDK image verifier imports the cumulative byte length for padding
         * on the final block. Preserve the raw result words in register order. */
        wf_hash_restore_words(state, final ? previous : 0, false);
    }
    if (HAL_HASH_run(block, bytes, final) != HAL_OK) return false;
    HAL_HASH_result((uint8_t *)state);
    return true;
}
#endif
