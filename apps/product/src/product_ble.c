/* NUS transport based on SiFli SDK v2.5.1's Apache-2.0 peripheral example. */
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "bf0_ble_gap.h"
#include "bf0_sibles.h"
#include "bf0_sibles_internal.h"
#include "bf0_sibles_advertising.h"
#include "ble_connection_manager.h"
#include "phone_protocol.h"
#include "phone_queue.h"
#include "product_ble.h"
#include "product_services.h"
#include "product_pm.h"

#define WF_BLE_NAME "Bangle.js WristFlow"
#define SERIAL_UUID_16(x) {((uint8_t)((x) & 0xff)), ((uint8_t)((x) >> 8))}
#define NUS_UUID(n) {0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,n,0x00,0x40,0x6e}
enum { SVC, RX_CHAR, RX_VALUE, TX_CHAR, TX_VALUE, TX_CCCD, ATT_COUNT };
static rt_mq_t packets;
static wf_phone_sender_t sender;
static wf_phone_receiver_t receiver;
static struct rt_mutex phone_lock;
static wf_phone_t phone;
static sibles_hdl service;
static volatile uint8_t connection = 0xff, subscribed;
static volatile uint32_t generation;
static uint32_t pause_epoch;
static wf_radio_policy_t radio;
static uint32_t radio_now(void)
{ return (uint32_t)((uint64_t)rt_tick_get() * 1000U / RT_TICK_PER_SECOND); }
wf_radio_policy_t wristflow_product_ble_radio(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    wf_radio_policy_t value = radio;
    rt_hw_interrupt_enable(level);
    return value;
}
static uint8_t cccd[2];
static bool ready, gps_reply, weather_reply, weather_v2_attempted;
static uint32_t revision, alert_sequence;
static int32_t alert_id;
enum { NOTIFY_TRACE_CAPACITY = 32 };
typedef struct {
    wf_phone_op_t op;
    wf_phone_result_t result;
    int32_t id;
    uint32_t tick, generation, sequence, dropped, gaps, stale;
    unsigned count;
} notification_trace_t;
/* phone_lock owns this bounded RAM-only diagnostic; disabled after every boot. */
static notification_trace_t notify_trace[NOTIFY_TRACE_CAPACITY];
static unsigned notify_trace_total;
static bool notify_trace_enabled;
static void trace_notification(wf_phone_op_t op, int32_t id, wf_phone_result_t result, void *context)
{
    (void)context;
    if (!notify_trace_enabled) return;
    notification_trace_t *r = &notify_trace[notify_trace_total++ % NOTIFY_TRACE_CAPACITY];
    *r = (notification_trace_t){op, result, id, (uint32_t)rt_tick_get(), receiver.generation,
        receiver.sequence, wf_phone_queue_dropped(&sender), receiver.gaps, receiver.stale, phone.count};
}
static uint8_t nus_uuid[16] = NUS_UUID(1);
SIBLES_ADVERTISING_CONTEXT_DECLAR(advertising);
BLE_GATT_SERVICE_DEFINE_128(attributes) {
    BLE_GATT_SERVICE_DECLARE(SVC, SERIAL_UUID_16_PRI_SERVICE, BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_DECLARE(RX_CHAR, SERIAL_UUID_16_CHARACTERISTIC, BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(RX_VALUE, NUS_UUID(2),
        BLE_GATT_PERM_WRITE_REQ_ENABLE | BLE_GATT_PERM_WRITE_COMMAND_ENABLE,
        BLE_GATT_VALUE_PERM_UUID_128, 512),
    BLE_GATT_CHAR_DECLARE(TX_CHAR, SERIAL_UUID_16_CHARACTERISTIC, BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(TX_VALUE, NUS_UUID(3), BLE_GATT_PERM_NOTIFY_ENABLE,
        BLE_GATT_VALUE_PERM_UUID_128 | BLE_GATT_VALUE_PERM_RI_ENABLE, 512),
    BLE_GATT_DESCRIPTOR_DECLARE(TX_CCCD, SERIAL_UUID_16_CLIENT_CHAR_CFG,
        BLE_GATT_PERM_READ_ENABLE | BLE_GATT_PERM_WRITE_REQ_ENABLE, BLE_GATT_VALUE_PERM_RI_ENABLE, 2),
};

static bool enqueue(unsigned kind, const uint8_t *data, unsigned size, uint32_t request)
{
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t peer = generation, epoch = pause_epoch;
    rt_hw_interrupt_enable(level);
    return wf_phone_queue_send_epoch(&sender, packets, peer, epoch, kind, data, size, request);
}
static bool queue_weather(uint32_t request, void *context)
{
    (void)context;
    return wristflow_product_ble_is_connected() && enqueue(PACKET_TX_WEATHER_REQ, NULL, 0, request);
}
static uint32_t weather_peer(void *context) { (void)context; return generation; }
static uint8_t *read_value(uint8_t conn, uint8_t index, uint16_t *size)
{
    (void)conn; *size = 0;
    if (index == TX_CCCD) { *size = 2; return cccd; }
    return NULL;
}
static uint8_t write_value(uint8_t conn, sibles_set_cbk_t *v)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool allowed = conn == connection && radio.enabled;
    uint32_t peer = generation, epoch = pause_epoch;
    rt_hw_interrupt_enable(level);
    if (!allowed) return 0x0e;
    if (v->idx == RX_VALUE)
        return wf_phone_queue_send_epoch(&sender, packets, peer, epoch,
            PACKET_RX, v->value, v->len, 0) ? 0 : 0x11;
    if (v->idx == TX_CCCD) {
        if (v->len != 2 || v->value[1] || v->value[0] > 1) return 0x0d;
        level = rt_hw_interrupt_disable();
        if (peer != generation || epoch != pause_epoch || !radio.enabled || conn != connection) {
            rt_hw_interrupt_enable(level); return 0x0e;
        }
        cccd[0] = v->value[0]; cccd[1] = 0; subscribed = cccd[0];
        rt_hw_interrupt_enable(level);
        wf_phone_queue_send_epoch(&sender, packets, peer, epoch, PACKET_SUBSCRIBE, NULL, 0, 0);
    }
    return 0;
}
static uint8_t adv_event(uint8_t event, void *context, void *data)
{
    (void)context;
    rt_base_t level = rt_hw_interrupt_disable();
    if (event == SIBLES_ADV_EVT_ADV_STARTED) {
        bool success = ((sibles_adv_evt_startted_t *)data)->status == 0;
        wf_radio_adv(&radio, success ? WF_RADIO_STARTED : WF_RADIO_UNREADY, success, radio_now());
    } else if (event == SIBLES_ADV_EVT_ADV_STOPPED)
        wf_radio_adv(&radio, WF_RADIO_READY, true, radio_now());
    rt_hw_interrupt_enable(level);
    if (event == SIBLES_ADV_EVT_ADV_STARTED)
        rt_kprintf("[ble] advertising status=%u name=%s\n", ((sibles_adv_evt_startted_t *)data)->status, WF_BLE_NAME);
    enqueue(PACKET_RADIO, NULL, 0, 0);
    wristflow_product_event_send(WF_EVENT_RADIO);
    return 0;
}
/* The SDK weak default is NON_DISC, which phone discovery may filter out. */
uint8_t sibles_advertising_disc_mode_get(void)
{
    return GAPM_ADV_MODE_GEN_DISC;
}
static void start_advertising(void)
{
    BLE_GATT_SERVICE_INIT_128(svc, attributes, ATT_COUNT,
        BLE_GATT_SERVICE_PERM_NOAUTH | BLE_GATT_SERVICE_PERM_UUID_128, nus_uuid);
    service = sibles_register_svc_128(&svc);
    RT_ASSERT(service);
    sibles_register_cbk(service, read_value, write_value);
    sibles_advertising_para_t p = {0};
    p.own_addr_type = GAPM_STATIC_ADDR;
    p.config.adv_mode = SIBLES_ADV_CONNECT_MODE;
    p.config.mode_config.conn_config.interval = 0xa0;
    p.config.is_auto_restart = 0; /* Product owns restart and endurance suspension. */
    p.config.max_tx_pwr = 0x7f;
    p.evt_handler = adv_event;
    p.rsp_data.completed_name = rt_malloc(sizeof(sibles_adv_type_name_t) + strlen(WF_BLE_NAME));
    p.adv_data.completed_uuid = rt_malloc(sizeof(sibles_adv_type_srv_uuid_t) + sizeof(sibles_adv_uuid_t));
    ble_gap_dev_name_t *name = rt_malloc(sizeof *name + strlen(WF_BLE_NAME));
    RT_ASSERT(p.rsp_data.completed_name && p.adv_data.completed_uuid && name);
    name->len = strlen(WF_BLE_NAME); memcpy(name->name, WF_BLE_NAME, name->len);
    ble_gap_set_dev_name(name); rt_free(name);
    p.rsp_data.completed_name->name_len = strlen(WF_BLE_NAME);
    memcpy(p.rsp_data.completed_name->name, WF_BLE_NAME, strlen(WF_BLE_NAME));
    p.adv_data.completed_uuid->count = 1;
    p.adv_data.completed_uuid->uuid_list[0].uuid_len = 16;
    memcpy(p.adv_data.completed_uuid->uuid_list[0].uuid.uuid_128, nus_uuid, 16);
    uint8_t result = sibles_advertising_init(advertising, &p);
    rt_free(p.rsp_data.completed_name); rt_free(p.adv_data.completed_uuid);
    bd_addr_t address;
    if (ble_get_public_address(&address) == HL_ERR_NO_ERROR)
        rt_kprintf("[ble] address=%02X:%02X:%02X:%02X:%02X:%02X discoverable=general\n",
            address.addr[5], address.addr[4], address.addr[3], address.addr[2], address.addr[1], address.addr[0]);
    rt_kprintf("[ble] NUS registered; adv init=%u\n", result);
    rt_base_t level = rt_hw_interrupt_disable();
    wf_radio_adv(&radio, result == SIBLES_ADV_NO_ERR ? WF_RADIO_READY : WF_RADIO_UNREADY,
        result == SIBLES_ADV_NO_ERR, radio_now());
    rt_hw_interrupt_enable(level);
}
void wristflow_product_ble_enable(bool enabled)
{
    /* Serialize the effective pause edge with parser side effects. No SDK call
     * runs under this mutex or the IRQ guard. The epoch survives a lost wake. */
    if (!ready || wristflow_product_ble_radio().enabled == enabled) return;
    rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
    rt_base_t level = rt_hw_interrupt_disable();
    bool changed = radio.enabled != enabled;
    if (changed && !enabled) ++pause_epoch;
    wf_radio_enable(&radio, enabled, radio_now());
    rt_hw_interrupt_enable(level);
    rt_mutex_release(&phone_lock);
    if (changed) enqueue(PACKET_RADIO, NULL, 0, 0);
}
static void reconcile_radio(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool failed_before = radio.failed;
    wf_radio_action_t action = wf_radio_next(&radio, radio_now());
    uint32_t command_revision = action == WF_RADIO_DISCONNECT ? radio.link_revision : radio.adv_revision;
    uint8_t peer = connection;
    rt_hw_interrupt_enable(level);
    uint8_t result = 0;
    if (action == WF_RADIO_START) result = sibles_advertising_start(advertising);
    else if (action == WF_RADIO_STOP) result = sibles_advertising_stop(advertising);
    else if (action == WF_RADIO_DISCONNECT) {
        ble_gap_disconnect_t request = {.conn_idx = peer, .reason = CO_ERROR_REMOTE_USER_TERM_CON};
        result = ble_gap_disconnect(&request);
    }
    level = rt_hw_interrupt_disable();
    wf_radio_result(&radio, action, command_revision, result == 0);
    bool failed = radio.failed;
    rt_hw_interrupt_enable(level);
    if (action || failed != failed_before) {
        rt_kprintf("[ble-radio] action=%u result=%u failed=%u\n", action, result, failed);
        wristflow_product_event_send(WF_EVENT_RADIO);
    }
}
static bool protocol_current(uint32_t peer, uint32_t epoch)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool current = peer == generation && epoch == pause_epoch && radio.enabled;
    rt_hw_interrupt_enable(level);
    return current;
}
static bool transmit(const char *text, uint32_t peer, uint32_t epoch)
{
    size_t left = strlen(text);
    while (left && subscribed && connection != 0xff && protocol_current(peer, epoch)) {
        uint16_t n = (uint16_t)(left > 20 ? 20 : left); /* Works at the minimum ATT MTU. */
        sibles_value_t value = {0};
        value.hdl = service; value.idx = TX_VALUE; value.len = n; value.value = (uint8_t *)text;
        int result = 0;
        for (unsigned retry = 0; retry < 5; ++retry) {
            rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
            if (!subscribed || connection == 0xff || !protocol_current(peer, epoch)) {
                rt_mutex_release(&phone_lock); return false;
            }
            result = sibles_write_value(connection, &value);
            rt_mutex_release(&phone_lock);
            if (result != 0) break;
            rt_thread_mdelay(20);
        }
        if (result != n) { rt_kprintf("[ble] TX failed=%d\n", result); return false; }
        text += n; left -= n;
    }
    return left == 0;
}
static void send_weather(uint32_t request, uint32_t peer, uint32_t epoch)
{
    if (!wristflow_weather_request_pending(request)) return;
    weather_v2_attempted = true;
    if (!transmit("\r\n{\"t\":\"weather\",\"v\":2,\"f\":true}\r\n", peer, epoch))
        wristflow_weather_request_failed(request);
    wristflow_product_event_send(WF_EVENT_PHONE);
}
static void phone_event(wf_phone_event_t e, int32_t id, void *context)
{
    (void)context;
    if (e == WF_PHONE_NOTIFY) { ++revision; ++alert_sequence; alert_id = id; }
    else if (e == WF_PHONE_REMOVE) ++revision;
    if (e == WF_PHONE_TIME) {
        int result = wristflow_product_services_set_time(phone.utc);
        rt_kprintf("[ble] phone time UTC=%u result=%d\n", phone.utc, result);
    } else if (e == WF_PHONE_GPS_QUERY) gps_reply = true;
    else if (e == WF_PHONE_WEATHER) {
        wristflow_product_services_update_weather(&phone.weather, phone.weather_updates);
        if (phone.weather_version == 2) weather_v2_attempted = true;
        else if (!weather_v2_attempted) weather_reply = true;
    }
    else rt_kprintf("[ble] event=%u id=%ld count=%u rejected=%u\n", e, (long)id, phone.count, phone.rejected);
    wristflow_product_pm_phone_event();
}
static void worker(void *context)
{
    (void)context;
    wf_phone_packet_t p;
    for (;;) {
        reconcile_radio();
        if (wristflow_weather_poll()) wristflow_product_event_send(WF_EVENT_PHONE);
        uint32_t remaining = wristflow_weather_request_remaining_ms();
        /* At most three hours between tick-extension samples, including while off. */
        rt_int32_t wait = rt_tick_from_millisecond(remaining ? remaining : (uint32_t)WRISTFLOW_WEATHER_EXPIRY_MS);
        wf_radio_policy_t state = wristflow_product_ble_radio();
        if (!state.failed && !wf_radio_settled(&state)) wait = rt_tick_from_millisecond(100);
        if (rt_mq_recv(packets, &p, sizeof p, wait) != RT_EOK) continue;
        rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
        if (wf_phone_queue_barrier(&receiver, pause_epoch)) {
            wf_phone_gap(&phone); gps_reply = weather_reply = false;
            wristflow_weather_request_disconnected();
        }
        wf_phone_packet_action_t action = wf_phone_queue_receive_epoch(&receiver, &p, generation, pause_epoch);
        if (action == WF_PHONE_PACKET_STALE) {
            rt_mutex_release(&phone_lock);
            continue;
        }
        uint32_t peer = receiver.generation;
        if (action == WF_PHONE_PACKET_RECONNECT) {
            wf_phone_reconnect(&phone);
            weather_reply = weather_v2_attempted = false;
        }
        else if (action == WF_PHONE_PACKET_GAP) wf_phone_gap(&phone);
        if (p.kind == PACKET_POWER) {
            rt_mutex_release(&phone_lock);
            start_advertising();
            continue;
        }
        if (p.kind == PACKET_RADIO) {
            rt_mutex_release(&phone_lock);
            continue;
        }
        if (!wristflow_product_ble_radio().enabled) {
            gps_reply = weather_reply = false;
            rt_mutex_release(&phone_lock);
            continue;
        }
        if (p.generation == generation && p.kind == PACKET_RX) wf_phone_feed(&phone, p.data, p.size);
        if (p.generation == generation && p.kind == PACKET_SUBSCRIBE && subscribed) {
            wf_phone_clear_weather(&phone);
            wristflow_weather_reset();
        }
        if (p.kind == PACKET_LINK || p.kind == PACKET_SUBSCRIBE) ++revision;
        if (p.generation == generation &&
            ((p.kind == PACKET_LINK && connection == 0xff) ||
             (p.kind == PACKET_SUBSCRIBE && !subscribed)))
            wristflow_weather_request_disconnected();
        bool reply = gps_reply; gps_reply = false;
        bool forecast_reply = weather_reply; weather_reply = false;
        rt_mutex_release(&phone_lock);
        if (p.kind == PACKET_LINK || p.kind == PACKET_SUBSCRIBE) wristflow_product_event_send(WF_EVENT_PHONE);
        if (!protocol_current(p.generation, p.epoch)) continue;
        if (p.kind == PACKET_SUBSCRIBE && subscribed) {
            /* Gadgetbridge's Bangle.js line reader removes CR before LF. */
            transmit("\r\n{\"t\":\"ver\",\"fw\":\"WristFlow Notify B1\",\"hw\":\"Huangshan\"}\r\n", peer, p.epoch);
            send_weather(wristflow_weather_request_begin(NULL), peer, p.epoch);
            rt_kprintf("[ble] subscribed: reset weather cache and requested weather from phone\n");
        }
        if (p.kind == PACKET_TX_WEATHER_REQ) send_weather(p.request, peer, p.epoch);
        else if (forecast_reply && subscribed) send_weather(wristflow_weather_request_begin(NULL), peer, p.epoch);
        if (reply) transmit("\r\n{\"t\":\"gps_power\",\"status\":false}\r\n", peer, p.epoch);
    }
}

static int ble_event(uint16_t e, uint8_t *data, uint16_t len, uint32_t context)
{
    (void)len; (void)context;
    if (e == BLE_POWER_ON_IND) enqueue(PACKET_POWER, NULL, 0, 0);
    else if (e == BLE_GAP_CONNECTED_IND) {
        rt_base_t level = rt_hw_interrupt_disable();
        connection = ((ble_gap_connect_ind_t *)data)->conn_idx;
        ++generation; subscribed = 0; cccd[0] = 0;
        wf_radio_link(&radio, true, radio_now());
        rt_hw_interrupt_enable(level);
        enqueue(PACKET_LINK, NULL, 0, 0);
        rt_kprintf("[ble] connected index=%u generation=%u\n", connection, generation);
    } else if (e == BLE_GAP_DISCONNECTED_IND) {
        rt_kprintf("[ble] disconnected reason=%u\n", ((ble_gap_disconnected_ind_t *)data)->reason);
        rt_base_t level = rt_hw_interrupt_disable();
        connection = 0xff; subscribed = 0; cccd[0] = 0; ++generation;
        wf_radio_link(&radio, false, radio_now());
        rt_hw_interrupt_enable(level);
        enqueue(PACKET_LINK, NULL, 0, 0);
    } else if (e == SIBLES_MTU_EXCHANGE_IND)
        rt_kprintf("[ble] MTU=%u\n", ((sibles_mtu_exchange_ind_t *)data)->mtu);
    return 0;
}
BLE_EVENT_REGISTER(ble_event, NULL);
#ifndef NVDS_AUTO_UPDATE_MAC_ADDRESS_ENABLE
ble_common_update_type_t ble_request_public_address(bd_addr_t *addr)
{
    return bt_mac_addr_generate_via_uid_v2(addr) == 0 ? BLE_UPDATE_ONCE : BLE_UPDATE_NO_UPDATE;
}
#endif
void wristflow_product_ble_start(bool enabled)
{
    wf_radio_init(&radio, enabled);
    RT_ASSERT(rt_mutex_init(&phone_lock, "wf_phone", RT_IPC_FLAG_PRIO) == RT_EOK);
    wf_phone_init(&phone, phone_event, NULL);
    phone.trace = trace_notification;
    packets = rt_mq_create("wf_ble", sizeof(wf_phone_packet_t), WF_PHONE_QUEUE_DEPTH, RT_IPC_FLAG_FIFO);
    RT_ASSERT(packets);
    wristflow_weather_set_sender(queue_weather, NULL);
    wristflow_weather_set_peer(weather_peer, NULL);
    rt_thread_t task = rt_thread_create("wf_ble", worker, NULL, 8192, 23, 10);
    RT_ASSERT(task && rt_thread_startup(task) == RT_EOK);
    ready = true;
    sifli_ble_enable();
}
void wristflow_product_ble_snapshot(wristflow_notifications_snapshot_t *snapshot)
{
    rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
    snapshot->count = phone.count;
    memcpy(snapshot->messages, phone.messages, phone.count * sizeof phone.messages[0]);
    snapshot->revision = revision;
    snapshot->alert_sequence = alert_sequence;
    snapshot->alert_id = alert_id;
    snapshot->connected = connection != 0xff;
    snapshot->subscribed = subscribed != 0;
    rt_mutex_release(&phone_lock);
}

void wristflow_product_ble_delete(bool all, int32_t id, const wf_phone_metadata_t *expected, void *context)
{
    (void)context;
    rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
    if (all) wf_phone_clear(&phone);
    else wf_phone_remove_group(&phone, id, expected);
    ++revision;
    rt_mutex_release(&phone_lock);
    /* Deliberately no NUS transmit: watch deletions are local only. */
    wristflow_product_event_send(WF_EVENT_PHONE);
}

bool wristflow_product_ble_is_connected(void)
{
    return connection != 0xff && subscribed && wristflow_product_ble_radio().enabled;
}

bool wristflow_product_ble_request_weather(void)
{
    rt_kprintf("[ble] requesting weather from phone\n");
    bool queued = wristflow_weather_request_sync();
    wristflow_product_event_send(WF_EVENT_PHONE);
    return queued;
}

/* Serial diagnostics are deliberate: routine logs never include notification bodies. */
static int wf_ble(int argc, char **argv)
{
    if (!ready) return -RT_ERROR;
    if (argc == 2 && !strcmp(argv[1], "radio")) {
        wf_radio_policy_t state = wristflow_product_ble_radio();
        rt_kprintf("[ble-radio] enabled=%u adv=%u connected=%u disconnecting=%u suspended=%u settled=%u failed=%u; link/advertising, not stack power\n",
            state.enabled, state.adv, state.connected, state.disconnecting, wf_radio_suspended(&state), wf_radio_settled(&state), state.failed);
        return 0;
    }
    if (argc < 2) {
        rt_kprintf("wf_ble status | ids | meta | list | clear | clear_weather | weather | remove <id>\n");
        rt_kprintf("wf_ble trace on | off | dump\n"); return -RT_EINVAL;
    }
    rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
    int result = RT_EOK;
    if (!strcmp(argv[1], "meta") && argc == 2) {
        /* FinSH owns a compact static copy; never print while holding phone_lock.
         * Only equality and time are exposed, not keys, packages or message text. */
        static struct {
            uint64_t when_ms;
            int32_t id;
            char key[33];
            bool valid, when_valid;
        } snapshot[WF_PHONE_MESSAGES];
        unsigned count = phone.count;
        uint32_t version = revision;
        for (unsigned i = 0; i < count; ++i) {
            snapshot[i].id = phone.messages[i].id;
            snapshot[i].when_ms = phone.messages[i].metadata.when_ms;
            snapshot[i].valid = phone.messages[i].metadata.valid;
            snapshot[i].when_valid = phone.messages[i].metadata.when_valid;
            memcpy(snapshot[i].key, phone.messages[i].metadata.notification_key, sizeof snapshot[i].key);
        }
        rt_mutex_release(&phone_lock);
        rt_kprintf("[ble] meta count=%u revision=%u\n", count, version);
        for (unsigned i = 0; i < count; ++i) {
            int same = -1;
            for (unsigned j = 0; snapshot[i].valid && j < i; ++j)
                if (snapshot[j].valid && !strcmp(snapshot[i].key, snapshot[j].key)) { same = (int)j; break; }
            rt_kprintf("[ble] meta slot=%u id=%ld valid=%u same_slot=%d when_valid=%u\n",
                i, (long)snapshot[i].id, snapshot[i].valid, same, snapshot[i].when_valid);
            /* Two uint32 limbs avoid depending on RT-Thread long-long printf support. */
            rt_kprintf("[ble] meta slot=%u when_hi=%lu when_lo=%lu\n", i,
                (unsigned long)(snapshot[i].when_ms >> 32), (unsigned long)(uint32_t)snapshot[i].when_ms);
        }
        return RT_EOK;
    }
    if (!strcmp(argv[1], "ids") || !strcmp(argv[1], "trace")) {
        if (!strcmp(argv[1], "ids") && argc == 2) {
            rt_kprintf("[ble] ids count=%u revision=%u\n", phone.count, revision);
            for (unsigned i = 0; i < phone.count; ++i)
                rt_kprintf("[ble] slot=%u id=%ld\n", i, (long)phone.messages[i].id);
        } else if (!strcmp(argv[1], "trace") && argc == 3 && !strcmp(argv[2], "on")) {
            notify_trace_total = 0; notify_trace_enabled = true;
            rt_kprintf("[ble] trace enabled capacity=%u\n", NOTIFY_TRACE_CAPACITY);
        } else if (!strcmp(argv[1], "trace") && argc == 3 && !strcmp(argv[2], "off")) {
            notify_trace_enabled = false;
            rt_kprintf("[ble] trace disabled total=%u\n", notify_trace_total);
        } else if (!strcmp(argv[1], "trace") && argc == 3 && !strcmp(argv[2], "dump")) {
            static const char *ops[] = {"notify", "notify~", "notify-"};
            static const char *results[] = {"add", "update", "remove", "miss", "invalid-id", "invalid-fields"};
            /* FinSH owns this output snapshot; UART printing must not block the BLE owner. */
            static notification_trace_t snapshot[NOTIFY_TRACE_CAPACITY];
            unsigned total = notify_trace_total;
            bool enabled = notify_trace_enabled;
            memcpy(snapshot, notify_trace, sizeof snapshot);
            rt_mutex_release(&phone_lock);
            unsigned first = total > NOTIFY_TRACE_CAPACITY ? total - NOTIFY_TRACE_CAPACITY : 0;
            rt_kprintf("[ble] trace enabled=%u total=%u overwritten=%u\n", enabled, total, first);
            for (unsigned n = first; n < total; ++n) {
                const notification_trace_t *r = &snapshot[n % NOTIFY_TRACE_CAPACITY];
                rt_kprintf("[ble] trace n=%u op=%s id=%ld valid=%u result=%s count=%u tick=%u\n",
                    n + 1, ops[r->op], (long)r->id, r->result != WF_PHONE_INVALID_ID,
                    results[r->result], r->count, r->tick);
                rt_kprintf("[ble] trace n=%u gen=%u seq=%u dropped=%u gaps=%u stale=%u\n",
                    n + 1, r->generation, r->sequence, r->dropped, r->gaps, r->stale);
            }
            return RT_EOK;
        } else result = -RT_EINVAL;
        rt_mutex_release(&phone_lock);
        return result; /* Queries and trace controls do not send a UI/PM event. */
    }
    if (!strcmp(argv[1], "weather")) {
        bool sent = wristflow_product_ble_request_weather();
        rt_kprintf("[ble] weather request queued=%u\n", sent);
    } else if (!strcmp(argv[1], "clear_weather")) {
        wf_phone_clear_weather(&phone);
        wristflow_weather_reset();
        rt_kprintf("[ble] cleared weather cache\n");
    } else if (!strcmp(argv[1], "status")) {
        /* Keep each line within the SDK's 128-byte console buffer. */
        rt_kprintf("[ble] A1 connected=%u subscribed=%u generation=%u messages=%u\n",
            connection != 0xff, subscribed, generation, phone.count);
        rt_kprintf("[ble] stats added=%u updated=%u removed=%u rejected=%u unknown=%u\n",
            phone.received, phone.updated, phone.removed, phone.rejected, phone.unknown);
        rt_kprintf("[ble] queue dropped=%u gaps=%u stale=%u\n",
            wf_phone_queue_dropped(&sender), receiver.gaps, receiver.stale);
        wristflow_weather_data_t weather;
        wristflow_weather_state_t state;
        bool cached = wristflow_weather_get_current(&weather, &state);
        rt_kprintf("[ble] weather cached=%u state=%u expired=%u age_known=%u received_utc=%u request=%u remaining_ms=%u\n",
            cached, state, weather.expired, weather.age_known, weather.received_utc, weather.request_state,
            wristflow_weather_request_remaining_ms());
        rt_kprintf("[ble] weather aqi_valid=%u aqi=%s index=%s\n",
            phone.weather.aqi_valid, weather.aqi, weather.indices[0].value);
    } else if (!strcmp(argv[1], "list")) {
        for (unsigned i = 0; i < phone.count; ++i)
            rt_kprintf("[ble] id=%ld src=%s title=%s body=%s\n", (long)phone.messages[i].id,
                phone.messages[i].source, phone.messages[i].title, phone.messages[i].body);
    } else if (!strcmp(argv[1], "clear")) { wf_phone_clear(&phone); ++revision; }
    else if (!strcmp(argv[1], "remove") && argc == 3) {
        char *end; errno = 0; long id = strtol(argv[2], &end, 10);
        if (errno || *end || end == argv[2] || id < INT32_MIN || id > INT32_MAX) result = -RT_EINVAL;
        else { wf_phone_remove(&phone, (int32_t)id); ++revision; }
    } else result = -RT_EINVAL;
    rt_mutex_release(&phone_lock);
    wristflow_product_event_send(WF_EVENT_PHONE);
    return result;
}
MSH_CMD_EXPORT(wf_ble, Product phone link diagnostics);
