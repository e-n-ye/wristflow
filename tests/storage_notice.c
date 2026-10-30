#include "storage_notice.h"
#include "wristflow_ui.h"
#include <assert.h>
#include <string.h>
static bool failed;
bool wristflow_product_services_storage_failed(void) { return failed; }
bool wf_product_runtime_failed(void) { return false; }
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(390,450);assert(display);
    wristflow_ui_init("");
    wf_product_storage_notice_poll();
    assert(!lv_obj_find_by_name(lv_layer_top(),"storage_write_failure"));
    failed=true;wf_product_storage_notice_poll();
    lv_obj_t *notice=lv_obj_find_by_name(lv_layer_top(),"storage_write_failure");
    assert(notice && !lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE));
    assert(!strcmp(lv_label_get_text(notice),"保存失败，请先导出记录"));
    uint32_t count=lv_obj_get_child_count(lv_layer_top());
    for(unsigned i=0;i<1000;++i)wf_product_storage_notice_poll();
    assert(lv_obj_get_child_count(lv_layer_top())==count);
    lv_obj_t *screen=lv_obj_create(NULL);lv_screen_load(screen);
    wf_product_storage_notice_poll();
    assert(lv_obj_get_parent(notice)==lv_layer_top() && !lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN));
    return 0;
}
