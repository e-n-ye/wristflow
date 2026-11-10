/* Experimental probe protocol; explicitly linked only by the two test images. */
#ifndef WF_OTA_PROBE_H
#define WF_OTA_PROBE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { WF_OTA_PROBE_WORDS=20, WF_OTA_PROBE_MAGIC=0x504f4657,
       WF_OTA_PROBE_PREPARED=1, WF_OTA_PROBE_MAPPED=2, WF_OTA_PROBE_EXECUTED=3,
       WF_OTA_PROBE_ERROR=4, WF_OTA_PROBE_TXE=1, WF_OTA_PROBE_TC=2 };
typedef struct {
    uint32_t magic, schema_kind, tag[4], phase, status, pc, sp, vtor, ccr,
             primask, alias[3], marker, marker_address, image_bytes, crc;
} wf_ota_probe_frame_t;
_Static_assert(sizeof(wf_ota_probe_frame_t)==80, "fixed little-endian wire frame");
typedef struct {
    uint32_t (*flags)(void *);
    void (*put)(void *, uint8_t);
    void *context;
} wf_ota_probe_tx_t;
uint32_t wf_ota_probe_crc(const void *, size_t);
void wf_ota_probe_frame_init(wf_ota_probe_frame_t *, unsigned, const uint32_t tag[4]);
void wf_ota_probe_frame_seal(wf_ota_probe_frame_t *);
/* One total flag-poll budget, including final TC; no clock/IRQ dependency.
 * This bounds software work, not a measured wall-clock duration. */
bool wf_ota_probe_send(const wf_ota_probe_frame_t *, const wf_ota_probe_tx_t *, uint32_t polls);
/* Fixed test-image identity by whole-byte equality, not production OTA auth. */
bool wf_ota_probe_match(const uint8_t *, uint32_t,
                       int (*read)(uint32_t, uint8_t *, uint32_t, void *), void *);
#endif
