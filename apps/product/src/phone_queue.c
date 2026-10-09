#include "phone_queue.h"
#include <rthw.h>

bool wf_phone_queue_send(wf_phone_sender_t *sender, rt_mq_t queue, uint32_t peer,
                         unsigned kind, const uint8_t *data, unsigned size, uint32_t request)
{
    wf_phone_packet_t packet = {0};
    packet.generation = peer;
    packet.kind = (uint16_t)kind;
    packet.request = request;
    bool valid = size <= sizeof packet.data && (!size || data);
    if (valid) {
        packet.size = (uint16_t)size;
        if (size) rt_memcpy(packet.data, data, size);
    }
    rt_base_t level = rt_hw_interrupt_disable();
    packet.sequence = ++sender->sequence;
    /* RT-Thread copies the full packet here and may request a context switch;
     * PRIMASK defers that switch until insertion and counters are complete. */
    bool sent = valid && queue && rt_mq_send(queue, &packet, sizeof packet) == RT_EOK;
    if (!sent) ++sender->dropped;
    rt_hw_interrupt_enable(level);
    return sent;
}

uint32_t wf_phone_queue_dropped(const wf_phone_sender_t *sender)
{
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t dropped = sender->dropped;
    rt_hw_interrupt_enable(level);
    return dropped;
}

wf_phone_packet_action_t wf_phone_queue_receive(wf_phone_receiver_t *receiver,
                                               const wf_phone_packet_t *packet, uint32_t current_peer)
{
    bool gap = packet->sequence != receiver->sequence + UINT32_C(1);
    receiver->sequence = packet->sequence;
    if (packet->generation != current_peer) {
        receiver->pending_gap |= gap;
        ++receiver->stale;
        return WF_PHONE_PACKET_STALE;
    }
    if (!receiver->has_peer || packet->generation != receiver->generation) {
        receiver->has_peer = true;
        receiver->generation = packet->generation;
        receiver->pending_gap = false;
        return WF_PHONE_PACKET_RECONNECT;
    }
    if (gap || receiver->pending_gap) {
        receiver->pending_gap = false;
        ++receiver->gaps;
        return WF_PHONE_PACKET_GAP;
    }
    return WF_PHONE_PACKET_CONTINUE;
}
