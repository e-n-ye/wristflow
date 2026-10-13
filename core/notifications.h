#ifndef WRISTFLOW_NOTIFICATIONS_H
#define WRISTFLOW_NOTIFICATIONS_H
#include <stdbool.h>
#include <stdint.h>

#define WF_PHONE_MESSAGES 10
/* Optional Android notification lifecycle metadata, not a contact/message ID.
 * Times are original Unix milliseconds, not synthesized from watch reception. */
typedef struct {
    uint64_t when_ms;
    char notification_key[33], package[256];
    bool valid, when_valid;
} wf_phone_metadata_t;
typedef struct {
    int32_t id;
    char source[161], title[321], body[1601];
    wf_phone_metadata_t metadata;
} wf_phone_message_t;
/* Valid lifecycle key AND package group different protocol IDs. Legacy or
 * incomplete metadata stays independent. Call on an owned/locked snapshot. */
bool wf_notification_same_group(const wf_phone_message_t *a, const wf_phone_message_t *b);
bool wf_notification_group_key_equal(const wf_phone_metadata_t *a, const wf_phone_metadata_t *b);
unsigned wf_notification_history(const wf_phone_message_t *messages, unsigned count,
    const wf_phone_message_t *anchor, unsigned indices[WF_PHONE_MESSAGES]);
unsigned wf_notification_groups(const wf_phone_message_t *messages, unsigned count,
    unsigned latest[WF_PHONE_MESSAGES]);
/* Bounded copy across the phone/UI ownership boundary. Revision includes local
 * deletions; alert_sequence advances only for accepted incoming notifications. */
typedef struct {
    wf_phone_message_t messages[WF_PHONE_MESSAGES];
    unsigned count;
    uint32_t revision, alert_sequence;
    int32_t alert_id;
    bool connected, subscribed;
} wristflow_notifications_snapshot_t;
typedef void (*wristflow_notification_delete_cb_t)(bool all, int32_t id,
    const wf_phone_metadata_t *expected, void *context);
#endif
