#ifndef WF_OTA_OFFLINE_RECEIVER_H
#define WF_OTA_OFFLINE_RECEIVER_H
#include "release.h"

enum { WF_DFU_BLOCK = 2048, WF_DFU_ENVELOPE = 18, WF_DFU_SLOT_BYTES = 0x400000 };
enum { WF_DFU_CANCEL = 37, WF_DFU_DATA = 40, WF_DFU_NOT_READY = 4 };
typedef enum { WF_DFU_IDLE, WF_DFU_RECEIVING, WF_DFU_VERIFIED, WF_DFU_INSTALLED } wf_dfu_phase_t;
typedef struct {
    wf_dfu_phase_t phase;
    uint32_t total, blocks, accepted, crc;
    wf_ota_ab_image_t image;
    /* Owned volatile storage is outside both Flash slots. A new session
     * always starts at packet 1; disconnected partial files cannot install. */
    uint8_t *file;
    size_t capacity;
    void *context;
    bool (*can_start)(void *);
    uint32_t (*crc32_mpeg2)(void *, const uint8_t *, size_t);
    bool (*install)(void *, const uint8_t *, const uint8_t *, const wf_ota_ab_image_t *);
    void (*verifying)(void *);
} wf_dfu_receiver_t;

void wf_dfu_disconnect(wf_dfu_receiver_t *);
uint8_t wf_dfu_percent(const wf_dfu_receiver_t *);
bool wf_dfu_is_cancel(const uint8_t *wire, size_t size);
/* Caller serializes requests with disconnect, supplies exactly one complete
 * SiFli serial message and returns the response on the same live connection. */
size_t wf_dfu_request(wf_dfu_receiver_t *, const uint8_t *, size_t, uint8_t response[12]);
/* Called by the sole worker after the verified file receipt was queued on BLE.
 * The phone may disconnect at that point. Only this local step can install. */
bool wf_dfu_install_received(wf_dfu_receiver_t *);
#endif
