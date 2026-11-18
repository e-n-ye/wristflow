#include "release.h"
#include <string.h>
#include "mbedtls/pk.h"

static const uint8_t public_key[] = {
#include "release_public_key.inc"
};
static const uint8_t board[24] = "sf32lb52-lchspi-ulp";

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool wf_ota_release_open(const uint8_t *header, size_t size, uint32_t slot_bytes,
                         wf_ota_ab_image_t *image)
{
    if (!image) return false;
    memset(image, 0, sizeof *image);
    if (!header || size != WF_RELEASE_HEADER_BYTES || slot_bytes != 0x400000u ||
        memcmp(header, "WFR1", 4) || le32(header + 4) != 1 ||
        memcmp(header + 8, board, sizeof board) || le32(header + 32) != 1 ||
        le32(header + 36) != 0x12020000u) return false;
    uint32_t bytes = le32(header + 40), version = le32(header + 44);
    if (bytes < 8 || bytes > slot_bytes - WF_RELEASE_HEADER_BYTES || !version) return false;

    /* The SHA engine reads from DMA-accessible, aligned RAM. */
    uint32_t manifest[WF_RELEASE_MANIFEST_BYTES / sizeof(uint32_t)], hash[8];
    memcpy(manifest, header, sizeof manifest);
    if (!wf_ota_release_hash(hash, 0, (uint8_t *)manifest, sizeof manifest, true)) return false;
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    bool valid = mbedtls_pk_parse_public_key(&key, public_key, sizeof public_key) == 0 &&
                 mbedtls_pk_get_type(&key) == MBEDTLS_PK_RSA &&
                 mbedtls_pk_get_bitlen(&key) == 2048 &&
                 mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, (const uint8_t *)hash, sizeof hash,
                                   header + WF_RELEASE_MANIFEST_BYTES, WF_RELEASE_SIGNATURE_BYTES) == 0;
    mbedtls_pk_free(&key);
    if (!valid) return false;
    image->bytes = bytes;
    image->release = version;
    memcpy(image->sha256, header + 48, sizeof image->sha256);
    return true;
}

bool wf_ota_release_verify_slot(wf_ota_physical_reader_t *reader, unsigned slot, wf_ota_ab_image_t *image)
{
    if (!image) return false;
    memset(image, 0, sizeof *image);
    if (!reader || slot > WF_OTA_READ_B || reader->region[slot].bytes != 0x400000u) return false;
    uint32_t capacity = reader->region[slot].bytes, tail = capacity - WF_RELEASE_HEADER_BYTES;
    uint8_t header[WF_RELEASE_HEADER_BYTES];
    if (wf_ota_physical_read(reader, slot, tail, header, WF_RELEASE_MANIFEST_BYTES) != WF_RELEASE_MANIFEST_BYTES ||
        wf_ota_physical_read(reader, slot, tail + WF_RELEASE_MANIFEST_BYTES, header + WF_RELEASE_MANIFEST_BYTES,
                             WF_RELEASE_SIGNATURE_BYTES) != WF_RELEASE_SIGNATURE_BYTES) return false;
    wf_ota_ab_image_t signed_image;
    if (!wf_ota_release_open(header, sizeof header, capacity, &signed_image)) return false;
    uint32_t block[64], hash[8];
    for (uint32_t offset = 0; offset < signed_image.bytes;) {
        unsigned n = signed_image.bytes - offset;
        if (n > sizeof block) n = sizeof block;
        if (wf_ota_physical_read(reader, slot, offset, block, n) != (int)n ||
            !wf_ota_release_hash(hash, offset, (uint8_t *)block, n, offset + n == signed_image.bytes)) return false;
        offset += n;
    }
    if (memcmp(hash, signed_image.sha256, sizeof hash)) return false;
    *image = signed_image;
    return true;
}
