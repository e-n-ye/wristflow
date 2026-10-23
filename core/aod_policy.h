#ifndef WRISTFLOW_AOD_POLICY_H
#define WRISTFLOW_AOD_POLICY_H
#include "product_state.h"
/* UINT32_MAX means no AOD-owned deadline. In the active interval redraw at
 * the next wall minute; outside it wait directly for the next start boundary. */
uint32_t wristflow_aod_wait_ms(const wristflow_settings_t *, const wristflow_watch_snapshot_t *, uint16_t subsecond_ms);
#endif
