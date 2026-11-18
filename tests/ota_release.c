#include "release.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A public signature of the ordinary 11/08 Product build, release 2026111801. */
static const uint8_t signed_header[] = {
#include "ota_release_fixture.inc"
};
static uint8_t slot[0x400000];
static const uint32_t physical_b = 0x12420000;

bool wf_ota_release_hash(uint32_t state[8], uint32_t previous, uint8_t *block,
                         unsigned bytes, bool final)
{
    static mbedtls_sha256_context sha;
    if (!previous) {
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);
    }
    assert(sha.total[0] == previous);
    mbedtls_sha256_update(&sha, block, bytes);
    if (final) {
        mbedtls_sha256_finish(&sha, (uint8_t *)state);
        mbedtls_sha256_free(&sha);
    }
    return true;
}
static bool ready(void *context) { (void)context; return true; }
static int read_bytes(void *context, uint32_t address, void *out, size_t bytes)
{
    (void)context;
    assert(address >= physical_b && address - physical_b <= sizeof slot - bytes);
    memcpy(out, slot + (address - physical_b), bytes);
    return (int)bytes;
}

int main(int argc, char **argv)
{
    wf_ota_ab_image_t image;
    assert(wf_ota_release_open(signed_header, sizeof signed_header, sizeof slot, &image));
    assert(image.bytes == 3028888 && image.release == 2026111801);
    assert(!wf_ota_release_open(signed_header, sizeof signed_header - 1, sizeof slot, &image));
    assert(!wf_ota_release_open(signed_header, sizeof signed_header, 0x800000, &image));
    uint8_t altered[sizeof signed_header];
    for (size_t i = 0; i < sizeof altered; i++) {
        memcpy(altered, signed_header, sizeof altered);
        altered[i] ^= 1;
        assert(!wf_ota_release_open(altered, sizeof altered, sizeof slot, &image));
        assert(!image.bytes && !image.release);
    }
    wf_ota_physical_reader_t reader = {.ready = ready, .read = read_bytes,
        .region = {[WF_OTA_READ_B] = {physical_b, sizeof slot}}};
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_B, &image));
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_META0, &image));
    if (argc == 2) {
        FILE *release = fopen(argv[1], "rb");
        assert(release);
        uint8_t *tail = slot + sizeof slot - WF_RELEASE_HEADER_BYTES;
        assert(fread(tail, 1, WF_RELEASE_HEADER_BYTES, release) == WF_RELEASE_HEADER_BYTES);
        assert(wf_ota_release_open(tail, WF_RELEASE_HEADER_BYTES, sizeof slot, &image));
        assert(fread(slot, 1, image.bytes, release) == image.bytes && fgetc(release) == EOF);
        fclose(release);
        assert(wf_ota_release_verify_slot(&reader, WF_OTA_READ_B, &image));
        printf("verified release %u, %u bytes\n", image.release, image.bytes);
        slot[image.bytes - 1] ^= 1;
        assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_B, &image));
        assert(!image.bytes && !image.release);
    }
    return 0;
}
