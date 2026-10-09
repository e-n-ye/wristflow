#ifndef WRISTFLOW_PHONE_QUEUE_H
#define WRISTFLOW_PHONE_QUEUE_H
#include <rtthread.h>
#include <stdbool.h>
#include <stdint.h>

enum { PACKET_POWER = 1, PACKET_LINK, PACKET_RX, PACKET_SUBSCRIBE, PACKET_TX_WEATHER_REQ };
enum { WF_PHONE_QUEUE_DEPTH = 32, WF_PHONE_PACKET_BYTES = 512 };
typedef struct {
    uint32_t sequence, generation, request;
    uint16_t kind, size;
    uint8_t data[WF_PHONE_PACKET_BYTES];
} wf_phone_packet_t;
typedef struct { uint32_t sequence, dropped; } wf_phone_sender_t;
typedef struct {
    uint32_t sequence, generation, gaps, stale;
    bool has_peer, pending_gap;
} wf_phone_receiver_t;
typedef enum {
    WF_PHONE_PACKET_CONTINUE, WF_PHONE_PACKET_RECONNECT,
    WF_PHONE_PACKET_GAP, WF_PHONE_PACKET_STALE
} wf_phone_packet_action_t;

/* Capture peer before calling. Copies the payload before the IRQ guard; on the
 * single HCPU, allocation and non-waiting MQ insertion share one IRQ guard.
 * Failed attempts also reserve a sequence, conservatively marking stream loss. */
bool wf_phone_queue_send(wf_phone_sender_t *sender, rt_mq_t queue, uint32_t peer,
                         unsigned kind, const uint8_t *data, unsigned size, uint32_t request);
uint32_t wf_phone_queue_dropped(const wf_phone_sender_t *sender);
/* Worker-owned. Every consumed packet advances global queue order. Stale peers
 * preserve the active partial frame and defer any real gap to its next packet. */
wf_phone_packet_action_t wf_phone_queue_receive(wf_phone_receiver_t *receiver,
                                               const wf_phone_packet_t *packet, uint32_t current_peer);
#endif
