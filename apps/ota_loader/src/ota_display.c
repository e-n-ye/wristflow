#include <rtconfig.h>
#ifdef WF_OTA_ANDROID_RECEIVER
#include <rtdevice.h>
#include <drv_lcd.h>
#include "ota_display.h"
#include "wristflow_ota_ui.h"

extern void lv_lcd_init(const char *name);

static const struct {
    const char *title;
    const char *hint;
} copy[] = {
    [WF_OTA_WAITING]        = {"等待手机连接", "在 SIFLI BLE 中选择此设备"},
    [WF_OTA_CONNECTED]      = {"手机已连接", "请在手机上选择升级包"},
    [WF_OTA_RECEIVING]      = {"正在接收更新", "请保持手机蓝牙连接"},
    [WF_OTA_VERIFYING]      = {"正在校验更新", "请保持手表供电"},
    [WF_OTA_INSTALLING]     = {"正在安装更新", "请勿关闭手表"},
    [WF_OTA_REBOOTING]      = {"即将重启", "重启后查看更新结果"},
    [WF_OTA_DISCONNECTED]   = {"手机连接已断开", "重新连接后请从头发送"},
    [WF_OTA_CANCELED]       = {"更新已取消", "在手机上可重新发送"},
    [WF_OTA_TIMEOUT]        = {"接收超时", "请在手机上重新发送"},
    [WF_OTA_BAD_PACKAGE]    = {"更新包无法使用", "请检查升级包并重试"},
    [WF_OTA_NOT_READY]      = {"暂不能升级", "请检查电量后重试"},
    [WF_OTA_INSTALL_FAILED] = {"安装失败", "返回表盘后重试"},
    [WF_OTA_RADIO_FAILED]   = {"蓝牙未就绪", "返回表盘后重试"},
};
static uint32_t display_state = WF_OTA_WAITING;

/* A single-word mailbox. Producers never wait for the display worker or DMA. */
void wf_ota_display_publish(wf_ota_display_phase_t phase, uint8_t percent)
{
    __atomic_store_n(&display_state, (uint32_t)phase | ((uint32_t)percent << 8), __ATOMIC_RELAXED);
}

static uint32_t tick_ms(void)
{
    return (uint32_t)rt_tick_get_millisecond();
}

static void show(void *context)
{
    (void)context;
    rt_device_t lcd = rt_device_find("lcd");
    if (!lcd) return;
    lv_tick_set_cb(tick_ms);
    lv_lcd_init("lcd");
    rt_device_control(lcd, SF_GRAPHIC_CTRL_ASSERT_IF_DRAWTIMEOUT, (void *)2);
    uint8_t brightness = 12;
    rt_device_control(lcd, RTGRAPHIC_CTRL_SET_BRIGHTNESS, &brightness);
    wristflow_ota_ui_init(NULL);
    lv_obj_t *screen = screen_ota_receiver_create();
    if (!screen) return;
    lv_obj_t *title = lv_obj_find_by_name(screen, "ota_state");
    lv_obj_t *ring = lv_obj_find_by_name(screen, "ota_progress");
    lv_obj_t *value = lv_obj_find_by_name(screen, "ota_value");
    lv_obj_t *peer = lv_obj_find_by_name(screen, "ota_peer");
    lv_obj_t *icon = lv_obj_find_by_name(screen, "ota_icon");
    lv_obj_t *hint = lv_obj_find_by_name(screen, "ota_instructions");
    if (!title || !ring || !value || !peer || !icon || !hint) return;

    uint32_t shown = UINT32_MAX;
    lv_screen_load(screen);
    for (;;) {
        uint32_t current = __atomic_load_n(&display_state, __ATOMIC_RELAXED);
        if (current != shown) {
            wf_ota_display_phase_t phase = current & 0xff;
            unsigned percent = current >> 8;
            bool receiving = phase == WF_OTA_RECEIVING;
            bool transfer_complete = phase == WF_OTA_VERIFYING || phase == WF_OTA_INSTALLING ||
                                     phase == WF_OTA_REBOOTING;
            if ((shown & 0xff) != phase) {
                lv_label_set_text_static(title, copy[phase].title);
                lv_label_set_text_static(hint, copy[phase].hint);
                lv_obj_set_flag(value, LV_OBJ_FLAG_HIDDEN, !receiving);
                lv_obj_set_flag(peer, LV_OBJ_FLAG_HIDDEN, phase != WF_OTA_WAITING);
                lv_obj_set_flag(icon, LV_OBJ_FLAG_HIDDEN,
                                phase != WF_OTA_WAITING && phase != WF_OTA_CONNECTED && phase != WF_OTA_DISCONNECTED);
            }
            if (receiving) lv_label_set_text_fmt(value, "%u%%", percent);
            lv_arc_set_value(ring, transfer_complete ? 100 : receiving ? percent : 0);
            lv_obj_set_style_arc_opa(ring, transfer_complete || (receiving && percent) ? LV_OPA_COVER : LV_OPA_TRANSP,
                                     LV_PART_INDICATOR);
            shown = current;
        }
        lv_timer_handler();
        rt_thread_mdelay(80);
    }
}

void wf_ota_display_start(void)
{
    /* Below ota_rx (22). LCD/LVGL work has no call into the OTA protocol. */
    rt_thread_t thread = rt_thread_create("ota_ui", show, NULL, 4096, 23, 10);
    if (thread) rt_thread_startup(thread);
}
#endif
