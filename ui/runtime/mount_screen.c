#include "mount_screen.h"

lv_obj_t *wristflow_mount_screen(lv_obj_t *parent, lv_obj_t *screen)
{
    /* LVGL top-level screens cannot be reparented. */
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, lv_obj_get_style_bg_color(screen, LV_PART_MAIN), 0);
    lv_obj_set_style_bg_opa(root, lv_obj_get_style_bg_opa(screen, LV_PART_MAIN), 0);
    lv_obj_set_style_text_color(root, lv_obj_get_style_text_color(screen, LV_PART_MAIN), 0);
    lv_obj_set_style_text_font(root, lv_obj_get_style_text_font(screen, LV_PART_MAIN), 0);
    while (lv_obj_get_child_count(screen))
        lv_obj_set_parent(lv_obj_get_child(screen, 0), root);
    lv_obj_delete(screen);
    return root;
}
