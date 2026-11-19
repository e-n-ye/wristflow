"""Independent bounds and reference checks for the 05 journal DMA observation."""
from ota_first_install import ROOT, read_json, require, sha

ENTRY = "trace-journal-dma"
RESTORE = "restore-journal-dma"
BASELINE_SHA = "acad8c5aee3d81f1f9defed9923274a7b6982e12b688cf834313a27420f521ac"
BIN_SHA = "2c746b23817444558183117cf02d846a3e2c9acfe24877332f529574d220b93c"
CHECK = dict(passed=True, startup_trace_tag=[1744612099,1910445575,324627080,2531195360], handoff_trace=5)
PROTECTED = (("ftab",0,0x10000),("a-vectors",0x20000,0x21000),("receiver",0x820000,0x879000))
OWNED = (("stable-a",0xda0000,0xda2000),("other-record",0xda2000,0xda4000),
         ("b-body",0x420000,0x704000),("b-tail",0x81f000,0x820000),("boot",0x10000,0x1f000))


def baseline():
    evidence = read_json(ROOT/"docs/evidence/2026-11-19/ota-05-handoff-device.json")
    p = ROOT/evidence["restoration"]["capture"]/"media/after.bin"
    require(sha(p.read_bytes()) == BASELINE_SHA, "Restored 16 MiB reference changed")
    return p


def diagnostic_boot(path):
    data = path.read_bytes()
    require(len(data)==58272 and sha(data)==BIN_SHA, "Wrong journal DMA boot")
    return data.ljust(61440,b"\xff")


def chunks(ranges):
    for role,start,end in ranges:
        for off in range(start,end,65536):
            yield role,off,min(end,off+65536)


def transfers(directory, action, source, trace_path):
    require(action in (ENTRY,RESTORE) and sha(source)==BASELINE_SHA, "Unknown short window")
    reads, writes, current = [], [], {}
    for role,off,end in chunks(PROTECTED+OWNED):
        name = f"key-{off:08x}"
        data = (directory/(name+".bin")).read_bytes()
        require(len(data)==end-off, "Missing key range")
        installed_trace = role == "boot" and action == ENTRY and data == diagnostic_boot(trace_path)
        if not installed_trace and (action == ENTRY or role in ("ftab","a-vectors","receiver")):
            require(data==source[off:end], "Unexpected initial key bytes")
        reads.append((name,off,data))
        current[off] = data
    if action == ENTRY:
        spans = [("boot",0x10000,diagnostic_boot(trace_path))]
    else:
        spans = [(role,off,source[off:end]) for role,off,end in chunks(OWNED)]
    for role,off,data in spans:
        if data != current[off]:
            writes.append((role,off,data))
            reads.append((f"{role}-{off:08x}",off,data))
    if action == RESTORE:
        reads.extend((f"restored-{off:08x}",off,source[off:end]) for _,off,end in chunks(OWNED))
    return reads,writes


def pair_generations(frames):
    """Attach a write to its boot generation. Never pair observations across resets."""
    generation = None
    observed = []
    for row in frames:
        if row["point"] == "P":
            generation = None
        elif row["point"] == "G":
            generation = row["detail"]
        elif row["point"] == "y":
            observed.append((generation,row["write_address"]))
    return observed


def fault_comparison(directory):
    evidence = read_json(ROOT/"docs/evidence/2026-11-19/ota-05-handoff-device.json")["fault_media"]
    reference = (ROOT/evidence["path"]).read_bytes()
    require(sha(reference)==evidence["sha256"], "Archived 05 fault data changed")
    results = {}
    for role,off,end in chunks(OWNED):
        if role == "boot":
            continue
        data = (directory/f"key-{off:08x}.bin").read_bytes()
        results[role] = results.get(role,True) and data==reference[off:end]
    return results
