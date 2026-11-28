"""Approved native-KiCad pin assignments relative to the archived EPRO."""

# Keep historical net labels stable across sheets and the rear FPC.
# Each tuple is (logical net, original land, current B-R2 land).
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
    ("PA_40_I2C1_SCL", "8", "24"),
    ("PA_39_I2C1_SDA", "9", "25"),
)
RESET_LANDS = {"TP16": "RTS_N_OD", "TP17": "GND"}

# Both terminals of each main-board spring are electrically common.
_TAIL = {
    "6": ("PA_40_I2C1_SCL", "PPG_INT_N"),
    "7": ("PA_39_I2C1_SDA", "PA_40_I2C1_SCL"),
    "8": ("PPG_INT_N", "PA_39_I2C1_SDA"),
}
_KEY = ("GND", "KEY_N")
_GROUND = ("KEY_N", "GND")
INTERFACE_PINS = {
    "main": {"J2": _TAIL, "K2": {"1": _KEY, "2": _KEY}, "K4": {"1": _GROUND, "2": _GROUND}},
    "rear": {"JF1": _TAIL},
    "crown": {"JC2": {"1": _KEY}, "JC4": {"1": _GROUND}},
    "bridge": {"P2": {"1": _KEY}, "JC12": {"1": _KEY}, "P4": {"1": _GROUND}, "JC14": {"1": _GROUND}},
}


def interface_net(which, ref, pin, original):
    change = INTERFACE_PINS[which].get(ref, {}).get(pin)
    if change is None:
        return original
    old, new = change
    if original != old:
        raise ValueError(f"{which} {ref}.{pin}: expected {old}, got {original}")
    return new


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
