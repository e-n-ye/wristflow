#ifndef WRISTFLOW_PRODUCT_SERVICES_H
#define WRISTFLOW_PRODUCT_SERVICES_H
#include "product_state.h"
#include "component_layout.h"

void wristflow_product_services_start(wristflow_settings_t *settings, wristflow_layout_t *layout);
wristflow_watch_snapshot_t wristflow_product_services_snapshot(void);
/* Copies preferences; FlashDB writes run on a separate thread after 1.5s idle. */
void wristflow_product_services_settings(const wristflow_settings_t *settings);
/* Copies a complete layout immediately; worker writes and verifies independently. */
uint32_t wristflow_product_services_layout(const wristflow_layout_t *layout, void *context);
wristflow_save_state_t wristflow_product_services_layout_status(uint32_t request, void *context);
#endif
