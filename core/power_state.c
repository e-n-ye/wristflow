#include "power_state.h"

bool wristflow_power_equal(const wristflow_power_snapshot_t *a, const wristflow_power_snapshot_t *b)
{
    return a->input_valid == b->input_valid && a->input_good == b->input_good &&
        a->voltage_valid == b->voltage_valid && a->millivolts == b->millivolts;
}

const char *wristflow_power_input_text(const wristflow_power_snapshot_t *power)
{
    return !power->input_valid ? "USB ?" : power->input_good ? "USB OK" : "USB --";
}
