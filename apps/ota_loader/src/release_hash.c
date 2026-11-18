#include "release.h"
#include "bf0_hal.h"

bool wf_ota_release_hash(uint32_t state[8], uint32_t previous, uint8_t *block,
                         unsigned bytes, bool final)
{
    if (!previous) HAL_RCC_EnableModule(RCC_MOD_AES);
    if (HAL_HASH_init(previous ? state : NULL, HASH_ALGO_SHA256, previous) != 0 ||
        HAL_HASH_run(block, bytes, final) != HAL_OK) return false;
    HAL_HASH_result((uint8_t *)state);
    return true;
}
