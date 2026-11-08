#ifndef WF_OTA_AB_H
#define WF_OTA_AB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { WF_OTA_AB_BYTES = 128, WF_OTA_AB_NONE = 255 };
typedef enum { WF_OTA_AB_STABLE, WF_OTA_AB_READY, WF_OTA_AB_TRIED } wf_ota_ab_phase_t;
typedef enum { WF_OTA_AB_RECOVERY = -1, WF_OTA_AB_SLOT_A = 0, WF_OTA_AB_SLOT_B = 1 } wf_ota_ab_target_t;
typedef struct {
    uint32_t bytes, release;
    uint8_t sha256[32];
} wf_ota_ab_image_t;
typedef struct {
    uint32_t generation;
    uint8_t stable, pending, phase;
    wf_ota_ab_image_t image[2];
} wf_ota_ab_state_t;
typedef struct {
    /* Provisional intent only. Boot adapters must use wf_ota_ab_resolve(). */
    wf_ota_ab_target_t target, fallback;
    bool persist;
    wf_ota_ab_state_t next;
} wf_ota_ab_plan_t;

/* Pure model: no I/O, address constants, allocation or authentication code.
 * Caller must independently bound/read/hash/authenticate physical slot images.
 * checked[] contains those identities; an all-zero identity denotes invalid.
 * Missing/corrupt metadata never implicitly provisions or boots a slot. */
bool wf_ota_ab_provision(wf_ota_ab_state_t *, unsigned stable, const wf_ota_ab_image_t *checked);
bool wf_ota_ab_stage(wf_ota_ab_state_t *next, const wf_ota_ab_state_t *, const wf_ota_ab_image_t *checked_candidate);
bool wf_ota_ab_confirm(wf_ota_ab_state_t *next, const wf_ota_ab_state_t *, unsigned running,
                       const wf_ota_ab_image_t *checked_running);
bool wf_ota_ab_plan(wf_ota_ab_plan_t *, const wf_ota_ab_state_t *, const wf_ota_ab_image_t checked[2]);
/* A trial can be selected only after exact readback of plan.next's durable
 * record. A failed/stale readback falls back to the already verified stable
 * slot. The adapter must write the other erase unit, preserving the old one. */
wf_ota_ab_target_t wf_ota_ab_resolve(const wf_ota_ab_plan_t *, const uint8_t *readback, size_t);
bool wf_ota_ab_encode(const wf_ota_ab_state_t *, uint8_t out[WF_OTA_AB_BYTES]);
bool wf_ota_ab_decode(wf_ota_ab_state_t *, const uint8_t *, size_t);
/* Two records in separate erase units; highest nonwrapping generation wins.
 * A lone READY record is conservatively treated as TRIED: its missing peer
 * could have recorded a consumed trial before corruption or a power cut.
 * Equal contradictory generations, blank and unsupported records fail closed. */
int wf_ota_ab_select(wf_ota_ab_state_t *, const uint8_t *, size_t, const uint8_t *, size_t);
#endif
