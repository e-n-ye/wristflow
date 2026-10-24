#include "compass_view.h"
#include <stdio.h>

static lv_obj_t *named(lv_obj_t *root, const char *name)
{
    lv_obj_t *object = lv_obj_find_by_name(root, name);
    LV_ASSERT(object);
    return object;
}

static void position(lv_obj_t *object, int angle, int radius, int width, int height)
{
    int x = radius * lv_trigo_sin((angle + 90) % 360) / LV_TRIGO_SIN_MAX;
    int y = radius * lv_trigo_sin(angle) / LV_TRIGO_SIN_MAX;
    lv_obj_set_pos(object, 195 + x - width / 2, 241 + y - height / 2);
}

void wristflow_compass_render(lv_obj_t *root, const wristflow_compass_display_t *display)
{
    if (!root) return;
    bool calibrating = display && display->state == WRISTFLOW_COMPASS_CALIBRATING;
    bool valid = display && display->state == WRISTFLOW_COMPASS_HEADING && display->heading_degrees < 360;
    lv_obj_set_flag(named(root, "compass_dial"), LV_OBJ_FLAG_HIDDEN, calibrating);
    lv_obj_set_flag(named(root, "compass_calibration_panel"), LV_OBJ_FLAG_HIDDEN, !calibrating);
    if (calibrating) return;

    static const char *const directions[] = {"北", "东北", "东", "东南", "南", "西南", "西", "西北"};
    char heading[16];
    if (valid) snprintf(heading, sizeof heading, "%u°", display->heading_degrees);
    else snprintf(heading, sizeof heading, "--°");
    lv_label_set_text(named(root, "compass_heading"), heading);
    lv_label_set_text(named(root, "compass_direction"), valid ? directions[((display->heading_degrees + 22) / 45) % 8] : "--");
    const char *status = "等待数据";
    if (display && (display->reason == WF_COMPASS_IO_FAILURE || display->reason == WF_COMPASS_SELFTEST_FAILURE))
        status = "测量失败";
    else if (display && display->reason == WF_COMPASS_BUSY) status = "暂不可用";
    lv_label_set_text(named(root, "compass_status"), valid ? "" : status);
    lv_label_set_text(named(root, "compass_notice"), display && display->uncalibrated ? "未校准，请保持水平" : "");

    /* An unknown heading has no north indication. Keep only a neutral ring. */
    const char *const marks[] = {"compass_n", "compass_e", "compass_s", "compass_w",
        "compass_north_arc", "compass_north_pointer", "compass_south_pointer"};
    for (unsigned i = 0; i < sizeof marks / sizeof marks[0]; ++i)
        lv_obj_set_flag(named(root, marks[i]), LV_OBJ_FLAG_HIDDEN, !valid);
    int north = valid ? (630 - (int)display->heading_degrees) % 360 : 270;
    lv_obj_t *a = named(root, "compass_ring_a"), *b = named(root, "compass_ring_b");
    lv_obj_set_style_arc_color(a, lv_color_hex(valid ? 0x56595e : 0x343638), LV_PART_MAIN);
    lv_obj_set_style_arc_color(b, lv_color_hex(valid ? 0x56595e : 0x343638), LV_PART_MAIN);
    lv_obj_set_style_bg_color(named(root, "compass_index"), lv_color_hex(valid ? 0x56595e : 0x686a6d), 0);
    lv_arc_set_bg_angles(a, 0, valid ? 154 : 180);
    lv_arc_set_bg_angles(b, 0, valid ? 154 : 180);
    lv_arc_set_rotation(a, (north + (valid ? 14 : 0)) % 360);
    lv_arc_set_rotation(b, (north + (valid ? 194 : 180)) % 360);
    if (!valid) return;

    lv_arc_set_rotation(named(root, "compass_north_arc"), (north + 315) % 360);
    for (unsigned i = 0; i < 4; ++i) {
        int angle = (north + (int)i * 90) % 360;
        lv_obj_t *label = named(root, marks[i]);
        position(label, angle, 126, 48, 44);
        lv_obj_set_style_transform_pivot_x(label, 24, 0);
        lv_obj_set_style_transform_pivot_y(label, 22, 0);
        lv_obj_set_style_transform_rotation(label, ((angle + 90) % 360) * 10, 0);
    }
    for (unsigned i = 0; i < 2; ++i) {
        int angle = (north + (int)i * 180) % 360;
        lv_obj_t *pointer = named(root, i ? "compass_south_pointer" : "compass_north_pointer");
        position(pointer, angle, 163, 24, 24);
        lv_image_set_rotation(pointer, ((angle + 90) % 360) * 10);
    }
}
