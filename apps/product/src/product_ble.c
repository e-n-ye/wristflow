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
#include "product_ble.h"
#include "product_services.h"
#include "product_pm.h"

#define WF_BLE_NAME "Bangle.js WristFlow"
#define SERIAL_UUID_16(x) {((uint8_t)((x) & 0xff)), ((uint8_t)((x) >> 8))}
#define NUS_UUID(n) {0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,n,0x00,0x40,0x6e}
enum { SVC, RX_CHAR, RX_VALUE, TX_CHAR, TX_VALUE, TX_CCCD, ATT_COUNT };
enum { PACKET_POWER = 1, PACKET_LINK, PACKET_RX, PACKET_SUBSCRIBE, PACKET_TX_WEATHER_REQ };
typedef struct {

    uint32_t sequence, generation, request;
    uint16_t kind, size;
    uint8_t data[512];
} packet_t;
static rt_mq_t packets;
static struct rt_mutex phone_lock;
static wf_phone_t phone;
static sibles_hdl service;
static volatile uint8_t connection = 0xff, subscribed;
static volatile uint32_t generation, sequence, dropped;
static uint8_t cccd[2];
static bool ready, gps_reply, weather_reply, weather_v2_attempted;
static uint32_t revision, alert_sequence;
static int32_t alert_id;
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
    packet_t p = {0};
    p.kind = (uint16_t)kind; p.generation = generation; p.sequence = ++sequence;
    p.request = request;
    if (size > sizeof p.data || !packets) { dropped++; return false; }
    p.size = (uint16_t)size;
    if (size) memcpy(p.data, data, size);
    if (rt_mq_send(packets, &p, sizeof p) != RT_EOK) { dropped++; return false; }
    return true;
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
    if (conn != connection) return 0x0e;
    if (v->idx == RX_VALUE)
        return enqueue(PACKET_RX, v->value, v->len, 0) ? 0 : 0x11;
    if (v->idx == TX_CCCD) {
        if (v->len != 2 || v->value[1] || v->value[0] > 1) return 0x0d;
        cccd[0] = v->value[0]; cccd[1] = 0; subscribed = cccd[0];
        enqueue(PACKET_SUBSCRIBE, NULL, 0, 0);
    }
    return 0;
}
static uint8_t adv_event(uint8_t event, void *context, void *data)
{
    (void)context;
    if (event == SIBLES_ADV_EVT_ADV_STARTED)
        rt_kprintf("[ble] advertising status=%u name=%s\n", ((sibles_adv_evt_startted_t *)data)->status, WF_BLE_NAME);
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
    p.config.is_auto_restart = 1;
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
    if (result == SIBLES_ADV_NO_ERR) sibles_advertising_start(advertising);
}
static bool transmit(const char *text, uint32_t peer)
{
    size_t left = strlen(text);
    while (left && subscribed && connection != 0xff && peer == generation) {
        uint16_t n = (uint16_t)(left > 20 ? 20 : left); /* Works at the minimum ATT MTU. */
        sibles_value_t value = {0};
        value.hdl = service; value.idx = TX_VALUE; value.len = n; value.value = (uint8_t *)text;
        int result = 0;
        for (unsigned retry = 0; retry < 5; ++retry) {
            if (!subscribed || connection == 0xff || peer != generation) return false;
            result = sibles_write_value(connection, &value);
            if (result != 0) break;
            rt_thread_mdelay(20);
        }
        if (result != n) { rt_kprintf("[ble] TX failed=%d\n", result); return false; }
        text += n; left -= n;
    }
    return left == 0;
}
static void send_weather(uint32_t request, uint32_t peer)
{
    if (!wristflow_weather_request_pending(request)) return;
    weather_v2_attempted = true;
    if (!transmit("\r\n{\"t\":\"weather\",\"v\":2,\"f\":true}\r\n", peer))
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
    packet_t p;
    uint32_t last_sequence = 0, peer = 0;
    for (;;) {
        if (wristflow_weather_poll()) wristflow_product_event_send(WF_EVENT_PHONE);
        uint32_t remaining = wristflow_weather_request_remaining_ms();
        /* At most three hours between tick-extension samples, including while off. */
        rt_int32_t wait = rt_tick_from_millisecond(remaining ? remaining : (uint32_t)WRISTFLOW_WEATHER_EXPIRY_MS);
        if (rt_mq_recv(packets, &p, sizeof p, wait) != RT_EOK) continue;
        if (p.kind == PACKET_POWER) { start_advertising(); last_sequence = p.sequence; continue; }
        rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
        if (p.generation != peer) {
            wf_phone_reconnect(&phone); peer = p.generation;
            weather_reply = weather_v2_attempted = false;
        }
        else if (p.sequence != last_sequence + 1) wf_phone_gap(&phone);
        last_sequence = p.sequence;
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
        if (p.generation != generation) continue;
        if (p.kind == PACKET_SUBSCRIBE && subscribed) {
            /* Gadgetbridge's Bangle.js line reader removes CR before LF. */
            transmit("\r\n{\"t\":\"ver\",\"fw\":\"WristFlow Notify B1\",\"hw\":\"Huangshan\"}\r\n", peer);
            send_weather(wristflow_weather_request_begin(NULL), peer);
            rt_kprintf("[ble] subscribed: reset weather cache and requested weather from phone\n");
        }
        if (p.kind == PACKET_TX_WEATHER_REQ) send_weather(p.request, peer);
        else if (forecast_reply && subscribed) send_weather(wristflow_weather_request_begin(NULL), peer);
        if (reply) transmit("\r\n{\"t\":\"gps_power\",\"status\":false}\r\n", peer);
    }
}

static int ble_event(uint16_t e, uint8_t *data, uint16_t len, uint32_t context)
{
    (void)len; (void)context;
    if (e == BLE_POWER_ON_IND) enqueue(PACKET_POWER, NULL, 0, 0);
    else if (e == BLE_GAP_CONNECTED_IND) {
        connection = ((ble_gap_connect_ind_t *)data)->conn_idx;
        ++generation; subscribed = 0; cccd[0] = 0;
        enqueue(PACKET_LINK, NULL, 0, 0);
        rt_kprintf("[ble] connected index=%u generation=%u\n", connection, generation);
    } else if (e == BLE_GAP_DISCONNECTED_IND) {
        rt_kprintf("[ble] disconnected reason=%u\n", ((ble_gap_disconnected_ind_t *)data)->reason);
        connection = 0xff; subscribed = 0; cccd[0] = 0; ++generation;
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
void wristflow_product_ble_start(void)
{
    RT_ASSERT(rt_mutex_init(&phone_lock, "wf_phone", RT_IPC_FLAG_PRIO) == RT_EOK);
    wf_phone_init(&phone, phone_event, NULL);
    packets = rt_mq_create("wf_ble", sizeof(packet_t), 32, RT_IPC_FLAG_FIFO);
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

void wristflow_product_ble_delete(bool all, int32_t id, void *context)
{
    (void)context;
    rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
    if (all) wf_phone_clear(&phone);
    else wf_phone_remove(&phone, id);
    ++revision;
    rt_mutex_release(&phone_lock);
    /* Deliberately no NUS transmit: watch deletions are local only. */
    wristflow_product_event_send(WF_EVENT_PHONE);
}

bool wristflow_product_ble_is_connected(void)
{
    return connection != 0xff && subscribed;
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
    if (argc < 2) { rt_kprintf("wf_ble status | list | clear | clear_weather | weather | remove <id>\n"); return -RT_EINVAL; }
    rt_mutex_take(&phone_lock, RT_WAITING_FOREVER);
    int result = RT_EOK;
    if (!strcmp(argv[1], "weather")) {
        bool sent = wristflow_product_ble_request_weather();
        rt_kprintf("[ble] weather request queued=%u\n", sent);
    } else if (!strcmp(argv[1], "clear_weather")) {
        wf_phone_clear_weather(&phone);
        wristflow_weather_reset();
        rt_kprintf("[ble] cleared weather cache\n");
    } else if (!strcmp(argv[1], "status")) {
        rt_kprintf("[ble] A1 connected=%u subscribed=%u generation=%u messages=%u added=%u updated=%u removed=%u rejected=%u unknown=%u dropped=%u\n",
            connection != 0xff, subscribed, generation, phone.count, phone.received, phone.updated,
            phone.removed, phone.rejected, phone.unknown, dropped);
        wristflow_weather_data_t weather;
        wristflow_weather_state_t state;
        bool cached = wristflow_weather_get_current(&weather, &state);
        rt_kprintf("[ble] weather cached=%u state=%u expired=%u age_known=%u received_utc=%u request=%u remaining_ms=%u\n",
            cached, state, weather.expired, weather.age_known, weather.received_utc, weather.request_state,
            wristflow_weather_request_remaining_ms());
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
