#ifdef WF_RESOURCE_PROBE
#include "resource_probe.h"
#include "wristflow_ui.h"
#include "mount_screen.h"

static void expired(void *owner) { wf_resource_probe_stop(owner, 1); }

void wf_resource_probe_stop(wf_resource_probe_t *probe, unsigned reason)
{
    if (!probe->active) return;
    probe->elapsed = lv_tick_get() - probe->started;
    probe->reason = reason;
    probe->active = false;
    lv_obj_delete(probe->root);
    probe->root = NULL;
    wristflow_ui_shell_release_awake(probe->shell, probe);
}

bool wf_resource_probe_start(wf_resource_probe_t *probe, wristflow_ui_shell_t *shell,
                             unsigned mode, uint32_t tag)
{
    if (probe->active || mode > 14 || !tag) return false;
    if (!wristflow_ui_shell_acquire_awake(shell, probe, "resource validation", 60000, expired))
        return false;
    *probe = (wf_resource_probe_t){.shell=shell, .tag=tag, .mode=mode,
        .started=lv_tick_get(), .active=true};
    lv_obj_t *screen = mode == 3 ? screen_notification_detail_create() : screen_watchface_create();
    if (!screen) {
        probe->active = false;
        wristflow_ui_shell_release_awake(shell, probe);
        return false;
    }
    probe->root = wristflow_mount_screen(lv_layer_top(), screen);
    if (mode == 14) {
        lv_obj_t *artwork = lv_obj_find_by_name(probe->root, "hour_artwork");
        lv_obj_add_flag(artwork, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *thumbnail = face_thumbnail_create(probe->root, false);
        lv_obj_center(thumbnail);
    } else if (mode == 3) {
        lv_label_set_text(lv_obj_find_by_name(probe->root, "message_source"), "资源验证 / 22px / 2bpp");
        lv_label_set_text(lv_obj_find_by_name(probe->root, "message_title"), "常用汉字、英文与标点");
        lv_label_set_text(lv_obj_find_by_name(probe->root, "message_body"),
            "你好，今天记得带钥匙。\n起床、笔记、天气与消息。\nABC xyz 0123456789\n，。！？：；（）《》\n二级字：𠳐 𥻗 𬉼\n范围外：龘 𠮷 😀");
    } else {
        const void *images[] = {hour_1, hour_7, hour_unknown, NULL,
            hour_2, hour_3, hour_4, hour_5, hour_6, hour_8, hour_9, hour_10, hour_11, hour_12};
        lv_image_set_src(lv_obj_find_by_name(probe->root, "hour_artwork"), images[mode]);
        lv_label_set_text(lv_obj_find_by_name(probe->root, "minute_label"), "TEST");
    }
    /* The mounted official view consumes touch; KEY1 or the deadline exits. */
    return true;
}

void wf_resource_probe_tick(wf_resource_probe_t *probe, bool eligible)
{
    if (!probe->active) return;
    probe->elapsed = lv_tick_get() - probe->started;
    if (!eligible) wf_resource_probe_stop(probe, 4);
    else if (probe->elapsed >= 60000) wf_resource_probe_stop(probe, 1);
}
#endif
