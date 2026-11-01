#ifndef WF_PRODUCT_STORAGE_ACCESS_H
#define WF_PRODUCT_STORAGE_ACCESS_H
#include <stdbool.h>
#include <stdint.h>
/* Thread context, services startup complete. Shares settings_db's recursive
 * mutex with candidate FS access, not every BLE/DFU/SDK Flash2 participant.
 * Caller owns PM. Never hold SDK rt_flash_lock around SDK write/erase. */
bool wf_product_storage_gate_acquire(uint32_t timeout_ms);
void wf_product_storage_gate_release(void);
#endif
