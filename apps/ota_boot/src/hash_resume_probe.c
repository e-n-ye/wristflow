/* Fixed SF32LB52 SHA-256 comparison for the signed original-A diagnostic. */
#ifdef WF_OTA_HASH_RESUME_PROBE
#include <bf0_hal.h>
#include <string.h>
#include "hash_resume_probe.h"
#include "release.h"
#include "release_hash_registers.h"
#include "startup_trace.h"

enum {
    ONE_SHOT, HAL_EVERY, HAL_FINAL, WORD_EVERY, WORD_FINAL,
    RETAIN_STATE, WORD_FINAL_REVERSED, CASES
};
static unsigned qualified = CASES;
static uint32_t input[656 / 4];
static const uint8_t expected[32] = {
    0xc0,0x83,0xa4,0x92,0x8d,0x78,0xf5,0x74,0x3a,0x30,0xd4,0x31,0xda,0xf6,0xc6,0x69,
    0xb3,0xf0,0x79,0xa0,0xe4,0x03,0x3c,0x80,0x54,0x71,0x78,0x9b,0xdf,0x8d,0x41,0xb0
};

static bool start_block(unsigned method, uint32_t state[8], uint32_t previous, bool final)
{
    if (!previous) {
        HAL_RCC_EnableModule(RCC_MOD_AES);
        return HAL_HASH_init(NULL, HASH_ALGO_SHA256, 0) == 0;
    }
    if (method == RETAIN_STATE) {
        hwp_aes_acc->HASH_SETTING &= ~(AES_ACC_HASH_SETTING_HASH_IV_LOAD | AES_ACC_HASH_SETTING_HASH_LEN_LOAD);
        return true;
    }
    uint32_t length = (final || method == HAL_EVERY || method == WORD_EVERY) ? previous : 0;
    if (method == HAL_EVERY || method == HAL_FINAL)
        return HAL_HASH_init(state, HASH_ALGO_SHA256, length) == 0;

    wf_hash_restore_words(state, length, method == WORD_FINAL_REVERSED);
    return true;
}

static bool run_block(unsigned method, uint32_t state[8], uint32_t previous,
                      uint8_t *block, unsigned bytes, bool final, bool measure)
{
    if (!start_block(method, state, previous, final)) return false;
    if (measure && previous && method != RETAIN_STATE) {
        unsigned mismatch = 0;
        for (unsigned i = 0; i < 8; ++i) {
            uint32_t intended = method == WORD_FINAL_REVERSED ? __REV(state[i]) : state[i];
            if (*wf_hash_iv_register(i) != intended) mismatch |= 1u << i;
        }
        wf_boot_trace('Y', mismatch);
        wf_boot_trace('L', hwp_aes_acc->HASH_LEN_L);
        if (mismatch && previous == 256) {
            for (unsigned i = 0; i < 8; ++i) wf_boot_trace('a', *wf_hash_iv_register(i));
        }
    }
    if (HAL_HASH_run(block, bytes, final) != HAL_OK) return false;
    HAL_HASH_result((uint8_t *)state);
    if (measure) {
        wf_boot_trace('G', hwp_aes_acc->HASH_RESULT_LEN_L);
        for (unsigned i = 0; i < 8; ++i) wf_boot_trace('W', state[i]);
    }
    return true;
}

static bool original_digest(wf_ota_physical_reader_t *reader, unsigned method, uint32_t state[8])
{
    const uint32_t size = 3055760;
    for (uint32_t off = 0; off < size; off += 256) {
        unsigned n = size - off;
        if (n > 256) n = 256;
        if (wf_ota_physical_read(reader, WF_OTA_READ_A, off, input, n) != (int)n ||
            !run_block(method, state, off, (uint8_t *)input, n, off+n == size, false)) {
            wf_boot_trace('f', off);
            return false;
        }
    }
    return true;
}

bool wf_boot_hash_qualify(wf_ota_physical_reader_t *reader)
{
    uint8_t *data = (uint8_t *)input;
    for (unsigned off = 0; off < sizeof input; off += 256) {
        unsigned n = sizeof input - off;
        if (n > 256) n = 256;
        if (wf_ota_physical_read(reader, WF_OTA_READ_A, off, data + off, n) != (int)n) return false;
    }
    unsigned passed = 0;
    for (unsigned method = 0; method < CASES; ++method) {
        uint32_t state[8];
        wf_boot_trace('K', method);
        bool ok = true;
        for (unsigned off = 0; off < sizeof input;) {
            unsigned n = sizeof input - off;
            if (method != ONE_SHOT && n > 256) n = 256;
            if (!run_block(method, state, off, data + off, n, off+n == sizeof input, true)) {
                wf_boot_trace('e', off);
                ok = false;
                break;
            }
            off += n;
        }
        if (ok && !memcmp(state, expected, sizeof expected)) passed |= 1u << method;
    }
    wf_boot_trace('U', passed);
    /* The previous full-image failure excludes HAL_EVERY as an installer.
     * Prefer the vendor image sequence, then a checked register transfer.
     * Internal state is an option only in this single-owner boot experiment. */
    const uint8_t preference[] = {HAL_FINAL, WORD_FINAL, WORD_FINAL_REVERSED, WORD_EVERY, RETAIN_STATE};
    qualified = CASES;
    if (passed & (1u << ONE_SHOT)) {
        uint32_t enrolled[8];
        if (wf_ota_physical_read(reader, WF_OTA_READ_ORIGINAL_A, 48, enrolled, sizeof enrolled) != sizeof enrolled)
            return false;
        for (unsigned i = 0; i < sizeof preference; ++i) {
            unsigned method = preference[i];
            if (!(passed & (1u << method))) continue;
            uint32_t state[8];
            wf_boot_trace('I', method);
            if (!original_digest(reader, method, state)) return false;
            for (unsigned j = 0; j < 8; ++j) wf_boot_trace('Z', state[j]);
            if (!memcmp(state, enrolled, sizeof enrolled)) {
                qualified = method;
                break;
            }
        }
    }
    /* Selection only qualifies a hardware sequence. The normal signed manifest,
     * body verification, stable journal and vector plan still gate handoff. */
    wf_boot_trace('C', qualified);
    return qualified < CASES;
}

bool wf_ota_release_hash(uint32_t state[8], uint32_t previous, uint8_t *block, unsigned bytes, bool final)
{
    return qualified < CASES && run_block(qualified, state, previous, block, bytes, final, false);
}
#endif
