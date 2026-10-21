/* Host input adapter for CURRENT core only. No legacy experiment wrapper. */
#include "battery_estimate.h"
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    unsigned seeded, mode, percent, index, mv, raw, known, good, charge;
    uint32_t sequence, filtered, sampled, now;
    if (scanf("%u %u %u %" SCNu32 " %" SCNu32 " %" SCNu32,
        &seeded, &mode, &percent, &sequence, &filtered, &sampled) != 6) return 2;
    wristflow_battery_estimator_t state = {.seeded=seeded != 0, .mode=(uint8_t)mode,
        .percent=(uint8_t)percent, .sequence=sequence,
        .filtered_decimillivolts=filtered, .sampled_ms=sampled};
    int fields;
    while ((fields=scanf("%u %" SCNu32 " %" SCNu32 " %" SCNu32 " %u %u %u %u %u",
        &index, &now, &sampled, &sequence, &mv, &raw, &known, &good, &charge)) == 9) {
        wristflow_power_snapshot_t power = {.voltage_valid=raw != 0, .millivolts=(uint16_t)mv,
            .voltage_sampled_ms=sampled, .voltage_sequence=sequence, .input_valid=known != 0,
            .input_good=good != 0, .charge=(wristflow_charge_display_t)charge};
        wristflow_battery_diagnostic_t d;
        wristflow_battery_estimate_diagnosed(&state, &power, now, &d);
        printf("%u %u %u %" PRIu32 " %u %u %u %u\n", index,
            power.estimate_valid, power.estimated_percent, d.filtered_decimillivolts,
            d.filter_valid, d.candidate, d.mode, d.decision);
    }
    return fields == EOF ? 0 : 2;
}
