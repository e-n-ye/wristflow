#ifndef WRISTFLOW_PRODUCT_BLE_H
#define WRISTFLOW_PRODUCT_BLE_H
#include "notifications.h"
#include <stdbool.h>
#include "radio_policy.h"
void wristflow_product_ble_start(bool enabled);
void wristflow_product_ble_enable(bool enabled);
wf_radio_policy_t wristflow_product_ble_radio(void);
void wristflow_product_ble_snapshot(wristflow_notifications_snapshot_t *snapshot);
void wristflow_product_ble_delete(bool all, int32_t id, const wf_phone_metadata_t *expected, void *context);
bool wristflow_product_ble_is_connected(void);
bool wristflow_product_ble_request_weather(void);
#endif
