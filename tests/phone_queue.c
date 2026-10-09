#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "phone_queue.h"
#include "phone_protocol.h"

struct test_queue {
    wf_phone_packet_t packets[WF_PHONE_QUEUE_DEPTH];
    unsigned head, count;
};
static struct test_queue queue;
static wf_phone_sender_t sender;
static wf_phone_receiver_t receiver;
static wf_phone_t phone;
static rt_base_t masked;
static void (*copy_hook)(void), (*send_hook)(void), (*deferred)(void);
static const char prefix[] = "GB({\"t\":\"notify\",\"id\":42,\"body\":\"";
static const char suffix[] = "ok\"})\n";
static const char complete[] = "GB({\"t\":\"notify\",\"id\":43,\"body\":\"recovered\"})\n";

rt_base_t rt_hw_interrupt_disable(void)
{
    rt_base_t previous = masked;
    masked = 1;
    return previous;
}
void rt_hw_interrupt_enable(rt_base_t level)
{
    assert(masked);
    masked = level;
    if (!masked && deferred) {
        void (*run)(void) = deferred;
        deferred = NULL;
        run();
    }
}
void *rt_memcpy(void *destination, const void *source, size_t size)
{
    assert(!masked || !copy_hook);
    memcpy(destination, source, size);
    if (copy_hook) {
        void (*run)(void) = copy_hook;
        copy_hook = NULL;
        run();
    }
    return destination;
}
int rt_mq_send(rt_mq_t q, const void *buffer, size_t size)
{
    assert(masked && size == sizeof(wf_phone_packet_t));
    if (send_hook) {
        assert(!deferred);
        deferred = send_hook;
        send_hook = NULL;
    }
    if (q->count == WF_PHONE_QUEUE_DEPTH) return -RT_EFULL;
    memcpy(&q->packets[(q->head + q->count) % WF_PHONE_QUEUE_DEPTH], buffer, size);
    ++q->count;
    return RT_EOK;
}
static void reset(void)
{
    assert(!masked && !copy_hook && !send_hook && !deferred);
    memset(&queue, 0, sizeof queue);
    memset(&sender, 0, sizeof sender);
    memset(&receiver, 0, sizeof receiver);
    wf_phone_init(&phone, NULL, NULL);
}
static bool send_text(uint32_t peer, const char *text)
{
    return wf_phone_queue_send(&sender, &queue, peer, PACKET_RX,
                               (const uint8_t *)text, (unsigned)strlen(text), 0);
}
static wf_phone_packet_action_t consume(uint32_t peer)
{
    assert(queue.count);
    wf_phone_packet_t *packet = &queue.packets[queue.head];
    wf_phone_packet_action_t action = wf_phone_queue_receive(&receiver, packet, peer);
    if (action == WF_PHONE_PACKET_RECONNECT) wf_phone_reconnect(&phone);
    else if (action == WF_PHONE_PACKET_GAP) wf_phone_gap(&phone);
    if (action != WF_PHONE_PACKET_STALE && packet->kind == PACKET_RX)
        wf_phone_feed(&phone, packet->data, packet->size);
    queue.head = (queue.head + 1) % WF_PHONE_QUEUE_DEPTH;
    --queue.count;
    return action;
}
static void producer_b(void)
{
    assert(!masked);
    assert(wf_phone_queue_send(&sender, &queue, 1, PACKET_TX_WEATHER_REQ, NULL, 0, 22));
}
static void new_peer_producer(void) { assert(send_text(2, prefix)); }

static void test_producer_order(void)
{
    reset();
    copy_hook = producer_b;
    assert(send_text(1, prefix));
    assert(queue.count == 2 && queue.packets[0].request == 22);
    assert(queue.packets[0].sequence == 1 && queue.packets[1].sequence == 2);
    assert(consume(1) == WF_PHONE_PACKET_RECONNECT);
    assert(consume(1) == WF_PHONE_PACKET_CONTINUE);
    assert(send_text(1, suffix));
    assert(consume(1) == WF_PHONE_PACKET_CONTINUE && phone.count == 1);
    assert(!receiver.gaps && !wf_phone_queue_dropped(&sender));

    reset();
    send_hook = producer_b;
    assert(wf_phone_queue_send(&sender, &queue, 1, PACKET_TX_WEATHER_REQ, NULL, 0, 11));
    assert(queue.count == 2 && queue.packets[0].request == 11 && queue.packets[1].request == 22);
    assert(queue.packets[0].sequence == 1 && queue.packets[1].sequence == 2);
    assert(!masked);
    /* Saving/restoring IRQ state must not enable a caller's existing mask. */
    masked = 1;
    assert(wf_phone_queue_send(&sender, &queue, 1, PACKET_LINK, NULL, 0, 0));
    assert(masked == 1 && wf_phone_queue_dropped(&sender) == 0 && masked == 1);
    rt_hw_interrupt_enable(0);
}
static void test_full_queue_loss(void)
{
    reset();
    assert(send_text(1, prefix));
    assert(consume(1) == WF_PHONE_PACKET_RECONNECT && phone.used);
    for (unsigned i = 0; i < WF_PHONE_QUEUE_DEPTH; ++i)
        assert(wf_phone_queue_send(&sender, &queue, 1, PACKET_TX_WEATHER_REQ, NULL, 0, i));
    assert(!send_text(1, "lost"));
    assert(queue.count == WF_PHONE_QUEUE_DEPTH && wf_phone_queue_dropped(&sender) == 1);
    while (queue.count) assert(consume(1) == WF_PHONE_PACKET_CONTINUE);
    assert(send_text(1, suffix));
    assert(consume(1) == WF_PHONE_PACKET_GAP && !phone.count && !phone.used && !phone.dropping);
    assert(receiver.gaps == 1);
    assert(send_text(1, complete));
    assert(consume(1) == WF_PHONE_PACKET_CONTINUE && phone.count == 1 && phone.messages[0].id == 43);
    /* A failed control packet conservatively marks loss in the shared stream. */
    assert(send_text(1, prefix)); consume(1);
    assert(!wf_phone_queue_send(&sender, NULL, 1, PACKET_TX_WEATHER_REQ, NULL, 0, 55));
    assert(send_text(1, suffix));
    assert(consume(1) == WF_PHONE_PACKET_GAP && phone.count == 1 && receiver.gaps == 2);
}
static void test_stale_and_generation(void)
{
    reset();
    /* An old producer paused during copying resumes after new-peer data. */
    copy_hook = new_peer_producer;
    assert(send_text(1, "ignored old peer\n"));
    assert(queue.packets[0].generation == 2 && queue.packets[1].generation == 1);
    assert(consume(2) == WF_PHONE_PACKET_RECONNECT);
    size_t used = phone.used;
    assert(consume(2) == WF_PHONE_PACKET_STALE && phone.used == used && receiver.generation == 2);
    assert(send_text(2, suffix));
    assert(consume(2) == WF_PHONE_PACKET_CONTINUE && phone.count == 1 && !receiver.gaps);
    assert(receiver.stale == 1);

    reset();
    assert(send_text(2, prefix)); consume(2);
    assert(!wf_phone_queue_send(&sender, NULL, 2, PACKET_RX, NULL, 0, 0));
    assert(send_text(1, "old\n"));
    assert(consume(2) == WF_PHONE_PACKET_STALE && receiver.pending_gap && phone.used);
    assert(send_text(2, suffix));
    assert(consume(2) == WF_PHONE_PACKET_GAP && !phone.count && receiver.gaps == 1);
    assert(send_text(2, complete)); consume(2); assert(phone.count == 1);

    assert(send_text(2, prefix)); consume(2); assert(phone.used);
    assert(send_text(3, suffix));
    assert(consume(3) == WF_PHONE_PACKET_RECONNECT && phone.count == 1 && !phone.used);
    assert(send_text(3, complete)); consume(3); assert(phone.count == 1);
    /* A new peer does not inherit an old peer's pending loss marker. */
    assert(!wf_phone_queue_send(&sender, NULL, 3, PACKET_RX, NULL, 0, 0));
    assert(send_text(2, "old\n")); consume(3); assert(receiver.pending_gap);
    assert(send_text(4, complete));
    assert(consume(4) == WF_PHONE_PACKET_RECONNECT && !receiver.pending_gap && !phone.dropping);
}
static void test_bounds_and_wrap(void)
{
    reset();
    uint8_t payload[WF_PHONE_PACKET_BYTES];
    memset(payload, 0xa5, sizeof payload);
    assert(wf_phone_queue_send(&sender, &queue, 1, PACKET_RX, payload, sizeof payload, 7));
    assert(queue.packets[0].size == sizeof payload && !memcmp(queue.packets[0].data, payload, sizeof payload));
    assert(!wf_phone_queue_send(&sender, &queue, 1, PACKET_RX, payload, sizeof payload + 1, 0));
    assert(!wf_phone_queue_send(&sender, &queue, 1, PACKET_RX, NULL, 1, 0));
    assert(!wf_phone_queue_send(&sender, NULL, 1, PACKET_LINK, NULL, 0, 0));
    assert(sender.sequence == 4 && wf_phone_queue_dropped(&sender) == 3 && queue.count == 1);
    reset();
    sender.sequence = UINT32_MAX - 1;
    receiver.sequence = UINT32_MAX - 1;
    receiver.generation = 1;
    receiver.has_peer = true;
    assert(send_text(1, prefix));
    assert(consume(1) == WF_PHONE_PACKET_CONTINUE && sender.sequence == UINT32_MAX);
    assert(send_text(1, suffix));
    assert(consume(1) == WF_PHONE_PACKET_CONTINUE && sender.sequence == 0 && phone.count == 1);
    reset();
    assert(send_text(UINT32_MAX, prefix)); consume(UINT32_MAX); assert(phone.used);
    assert(send_text(0, complete));
    assert(consume(0) == WF_PHONE_PACKET_RECONNECT && receiver.generation == 0);
    assert(phone.count == 1 && phone.messages[0].id == 43);
    assert(send_text(UINT32_MAX, "delayed\n"));
    assert(consume(0) == WF_PHONE_PACKET_STALE && receiver.generation == 0);
}
int main(void)
{
    assert(sizeof(wf_phone_packet_t) == 528);
    test_producer_order();
    test_full_queue_loss();
    test_stale_and_generation();
    test_bounds_and_wrap();
    puts("phone_queue: producer interleaving, overflow, stale peers, parser recovery and wrap PASS");
    return 0;
}
