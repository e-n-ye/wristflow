#include "watchface_presenter.h"
#include "clock_format.h"

bool wristflow_watchface_set_time(lv_obj_t *screen, unsigned int hour_24,
                                 unsigned int minute)
{
    wristflow_clock_text_t value;
    if (screen == NULL || !wristflow_clock_format(hour_24, minute, &value))
        return false;
    lv_obj_t *hour = lv_obj_find_by_name(screen, "hour_artwork");
    lv_obj_t *minutes = lv_obj_find_by_name(screen, "minute_label");
    if (hour == NULL || minutes == NULL)
        return false;
    const void *hours[] = {hour_1, hour_2, hour_3, hour_4, hour_5, hour_6,
                          hour_7, hour_8, hour_9, hour_10, hour_11, hour_12};
    if (hours[value.hour_12 - 1] == NULL)
        return false;
    lv_image_set_src(hour, hours[value.hour_12 - 1]);
    lv_label_set_text(minutes, value.minute_text);
    return true;
}
