#ifndef WF_TEST_HASH_HAL_H
#define WF_TEST_HASH_HAL_H
#include <stdint.h>
#define __IO volatile
#include "aes_acc.h"
extern AES_ACC_TypeDef *hwp_aes_acc;
enum { HAL_OK = 0, HAL_ERROR = 1, HASH_ALGO_SHA256 = 2, RCC_MOD_AES = 1 };
static inline uint32_t __REV(uint32_t value)
{
    return (value >> 24) | ((value >> 8) & 0xff00u) | ((value << 8) & 0xff0000u) | (value << 24);
}
void HAL_RCC_EnableModule(int module);
int HAL_HASH_init(uint32_t *iv, uint8_t algorithm, uint32_t previous);
int HAL_HASH_reset(void);
int HAL_HASH_run(uint8_t *input, int size, int final);
void HAL_HASH_result(uint8_t *output);
#endif
