#ifndef WRISTFLOW_PRODUCT_SERVICES_H
#define WRISTFLOW_PRODUCT_SERVICES_H
#include "product_state.h"

void wristflow_product_services_start(wristflow_settings_t *settings);
wristflow_watch_snapshot_t wristflow_product_services_snapshot(void);
/* Copies preferences; FlashDB writes run on a separate thread after 1.5s idle. */
void wristflow_product_services_settings(const wristflow_settings_t *settings);
#endif
