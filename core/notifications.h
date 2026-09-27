#ifndef WRISTFLOW_NOTIFICATIONS_H
#define WRISTFLOW_NOTIFICATIONS_H
#include <stdbool.h>
#include <stdint.h>

#define WF_PHONE_MESSAGES 10
typedef struct {
    int32_t id;
    char source[161], title[321], body[1601];
} wf_phone_message_t;
/* Bounded copy across the phone/UI ownership boundary. Revision includes local
 * deletions; alert_sequence advances only for accepted incoming notifications. */
typedef struct {
    wf_phone_message_t messages[WF_PHONE_MESSAGES];
    unsigned count;
    uint32_t revision, alert_sequence;
    int32_t alert_id;
    bool connected, subscribed;
} wristflow_notifications_snapshot_t;
typedef void (*wristflow_notification_delete_cb_t)(bool all, int32_t id, void *context);
#endif
