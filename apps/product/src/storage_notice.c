#include "storage_notice.h"
#include "product_services.h"
#include "wristflow_ui.h"
void wf_product_storage_notice_poll(void)
{
    static lv_obj_t *notice;
    if(!wristflow_product_services_storage_failed())return;
    if(!notice) {
        notice=layout_save_status_create(lv_layer_top());
        if(!notice)return;
        lv_obj_set_name(notice,"storage_write_failure");
        lv_label_set_text(notice,"保存失败，请先导出记录");
        lv_obj_set_style_text_font(notice,notification_22,0);
        lv_obj_remove_flag(notice,LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE);
    }
    /* No click-to-retry: the partition is frozen for the rest of this boot. */
    lv_obj_move_foreground(notice);
}
