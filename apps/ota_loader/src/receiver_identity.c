#include "receiver_identity.h"
#include "release.h"
#include <string.h>

bool wf_ota_receiver_address(const uint8_t uid[16], uint8_t address[6])
{
    /* This public, randomly allocated namespace is part of the receiver
     * identity. Keep it across firmware versions so the phone can reconnect. */
    _Alignas(4) uint8_t input[32] = {
        0x1c,0x6d,0xc3,0xc3,0x85,0x9c,0x04,0xcd,
        0x00,0xb5,0x6e,0x91,0x50,0x31,0x18,0x15
    };
    bool has_zero = false, has_one = false;
    for (unsigned i = 0; i < 16; ++i) {
        has_zero |= uid[i] != 0xff;
        has_one |= uid[i] != 0;
    }
    if (!has_zero || !has_one) return false;
    memcpy(input + 16, uid, 16);
    uint32_t digest[8];
    if (!wf_ota_release_hash(digest, 0, input, sizeof input, true)) return false;
    memcpy(address, digest, 6);
    address[5] |= 0xc0;
    static const uint8_t low[6] = {0,0,0,0,0,0xc0};
    static const uint8_t high[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    if (!memcmp(address, low, 6) || !memcmp(address, high, 6)) address[0] ^= 1;
    return true;
}
