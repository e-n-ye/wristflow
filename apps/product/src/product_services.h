#ifndef WRISTFLOW_PRODUCT_SERVICES_H
#define WRISTFLOW_PRODUCT_SERVICES_H
#include <stddef.h>
#include "product_state.h"
#include "component_layout.h"
#include "weather.h"
#include "alarm_store.h"

void wristflow_product_services_start(wristflow_settings_t *settings, wristflow_layout_t *layout);
wristflow_watch_snapshot_t wristflow_product_services_snapshot(void);
int wristflow_product_services_set_time(uint32_t utc_seconds);
wristflow_alarm_clock_t wristflow_product_services_time(void);
/* Latest existing RTC observation, copied without reading hardware. */
wristflow_alarm_clock_t wristflow_product_services_clock_cache(void);
uint64_t wristflow_product_services_monotonic(void);
bool wristflow_product_services_storage_ready(void);
/* Thread context only; copies a latched fault under the shared DB mutex. */
bool wristflow_product_services_storage_failed(void);
/* Freeze shared writes after a failed logical checkpoint; reads remain available. */
void wristflow_product_services_checkpoint_failed(void);
#ifdef WF_STORAGE_PROBE
#include "storage_snapshot.h"
unsigned wristflow_product_services_storage_snapshot(uint8_t *, size_t, wf_storage_probe_info_t *);
#endif
/* Independent bounded step checkpoint keys; called by the IMU worker only. */
size_t wristflow_product_services_steps_read(unsigned key, uint8_t *record, size_t capacity, void *context);
bool wristflow_product_services_steps_write(unsigned key, const uint8_t *record, void *context);
/* Four bounded keys: payload A/B and matching publication markers. */
bool wristflow_product_services_alarm_read(unsigned key, uint8_t *record, void *context);
bool wristflow_product_services_alarm_write(unsigned key, const uint8_t *record, void *context);
/* Copies preferences; bounded retries/readback run on the event-driven worker. */
uint32_t wristflow_product_services_settings(const wristflow_settings_t *settings, void *context);
wristflow_save_state_t wristflow_product_services_settings_status(uint32_t request, void *context);
/* Copies a complete layout immediately; worker writes and verifies independently. */
uint32_t wristflow_product_services_layout(const wristflow_layout_t *layout, void *context);
wristflow_save_state_t wristflow_product_services_layout_status(uint32_t request, void *context);
void wristflow_product_services_update_weather(const wristflow_phone_weather_t *w, unsigned updates);
#endif
