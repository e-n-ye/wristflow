#include <rtconfig.h>
#ifdef WF_OTA_ANDROID_RECEIVER
#include <rtthread.h>
#include <bf0_hal.h>
#include <string.h>
#include "bf0_ble_gap.h"
#include "bf0_sibles.h"
#include "bf0_sibles_internal.h"
#include "bf0_sibles_advertising.h"
#include "bf0_sibles_serial_trans_service.h"
#include "bf0_sibles_nvds.h"
#include "receiver_identity.h"
#include "probe_port.h"
#include "startup.h"
#include "product_power.h"
#include "offline_receiver.h"
#include "ota_display.h"

/* The extra two bytes align the signed header and main body after FCES. */
static uint8_t package[2 + WF_DFU_ENVELOPE + WF_DFU_SLOT_BYTES]
    __attribute__((section(".bss.l2_non_ret_bss_ota"), aligned(32)));
static CRC_HandleTypeDef crc;
static struct rt_semaphore power;
static struct rt_semaphore address_ready;
static bd_addr_t receiver_address;
static volatile bool address_requested;
static volatile bool address_matched;
static rt_mq_t requests;
static volatile uint32_t connection_epoch;
static volatile uint8_t peer = 0xff;
typedef struct {
    uint32_t epoch;
    uint16_t len;
    uint8_t handle, data[];
} message_t;
SIBLES_ADVERTISING_CONTEXT_DECLAR(advertising);

static bool active(const message_t *m)
{
    return m && m->epoch == connection_epoch && m->handle == peer;
}

static uint32_t checksum(void *context, const uint8_t *bytes, size_t n)
{
    (void)context;
    HAL_CRC_Setmode(&crc, CRC_32_MPEG_2);
    if (n >= 4) return HAL_CRC_Calculate(&crc, (uint8_t *)bytes, n);
    /* The HAL word loop requires at least four bytes. Feed a short final
     * packet at its actual hardware data width. */
    uint32_t data = 0;
    memcpy(&data, bytes, n);
    MODIFY_REG(crc.Instance->CR, CRC_CR_DATASIZE_Msk, (n - 1) << CRC_CR_DATASIZE_Pos);
    crc.Instance->DR = data;
    while (!(crc.Instance->SR & CRC_SR_DONE)) { }
    return crc.Instance->DR;
}

static void serial_event(ble_serial_tran_event_t event, uint8_t *arg)
{
    if (!requests || !arg) return;
    uint8_t handle = arg[0];
    if (event == BLE_SERIAL_TRAN_OPEN) { ++connection_epoch; peer = handle; return; }
    if (handle != peer) return;
    if (event == BLE_SERIAL_TRAN_CLOSE || event == BLE_SERIAL_TRAN_ERROR) {
        ++connection_epoch;
        if (event == BLE_SERIAL_TRAN_CLOSE) peer = 0xff;
        return;
    }
    if (event != BLE_SERIAL_TRAN_DATA) return;
    ble_serial_tran_data_t *packet = (ble_serial_tran_data_t *)arg;
    if (packet->cate_id != BLE_DFU_CATEID || !packet->data ||
        packet->len < 4 || packet->len > 4 + 12 + WF_DFU_BLOCK) return;
    if (wf_dfu_is_cancel(packet->data, packet->len)) ++connection_epoch;
    message_t *m = rt_malloc(sizeof *m + packet->len);
    if (!m) { ++connection_epoch; return; }
    m->epoch = connection_epoch; m->handle = handle; m->len = packet->len;
    memcpy(m->data, packet->data, packet->len);
    if (rt_mq_send(requests, &m, sizeof m) != RT_EOK) { ++connection_epoch; rt_free(m); }
}
BLE_SERIAL_TRAN_EXPORT(BLE_DFU_CATEID, serial_event);

static int gap_event(uint16_t event, uint8_t *data, uint16_t len, uint32_t ctx)
{
    (void)ctx;
    if (event == BLE_POWER_ON_IND && requests) rt_sem_release(&power);
    if (event == BLE_GAP_LOCAL_BD_ADDR_IND && address_requested && data &&
        len >= sizeof(ble_gap_dev_bdaddr_ind_t)) {
        const ble_gap_dev_bdaddr_ind_t *ind = (const ble_gap_dev_bdaddr_ind_t *)data;
        address_matched = ind->addr.addr_type == 1 &&
            !memcmp(ind->addr.addr.addr, receiver_address.addr, sizeof receiver_address.addr);
        address_requested = false;
        rt_sem_release(&address_ready);
    }
    return 0;
}
BLE_EVENT_REGISTER(gap_event, NULL);

static bool prepare_address(void)
{
    /* SF32LB52 UID field used by the SDK's bt_mac_addr_generate_via_uid_v2. */
    _Alignas(4) uint8_t uid[16];
    if (HAL_EFUSE_Read(0, uid, sizeof uid) != sizeof uid ||
        !wf_ota_receiver_address(uid, receiver_address.addr)) return false;
    uint8_t tag[2 + NVDS_APP_LEN_BD_ADDRESS] = {NVDS_APP_TAG_BD_ADDRESS, NVDS_APP_LEN_BD_ADDRESS};
    memcpy(tag + 2, receiver_address.addr, sizeof receiver_address.addr);
    /* The SDK reads the APP TLV while configuring GAP, before POWER_ON_IND.
     * Its persistent-tag API is not initialized yet; this backend is RAM only. */
    return sifli_nvds_flash_adaptor_write("SIF_APP", tag, sizeof tag) == NVDS_OK;
}

static bool confirm_address(void)
{
    bd_addr_t public_address;
    if (ble_get_public_address(&public_address) != 0 ||
        !memcmp(public_address.addr, receiver_address.addr, sizeof receiver_address.addr)) return false;
    address_requested = true;
    bool ok = ble_gap_get_local_bdaddr() == 0 &&
              rt_sem_take(&address_ready, rt_tick_from_millisecond(3000)) == RT_EOK && address_matched;
    address_requested = false;
    if (ok) {
        const uint8_t *a = receiver_address.addr;
        rt_kprintf("[ota-radio] receiver address=%02X:%02X:%02X:%02X:%02X:%02X type=1\n",
                   a[5], a[4], a[3], a[2], a[1], a[0]);
    }
    return ok;
}

static void address_failure(void)
{
    rt_kprintf("[ota-radio] receiver address unconfirmed\n");
    wf_ota_display_publish(WF_OTA_RADIO_FAILED, 0);
    rt_thread_mdelay(120000);
    if (wf_ota_loader_has_stable()) HAL_PMU_Reboot();
}

uint8_t sibles_advertising_disc_mode_get(void) { return GAPM_ADV_MODE_GEN_DISC; }
static uint8_t advertising_event(uint8_t event, void *context, void *data)
{
    (void)context;
    if (event == SIBLES_ADV_EVT_ADV_STARTED)
        rt_kprintf("[ota-radio] advertising=%u name=WristFlow OTA\n", ((sibles_adv_evt_startted_t *)data)->status);
    return 0;
}
static bool advertise(void)
{
    const uint8_t uuid[] = serial_tran_svc_uuid;
    const char name[] = "WristFlow OTA";
    sibles_advertising_para_t p = {0};
    p.own_addr_type = GAPM_STATIC_ADDR;
    p.config.adv_mode = SIBLES_ADV_CONNECT_MODE;
    p.config.mode_config.conn_config.interval = 0xa0;
    p.config.is_auto_restart = 1;
    p.config.max_tx_pwr = 0x7f;
    p.evt_handler = advertising_event;
    p.rsp_data.completed_name = rt_malloc(sizeof(sibles_adv_type_name_t) + sizeof name);
    p.adv_data.completed_uuid = rt_malloc(sizeof(sibles_adv_type_srv_uuid_t) + sizeof(sibles_adv_uuid_t));
    ble_gap_dev_name_t *local = rt_malloc(sizeof *local + sizeof name);
    bool ok = false;
    if (local && p.rsp_data.completed_name && p.adv_data.completed_uuid) {
        local->len = sizeof name - 1;
        memcpy(local->name, name, local->len);
        ble_gap_set_dev_name(local);
        p.rsp_data.completed_name->name_len = local->len;
        memcpy(p.rsp_data.completed_name->name, name, local->len);
        p.adv_data.completed_uuid->count = 1;
        p.adv_data.completed_uuid->uuid_list[0].uuid_len = sizeof uuid;
        memcpy(p.adv_data.completed_uuid->uuid_list[0].uuid.uuid_128, uuid, sizeof uuid);
        ok = sibles_advertising_init(advertising, &p) == SIBLES_ADV_NO_ERR &&
             sibles_advertising_start(advertising) == SIBLES_ADV_NO_ERR;
    }
    rt_free(local); rt_free(p.rsp_data.completed_name); rt_free(p.adv_data.completed_uuid);
    return ok;
}

static void verifying(void *context)
{
    (void)context;
    wf_ota_display_publish(WF_OTA_VERIFYING, 100);
}

static void worker(void *unused)
{
    (void)unused;
    if (rt_sem_take(&power, rt_tick_from_millisecond(15000)) != RT_EOK) {
        rt_kprintf("[ota-radio] power-on timeout\n");
        wf_ota_display_publish(WF_OTA_RADIO_FAILED, 0);
        return;
    }
    if (!confirm_address()) { address_failure(); return; }
    ble_serial_tran_init();
    if (!advertise()) {
        rt_kprintf("[ota-radio] advertising setup failed\n");
        wf_ota_display_publish(WF_OTA_RADIO_FAILED, 0);
        return;
    }
    wf_dfu_receiver_t r = {.file=package + 2, .capacity=sizeof package - 2,
                           .can_start=wf_ota_loader_accepting, .crc32_mpeg2=checksum,
                           .install=wf_ota_loader_install, .verifying=verifying};
    uint32_t epoch = connection_epoch;
    bool connected = peer != 0xff;
    if (connected) wf_ota_display_publish(WF_OTA_CONNECTED, 0);
    rt_tick_t last = rt_tick_get();
    for (;;) {
        message_t *m = NULL;
        rt_mq_recv(requests, &m, sizeof m, rt_tick_from_millisecond(500));
        if (epoch != connection_epoch) {
            bool now_connected = peer != 0xff;
            if (r.phase == WF_DFU_RECEIVING) wf_ota_display_disconnected(last);
            else if (now_connected != connected)
                wf_ota_display_publish(now_connected ? WF_OTA_CONNECTED : WF_OTA_WAITING, 0);
            connected = now_connected;
            wf_dfu_disconnect(&r);
            epoch = connection_epoch;
        }
        if (r.phase == WF_DFU_RECEIVING && rt_tick_get() - last > rt_tick_from_millisecond(60000)) {
            wf_dfu_disconnect(&r);
            wf_ota_display_publish(WF_OTA_TIMEOUT, 0);
        }
        if (rt_tick_get() - last > rt_tick_from_millisecond(WF_OTA_IDLE_TIMEOUT_MS)) {
            last = rt_tick_get();
            if (wf_ota_loader_has_stable()) {
                if (m) rt_free(m);
                rt_kprintf("[ota-radio] inactive, returning to boot selection\n");
                HAL_PMU_Reboot(); return;
            }
        }
        if (!m) continue;
        if (active(m)) {
            uint8_t reply[12];
            size_t n = wf_dfu_request(&r, m->data, m->len, reply);
            bool receipt = false;
            if (n && active(m)) {
                ble_serial_tran_data_t packet = {.handle=m->handle,.cate_id=BLE_DFU_CATEID,.len=n,.data=reply};
                int sent = ble_serial_tran_send_data(&packet);
                receipt = sent == (int)n + 4;
                if (m->data[0] != WF_DFU_DATA || reply[4])
                    rt_kprintf("[ota-radio] request=%u result=%u accepted=%u sent=%d\n", m->data[0], reply[4], r.accepted, sent);
            }
            last = rt_tick_get();
            if (wf_dfu_is_cancel(m->data, m->len)) {
                wf_ota_display_publish(WF_OTA_CANCELED, 0);
            } else if (!receipt && n) {
                wf_dfu_disconnect(&r);
                wf_ota_display_disconnected(last);
            } else if (n && r.phase == WF_DFU_IDLE && reply[4]) {
                wf_ota_display_publish(reply[4] == WF_DFU_NOT_READY ? WF_OTA_NOT_READY : WF_OTA_BAD_PACKAGE, 0);
            } else if (n && !reply[4] && r.phase == WF_DFU_RECEIVING) {
                wf_ota_display_publish(WF_OTA_RECEIVING, wf_dfu_percent(&r));
            }
            if (receipt && r.phase == WF_DFU_VERIFIED) {
                rt_kprintf("[ota-radio] complete signed file, starting local install\n");
                wf_ota_display_publish(WF_OTA_INSTALLING, 100);
                if (wf_dfu_install_received(&r)) {
                    rt_free(m);
                    rt_kprintf("[ota-ready] release=%u bytes=%u trial committed\n", r.image.release, r.image.bytes);
                    wf_ota_display_publish(WF_OTA_REBOOTING, 100);
                    rt_thread_mdelay(1000); HAL_PMU_Reboot(); return;
                }
                rt_kprintf("[ota-radio] local install failed; inspect slots and journal\n");
                wf_ota_display_publish(WF_OTA_INSTALL_FAILED, 0);
            }
        }
        rt_free(m);
    }
}

void wf_ota_android_start(void)
{
    if (!wf_ota_loader_inhibited()) { rt_kprintf("[ota-radio] charger inhibit unverified\n"); return; }
    wf_ota_display_start();
    HAL_RCC_EnableModule(RCC_MOD_CRC1);
    if (HAL_CRC_Init(&crc) != HAL_OK || !wristflow_product_power_adc_init() ||
        checksum(NULL, (const uint8_t *)"123456789", 9) != 0x0376e6e7u) {
        rt_kprintf("[ota-radio] startup guard rejected\n");
        wf_ota_display_publish(WF_OTA_NOT_READY, 0);
        return;
    }
    if (!prepare_address()) { address_failure(); return; }
    rt_sem_init(&power, "ota_pwr", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&address_ready, "ota_addr", 0, RT_IPC_FLAG_FIFO);
    requests = rt_mq_create("ota_wire", sizeof(message_t *), 4, RT_IPC_FLAG_FIFO);
    if (!requests) { wf_ota_display_publish(WF_OTA_RADIO_FAILED, 0); return; }
    rt_thread_t th = rt_thread_create("ota_rx", worker, NULL, 16384, 22, 10);
    if (!th || rt_thread_startup(th) != RT_EOK) { wf_ota_display_publish(WF_OTA_RADIO_FAILED, 0); return; }
    sifli_ble_enable();
}
#endif
