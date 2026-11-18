/* SF32LB52 hash state registers require aligned, 32-bit transfers. */
#ifndef WF_RELEASE_HASH_REGISTERS_H
#define WF_RELEASE_HASH_REGISTERS_H
#include <bf0_hal.h>
#include <stdbool.h>
#include <stddef.h>

_Static_assert(offsetof(AES_ACC_TypeDef, HASH_IV_H7) == offsetof(AES_ACC_TypeDef, HASH_IV_H0) + 28,
               "SHA-256 IV register layout");
static inline volatile uint32_t *wf_hash_iv_register(unsigned index)
{
    return (volatile uint32_t *)((uintptr_t)&hwp_aes_acc->HASH_IV_H0 + 4u * index);
}

static inline void wf_hash_restore_words(const uint32_t state[8], uint32_t length, bool reversed)
{
    HAL_HASH_reset();
    for (unsigned i = 0; i < 8; ++i)
        *wf_hash_iv_register(i) = reversed ? __REV(state[i]) : state[i];
    uint32_t setting = (HASH_ALGO_SHA256 << AES_ACC_HASH_SETTING_HASH_MODE_Pos) |
                       AES_ACC_HASH_SETTING_DFT_IV_SEL | AES_ACC_HASH_SETTING_HASH_IV_LOAD;
    hwp_aes_acc->HASH_SETTING = setting;
    if (length) {
        hwp_aes_acc->HASH_LEN_L = length;
        setting |= AES_ACC_HASH_SETTING_HASH_LEN_LOAD;
    }
    hwp_aes_acc->HASH_SETTING = setting;
}
#endif
