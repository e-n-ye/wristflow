#ifdef WF_OTA_PRODUCT
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include <bf0_hal.h>
#include <drv_flash.h>
#include <drv_lcd.h>
#include <ptab.h>
#include <string.h>
#include "ota_trial.h"
#include "product_ble.h"
#include "product_flash_gate.h"
#include "product_ota.h"
#include "product_runtime_record.h"
#include "product_services.h"
#include "product_settings_store.h"
#include "product_storage_access.h"
#include "product_power.h"
#include "product_alarm.h"
#include "product_steps.h"
#include "product_charger.h"
#include "wristflow_ui.h"

enum { SLOT_BYTES = HCPU_FLASH_CODE_SIZE / 2, HEADER_BYTES = 336 };
_Static_assert(HCPU_FLASH_CODE_START_ADDR == 0x12020000 && SLOT_BYTES == 0x400000, "Product slot layout");
_Static_assert(KVDB_DFU_REGION_SIZE == 16384, "Two independent journal units required");
static const char board[24] = "sf32lb52-lchspi-ulp";
static uint32_t trial, version, outcome;
static unsigned running;
static bool requested, attempted;
static rt_tick_t first_frame;
static rt_device_t display;

/* All selection and writes to the journal belong to RAM boot or the receiver.
 * Here a mapped Product checks its boot context before it can request a reset. */
static bool inspect(void)
{
    if (hwp_mpi2->CR & MPI_CR_CTRE) return false;
    if (!hwp_mpi2->AASAR && !hwp_mpi2->AAEAR && !hwp_mpi2->AAOAR) running = 0;
    else if (hwp_mpi2->AASAR == HCPU_FLASH_CODE_OFFSET && hwp_mpi2->AAEAR == HCPU_FLASH_CODE_OFFSET + SLOT_BYTES &&
             hwp_mpi2->AAOAR == SLOT_BYTES) running = 1;
    else return false;
    uint8_t raw[2][WF_OTA_AB_BYTES];
    for (unsigned i = 0; i < 2; i++) {
        uint32_t addr = KVDB_DFU_REGION_START_ADDR + i * (KVDB_DFU_REGION_SIZE / 2);
        if (rt_flash_read(addr, raw[i], sizeof raw[i]) != sizeof raw[i]) return false;
    }
    wf_ota_ab_state_t state;
    int selected = wf_ota_ab_select(&state, raw[0], sizeof raw[0], raw[1], sizeof raw[1]);
    if (selected < 0) return false;
    uint32_t word = HAL_Get_backup(RTC_BAKCUP_OTA_FORCE_MODE);
    bool trying = state.phase == WF_OTA_AB_TRIED && state.pending == running &&
                  word == (WF_OTA_TRIAL_PENDING | wf_ota_trial_cookie(&state)) &&
                  (hwp_wdt1->WDT_SR & WDT_WDT_SR_WDT_ACTIVE);
    if (!trying && (state.phase != WF_OTA_AB_STABLE || state.stable != running || word != 0)) return false;
    uint32_t manifest[20];
    memcpy(manifest, (const void *)(HCPU_FLASH_CODE_START_ADDR + SLOT_BYTES - HEADER_BYTES), sizeof manifest);
    const wf_ota_ab_image_t *image = &state.image[running];
    if (memcmp(manifest, "WFR1", 4) || manifest[1] != 1 || memcmp(manifest + 2, board, sizeof board) ||
        manifest[8] != 1 || manifest[9] != HCPU_FLASH_CODE_START_ADDR ||
        manifest[10] != image->bytes || manifest[11] != image->release ||
        memcmp(manifest + 12, image->sha256, sizeof image->sha256)) return false;
    trial = trying ? word : 0;
    version = image->release;
    uint32_t receipt = HAL_Get_backup(RTC_BACKUP_NAND_OTA_DES);
    uint32_t kind = receipt & WF_OTA_TRIAL_KIND;
    if (kind == WF_OTA_RESULT_SUCCESS || kind == WF_OTA_RESULT_ROLLBACK) {
        HAL_Set_backup(RTC_BACKUP_NAND_OTA_DES, 0);
        wf_ota_ab_state_t previous;
        if (HAL_Get_backup(RTC_BACKUP_NAND_OTA_DES) == 0 &&
            wf_ota_ab_decode(&previous, raw[1 - selected], sizeof raw[0]))
            outcome = wf_ota_trial_result(receipt, &state, &previous);
    }
    return true;
}

static bool request(void)
{
    if (!version || trial || requested) return false;
    requested = true;
    return true;
}

void wf_product_ota_start(wristflow_ui_shell_t *shell, bool services_started)
{
    if (!inspect()) return;
    rt_kprintf("[ota-product] slot=%c release=%u trial=%u services=%u\n", 'A'+running, version, !!trial, services_started);
    if (!services_started) { attempted = true; return; }
    wristflow_ui_shell_bind_firmware(shell, version, trial ? NULL : request);
    if (outcome) {
        lv_obj_t *notice = layout_save_status_create(lv_layer_top());
        if (notice) {
            lv_obj_set_style_text_font(notice, notification_22, 0);
            lv_label_set_text_fmt(notice, outcome == WF_OTA_RESULT_SUCCESS ?
                                  "更新成功，版本 %u" : "已回退到版本 %u", (unsigned)version);
            lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE);
            lv_obj_delete_delayed(notice, 6500);
        }
    }
    if (trial) {
        display = rt_device_find("lcd");
        lv_refr_now(NULL);
        first_frame = rt_tick_get();
    }
}
bool wf_product_ota_trial_pending(void) { return trial != 0; }

static bool saved(uint32_t generation)
{
    wf_runtime_summary_t current, durable;
    bool valid, pending;
    return wf_product_runtime_status(&current, &durable, &valid, &pending) && valid && !pending &&
           current.boot == durable.boot && durable.generation == generation;
}
/* Lock order mirrors alarm marker publication: alarm model, settings database,
 * then the physical Flash gate. Neither worker must do I/O under these gates. */
static bool try_reboot(uint32_t word, uint32_t generation)
{
    if (!wristflow_product_alarm_hold_saved()) return false;
    bool attempted_write = false;
    if (wf_product_storage_gate_acquire(100)) {
        if (!wristflow_product_services_storage_failed() && wf_product_flash_gate_take()) {
            if (wf_product_steps_held() && wf_product_charger_inhibited()) {
                rt_kprintf("[ota-product] handoff_intent=%s saved_generation=%u\n",
                           word == WF_OTA_ENTER_RECEIVER ? "receiver" : "healthy", generation);
                rt_base_t level = rt_hw_interrupt_disable();
                if (wf_product_steps_held() && wf_product_charger_inhibited() &&
                    HAL_Get_backup(RTC_BAKCUP_OTA_FORCE_MODE) == trial) {
                    attempted_write = true;
                    HAL_Set_backup(RTC_BAKCUP_OTA_FORCE_MODE, word);
                    if (HAL_Get_backup(RTC_BAKCUP_OTA_FORCE_MODE) == word) HAL_PMU_Reboot();
                    HAL_Set_backup(RTC_BAKCUP_OTA_FORCE_MODE, trial);
                }
                rt_hw_interrupt_enable(level);
            }
            wf_product_flash_gate_give();
        }
        wf_product_storage_gate_release();
    }
    wristflow_product_alarm_release();
    return attempted_write;
}
static void transfer(uint32_t word, bool was_enabled)
{
    if (!wristflow_product_settings_hold()) return;
    uint32_t generation;
    bool record_held = wf_product_runtime_hold(&generation);
    bool steps_held = record_held && wf_product_steps_hold();
    bool charger_held = steps_held && wf_product_charger_hold();
    if (charger_held) {
        wristflow_product_ble_enable(false);
        rt_tick_t start = rt_tick_get();
        while (rt_tick_get() - start < rt_tick_from_millisecond(14000)) {
            wristflow_save_state_t config = wristflow_product_settings_drain();
            if (config == WRISTFLOW_SAVE_FAILED || wf_product_runtime_failed() || wristflow_product_services_storage_failed()) break;
            wf_radio_policy_t radio = wristflow_product_ble_radio();
            if (config == WRISTFLOW_SAVE_DONE && saved(generation) && wf_radio_suspended(&radio) &&
                wf_product_steps_held() && wf_product_charger_inhibited() && try_reboot(word, generation)) break;
            rt_thread_mdelay(50);
        }
        wristflow_product_ble_enable(was_enabled);
        wf_product_charger_release();
    }
    if (steps_held) wf_product_steps_resume();
    if (record_held) wf_product_runtime_resume();
    wristflow_product_settings_resume();
}
void wf_product_ota_poll(wristflow_ui_shell_t *shell)
{
    if ((!trial || attempted) && !requested) return;
    wf_radio_policy_t radio = wristflow_product_ble_radio();
    if (trial) {
        bool busy = true;
        if (rt_tick_get() - first_frame < rt_tick_from_millisecond(3000) || !wf_radio_settled(&radio) ||
            !display || rt_device_control(display, RTGRAPHIC_CTRL_GET_BUSY, &busy) != RT_EOK || busy) return;
    }
    wristflow_power_snapshot_t v = wristflow_product_power_snapshot();
    if (trial && !v.voltage_valid) return;
    uint32_t word = trial ? WF_OTA_TRIAL_HEALTHY | (trial & WF_OTA_TRIAL_PAYLOAD) : WF_OTA_ENTER_RECEIVER;
    attempted = !!trial;
    requested = false;
    wristflow_ui_shell_sync_settings_save(shell);
    if (wristflow_product_services_storage_ready() && !wristflow_product_services_storage_failed() &&
        !wf_product_runtime_failed() && wristflow_ui_shell_settings_save_state(shell) != WRISTFLOW_SAVE_FAILED &&
        v.voltage_valid && v.millivolts > 3600 && v.millivolts <= 4350) {
        if (!trial) lv_refr_now(NULL);
        transfer(word, radio.enabled);
    }
    rt_kprintf("[ota-product] handoff refused slot=%c release=%u vbat=%u\n", 'A'+running, version, v.millivolts);
    if (!trial) wristflow_ui_shell_firmware_failed(shell);
}
#endif
