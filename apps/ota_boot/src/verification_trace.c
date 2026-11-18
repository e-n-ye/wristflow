/* Observe the authentication decision without substituting a diagnostic result. */
#ifdef WF_OTA_BOOT_TRACE
#include "startup_trace.h"
#include "release.h"
#include "mbedtls/sha256.h"
#ifdef WF_OTA_HASH_RESUME_PROBE
#include <string.h>
#endif

int wf_boot_trace_rc(char phase, int rc)
{
    wf_boot_trace(phase, (uint32_t)rc);
    return rc;
}

bool wf_boot_trace_hash(char phase, uint32_t state[8], uint32_t offset,
                        uint8_t *block, unsigned bytes, bool final)
{
    /* A single boot stream owns the reference context. The CPU and peripheral
     * read the same SRAM block before its caller advances to the next block. */
    static mbedtls_sha256_context reference;
    if (!offset) mbedtls_sha256_starts(&reference, 0);
    mbedtls_sha256_update(&reference, block, bytes);
    if (!wf_ota_release_hash(state, offset, block, bytes, final)) {
        wf_boot_trace('g', offset);
        return false;
    }
    if (final) {
        uint32_t software[8];
        mbedtls_sha256_finish(&reference, (uint8_t *)software);
        for (unsigned i = 0; i < 8; ++i) wf_boot_trace(phase, state[i]);
        for (unsigned i = 0; i < 8; ++i) wf_boot_trace(phase + 1, software[i]);
#ifdef WF_OTA_HASH_RESUME_PROBE
        if (memcmp(software, state, sizeof software)) {
            wf_boot_trace('X', (uint8_t)phase);
            return false;
        }
#endif
    }
    return true;
}
#endif
