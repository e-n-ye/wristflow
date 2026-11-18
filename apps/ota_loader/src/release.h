#ifndef WF_OTA_RELEASE_H
#define WF_OTA_RELEASE_H
#include "ota_ab.h"
#include "physical_read.h"

enum { WF_RELEASE_MANIFEST_BYTES = 80, WF_RELEASE_SIGNATURE_BYTES = 256,
       WF_RELEASE_HEADER_BYTES = WF_RELEASE_MANIFEST_BYTES + WF_RELEASE_SIGNATURE_BYTES };

/* Wire order is signed header then unpadded main.bin. A slot keeps main.bin at
 * offset zero and the same header at its fixed end, outside the image extent. */
bool wf_ota_release_open(const uint8_t *header, size_t size, uint32_t slot_bytes,
                         wf_ota_ab_image_t *image);
bool wf_ota_release_verify_slot(wf_ota_physical_reader_t *reader, unsigned slot,
                                wf_ota_ab_image_t *image);

/* The standalone loader owns the SHA peripheral during verification. Each
 * non-final input is a multiple of 64 bytes. State is the prior hardware IV. */
bool wf_ota_release_hash(uint32_t state[8], uint32_t previous, uint8_t *block,
                         unsigned bytes, bool final);
#endif
