#ifndef WRISTFLOW_PRODUCT_SETTINGS_STORE_H
#define WRISTFLOW_PRODUCT_SETTINGS_STORE_H

#include "product_state.h"
#include "component_layout.h"

typedef struct {
    void *context;
    bool (*read_settings)(void *, uint8_t record[WRISTFLOW_SETTINGS_BYTES]);
    bool (*write_settings)(void *, const uint8_t record[WRISTFLOW_SETTINGS_BYTES]);
    wristflow_layout_read_t read_layout;
    wristflow_layout_write_t write_layout;
} wristflow_settings_store_io_t;

/* Startup only. Owns copies of the backend and restored layout publication.
 * No I/O on this call; unavailable storage must not initialize this service. */
bool wristflow_product_settings_store_start(const wristflow_settings_t *boot,
    bool restored, const wristflow_layout_store_t *, const wristflow_settings_store_io_t *);
/* UI-facing calls copy under a short mutex; never execute storage I/O.
 * Same pending/done preferences reuse their ID; a failed same-value request is
 * an explicit retry with a fresh ID/budget. New values debounce for 1500ms.
 * Each request attempts at most three times, with 5000ms between failures. */
uint32_t wristflow_product_settings_request(const wristflow_settings_t *, void *);
wristflow_save_state_t wristflow_product_settings_status(uint32_t, void *);
uint32_t wristflow_product_layout_request(const wristflow_layout_t *, void *);
wristflow_save_state_t wristflow_product_layout_status(uint32_t, void *);

#endif
