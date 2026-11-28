"""Native pad order and routing limits for the five display lanes."""

QSPI_PADS = {name: (ref + ".1", ref + ".2") for name, ref in
             zip(("CLK", "D0", "D1", "D2", "D3"), ("R18", "R19", "R20", "R16", "R17"))}


def check_scheme3_leg(name, source, layers, via_count):
    allowed = {"F", "B"} if source else ({"B", "L4"} if name in ("D0", "D1") else {"B"})
    limit = 1 if source else 2
    if not layers or layers - allowed or via_count > limit:
        raise ValueError(f"{name} {'source' if source else 'display'} leg: {layers}, {via_count} vias")
