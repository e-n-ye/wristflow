#include "bf0_hal.h"
#include "release.h"
#include <assert.h>
#include <string.h>

static AES_ACC_TypeDef regs;
AES_ACC_TypeDef *hwp_aes_acc = &regs;
static unsigned starts, restores, results;
static int status;
static uint32_t digest[8];
void HAL_RCC_EnableModule(int module) { assert(module == RCC_MOD_AES); }
int HAL_HASH_init(uint32_t *iv, uint8_t algorithm, uint32_t previous)
{
    assert(!iv && algorithm == HASH_ALGO_SHA256 && !previous);
    ++starts;
    return 0;
}
int HAL_HASH_reset(void)
{
    ++restores;
    memset(&regs, 0, sizeof regs);
    return 0;
}
int HAL_HASH_run(uint8_t *input, int size, int final)
{
    assert(input && size && (final == 0 || final == 1));
    return status;
}
void HAL_HASH_result(uint8_t *output)
{
    ++results;
    memcpy(output, digest, sizeof digest);
}
int main(void)
{
    uint8_t block[256] = {0};
    uint32_t state[8];
    for (unsigned i = 0; i < 8; ++i) digest[i] = 0x1234abf0u + i;
    assert(wf_ota_release_hash(state, 0, block, 256, false) && starts == 1 && results == 1);
    assert(wf_ota_release_hash(state, 256, block, 256, false));
    uint32_t imported[8];
    memcpy(imported, (const void *)&regs.HASH_IV_H0, sizeof imported);
    assert(!memcmp(imported, digest, sizeof digest));
    const unsigned iv_setting = HASH_ALGO_SHA256 | AES_ACC_HASH_SETTING_DFT_IV_SEL | AES_ACC_HASH_SETTING_HASH_IV_LOAD;
    assert(regs.HASH_SETTING == iv_setting && regs.HASH_LEN_L == 0);
    assert(wf_ota_release_hash(state, 512, block, 144, true));
    assert(regs.HASH_SETTING == (iv_setting | AES_ACC_HASH_SETTING_HASH_LEN_LOAD) && regs.HASH_LEN_L == 512);
    assert(!wf_ota_release_hash(state, 511, block, 144, true) && restores == 2 && starts == 1);
    status = HAL_ERROR;
    assert(!wf_ota_release_hash(state, 512, block, 144, true) && results == 3);
    return 0;
}
