#include "aod_policy.h"

uint32_t wristflow_aod_wait_ms(const wristflow_settings_t *settings,
    const wristflow_watch_snapshot_t *clock, uint16_t subsecond_ms)
{
    if (!wristflow_settings_valid(settings) || settings->endurance_mode ||
        settings->aod_mode != WRISTFLOW_AOD_SCHEDULED || !clock || clock->time_unavailable ||
        clock->utc_seconds < WRISTFLOW_TIME_MIN || clock->utc_seconds > WRISTFLOW_TIME_MAX ||
        clock->hour_24 > 23 || clock->minute > 59 || subsecond_ms > 999) return UINT32_MAX;
    uint32_t elapsed = clock->utc_seconds % 60U * 1000U + subsecond_ms;
    if (wristflow_settings_aod_active(settings, clock)) return 60000U - elapsed;
    unsigned minute = clock->hour_24 * 60U + clock->minute;
    unsigned delta = (settings->aod_start_minute + 1440U - minute) % 1440U;
    return delta ? delta * 60000U - elapsed : 60000U - elapsed;
}
