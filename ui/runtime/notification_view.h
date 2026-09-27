#ifndef WRISTFLOW_NOTIFICATION_VIEW_H
#define WRISTFLOW_NOTIFICATION_VIEW_H
#include "ui_shell.h"
typedef struct wristflow_notification_view wristflow_notification_view_t;
typedef enum { WF_ALERT_NONE, WF_ALERT_BANNER, WF_ALERT_PREVIEW } wristflow_alert_t;
wristflow_notification_view_t *wristflow_notification_view_create(wristflow_ui_shell_t *shell);
void wristflow_notification_view_destroy(wristflow_notification_view_t *view);
void wristflow_notification_view_update(wristflow_notification_view_t *view,
    const wristflow_notifications_snapshot_t *snapshot);
bool wristflow_notification_view_contains(const wristflow_notification_view_t *view, int32_t id);
void wristflow_notification_view_select(wristflow_notification_view_t *view, int32_t id);
lv_obj_t *wristflow_notification_view_screen(wristflow_notification_view_t *view,
    wristflow_surface_t surface, bool *created);
void wristflow_notification_view_show(wristflow_notification_view_t *view, wristflow_alert_t kind, int32_t id);
void wristflow_notification_view_hide(wristflow_notification_view_t *view);
wristflow_alert_t wristflow_notification_view_alert(const wristflow_notification_view_t *view);
void wristflow_notification_view_activity(wristflow_notification_view_t *view);
#endif
