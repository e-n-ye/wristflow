#ifndef WRISTFLOW_PRODUCT_BLE_H
#define WRISTFLOW_PRODUCT_BLE_H
#include "notifications.h"
void wristflow_product_ble_start(void);
void wristflow_product_ble_snapshot(wristflow_notifications_snapshot_t *snapshot);
void wristflow_product_ble_delete(bool all, int32_t id, void *context);
#endif
