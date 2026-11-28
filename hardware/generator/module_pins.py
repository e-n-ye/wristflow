"""Approved changes relative to the archived EPRO, keyed by physical U1 pad."""

# Keep historical net labels stable across sheets and the rear FPC.
# Each tuple is (logical net, original land, B-R2 scheme-two land).
MODULE_MOVES = (
    ("PA_09_TP_RSTN", "38", "18"),
    ("PA_42", "4", "31"),
    ("PA_20_VIB_PWM", "28", "10"),
    ("PA_31", "25", "12"),
    ("CROWN_A", "18", "13"),
    ("CROWN_B", "19", "16"),
    ("PA_10_I2C0_SCL", "32", "23"),
    ("PA_11_I2C0_SDA", "31", "22"),
    ("PA_30", "24", "19"),
    ("PA_38", "10", "28"),
)
RESET_LANDS = {"TP16": "RTS_N_OD", "TP17": "GND"}


def module_nets(original):
    result = original.copy()
    for name, old, new in MODULE_MOVES:
        if original[old] != name:
            raise ValueError(f"Unexpected source U1.{old}: {original[old]}")
        result[old] = ""
    for name, old, new in MODULE_MOVES:
        if result.get(new):
            raise ValueError(f"U1.{new} already assigned to {result[new]}")
        result[new] = name
    return result
