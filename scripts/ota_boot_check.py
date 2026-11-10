"""Validate the diagnostic RAM boot ELF, never access or deploy a device."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct

RAM_CODE = (0x20020000, 0x20030000)
RAM_DATA = (0x20040000, 0x20050000)


def require(value, reason):
    if not value:
        raise ValueError(reason)


def leaf_block(disassembly, name, *, final_jump=False, byte_size=None):
    match = re.search(r"(?m)^([0-9a-f]+) <" + re.escape(name) + r">:\n(.*?)(?=^[0-9a-f]+ <|\Z)", disassembly, re.S | re.M)
    require(match, "Missing function disassembly: " + name)
    start = int(match[1], 16)
    lines = match[2].splitlines()
    instructions = []
    for line in lines:
        insn = re.match(r"\s*([0-9a-f]+):\s+([a-z.][a-z0-9.]*)\s*(.*)", line)
        if insn:
            instructions.append((int(insn[1], 16), insn[2], insn[3]))
    require(instructions, "Empty function: " + name)
    end = start + byte_size if byte_size else instructions[-1][0] + 2
    require(RAM_CODE[0] <= start < end <= RAM_CODE[1], "Function outside boot RAM: " + name)
    for _, op, args in instructions:
        require(op not in ("bl", "bl.w", "blx"), "Call inside RAM transition: " + name)
        require(not op.startswith("."), "Embedded data inside RAM transition: " + name)
        require("[pc" not in args and op not in ("tbb", "tbh"), "Literal/table dependency inside transition: " + name)
        if op.startswith("b") and op not in ("bic", "bic.w", "bics", "bfi", "bfc"):
            if op == "bx":
                require(args.strip() == ("r3" if final_jump else "lr"), "Unexpected indirect branch: " + name)
            else:
                target = re.match(r"([0-9a-f]+)\s", args)
                require(target and start <= int(target[1], 16) < end, "External branch inside transition: " + name)
        if op in ("cbz", "cbnz"):
            target = re.search(r",\s*([0-9a-f]+)\s", args)
            require(target and start <= int(target[1], 16) < end, "External compare branch: " + name)
    require(any(op.startswith("dsb") for _, op, _ in instructions) and
            any(op.startswith("isb") for _, op, _ in instructions), "Missing barriers: " + name)
    if final_jump:
        require(instructions[-1][1:] == ("bx", "r3"), "Unexpected jump tail")
    return {"start": hex(start), "instructions": len(instructions)}


def jump_unreferenced(disassembly, start, size):
    for match in re.finditer(r"(?m)^\s*([0-9a-f]+):\s+[^\n]*<wf_ota_ram_jump(?:\+|>)", disassembly):
        require(start <= int(match[1], 16) < start + size, "Diagnostic code references jump")


def check(elf, binary=None, binding=None):
    def output(tool, *args):
        return subprocess.check_output([tool, *args, str(elf)], text=True, encoding="utf-8")
    symbols = output("arm-none-eabi-nm", "-S")
    disassembly = output("arm-none-eabi-objdump", "-d", "-z", "--no-show-raw-insn")
    sizes = {}
    for name in ("entry", "hw_preinit0", "wf_ota_ram_apply", "wf_ota_ram_jump", "wf_ota_boot_plan"):
        match = re.search(r"(?m)^([0-9a-f]+) ([0-9a-f]+) [Tt] " + name + r"$", symbols)
        require(match and RAM_CODE[0] <= int(match[1], 16) < int(match[1], 16) + int(match[2], 16) <= RAM_CODE[1], "Missing/RAM placement error: " + name)
        sizes[name] = int(match[2], 16)
    match = re.search(r"(?m)^([0-9a-f]+) ([0-9a-f]+) [BbDd] wf_ota_boot_probe_result$", symbols)
    require(match and RAM_DATA[0] <= int(match[1], 16) < int(match[1], 16) + int(match[2], 16) <= RAM_DATA[1], "Probe record outside RAM data")
    forbidden = r"\b(boot_images_help|select_boot|run_img|HAL_FLASH_ALIAS_CFG|HAL_FLASH_SET_ALIAS_RANGE|HAL_FLASH_SET_ALIAS_OFFSET|aw32001_init|aw32001_charge_enable|rt_flash_write|rt_flash_erase|dfu_flash_update)\s*$"
    require(not re.search(forbidden, symbols, re.M), "Legacy route, charging or mutation symbol linked")
    require(not re.search(r"\b__fsym_\w+\s*$", symbols, re.M), "Shell export linked")
    # Every allocated executable/readonly section must execute/load from boot
    # RAM; initialized writable sections may use RAM_DATA with a RAM_CODE LMA.
    sections = output("arm-none-eabi-objdump", "-h")
    load_ends=[]
    for match in re.finditer(r"(?m)^\s*\d+ (\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)[^\n]*\n\s+([^\n]+)", sections):
        name, size, vma, lma, flags = match.groups()
        size, vma, lma = (int(value, 16) for value in (size, vma, lma))
        if size and "ALLOC" in flags:
            limits = RAM_CODE if "READONLY" in flags or "CODE" in flags or name in (".copy.table", ".zero.table") else RAM_DATA
            require(limits[0] <= vma < vma + size <= limits[1], "Section placement: " + name)
            if "LOAD" in flags:
                require(RAM_CODE[0] <= lma < lma + size <= RAM_CODE[1], "Section load address: " + name)
                load_ends.append(lma+size)
    # Entry never references the retained jump function: structural vector
    # plausibility alone must not become permission to boot an image.
    jump = re.search(r"(?m)^([0-9a-f]+) [0-9a-f]+ [Tt] wf_ota_ram_jump$", symbols)
    jump_unreferenced(disassembly, int(jump[1], 16), sizes["wf_ota_ram_jump"])
    extra={"probe_execution_bound":False}
    if binary is not None:
        data=binary.read_bytes()
        require(load_ends and max(load_ends)-RAM_CODE[0]==len(data),"BIN differs from ELF LOAD extent")
        sp,pc=struct.unpack_from('<II',data)
        require(sp==0x20042000 and pc&1 and RAM_CODE[0]<=pc-1<pc+1<=RAM_CODE[0]+len(data),"Boot BIN vectors invalid")
        reset=re.search(r'(?m)^([0-9a-f]+) [0-9a-f]+ [Tt] Reset_Handler$',symbols)
        require(reset and (pc&~1)==int(reset[1],16),'Boot vector bypasses Reset_Handler')
        entry=struct.unpack_from('<I',elf.read_bytes(),24)[0]
        require((entry&~1)==(pc&~1),'Boot ELF entry bypasses reset initialization')
        extra.update(bin_bytes=len(data),bin_sha256=hashlib.sha256(data).hexdigest(),initial_sp=hex(sp),reset_pc=hex(pc))
    if binding is not None:
        require(binary is not None,"Probe binding requires boot BIN")
        item=json.loads(binding.read_text(encoding='utf-8'))
        probe=Path(item['probe']['path']).read_bytes()
        require(hashlib.sha256(probe).hexdigest()==item['probe']['sha256'],"Bound probe changed")
        symbol=re.search(r'(?m)^([0-9a-f]+) ([0-9a-f]+) [RrTt] wf_ota_probe_expected$',symbols)
        require(symbol and int(symbol[2],16)==len(probe),"Missing exact literal probe identity")
        offset=int(symbol[1],16)-RAM_CODE[0]
        require(0<=offset<=len(data)-len(probe) and data[offset:offset+len(probe)]==probe,"Embedded probe bytes differ")
        tag=re.search(r'(?m)^([0-9a-f]+) 00000010 [RrTt] wf_ota_probe_tag$',symbols)
        require(tag,"Missing probe tag")
        offset=int(tag[1],16)-RAM_CODE[0]
        require(struct.unpack_from('<4I',data,offset)==tuple(item['tag']),"Boot tag differs")
        jump_probe=re.search(r'(?m)^([0-9a-f]+) ([0-9a-f]+) [Tt] wf_ota_ram_jump_probe$',symbols)
        require(jump_probe,"Missing masked-IRQ probe handoff")
        extra['probe_jump']=leaf_block(disassembly,'wf_ota_ram_jump_probe',final_jump=True,byte_size=int(jump_probe[2],16))
        block=re.search(r'<wf_ota_ram_jump_probe>:\n(.*?)(?=^[0-9a-f]+ <|\Z)',disassembly,re.S|re.M)[1]
        require('cpsie' not in block,"Probe handoff unmasks IRQs")
        require(re.search(r'<wf_ota_ram_jump_probe>',disassembly[:disassembly.find('<wf_ota_ram_jump_probe>:')]) or
                len(re.findall(r'<wf_ota_ram_jump_probe>',disassembly))>1,"Probe execution route missing")
        extra['probe_execution_bound']=True
    else:
        require(not re.search(r'\bwf_ota_ram_jump_probe$',symbols,re.M),"Unexpected execution code in diagnostic build")
    return {"elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(), "ram_placement_passed": True,
            "apply": leaf_block(disassembly, "wf_ota_ram_apply", byte_size=sizes["wf_ota_ram_apply"]),
            "jump": leaf_block(disassembly, "wf_ota_ram_jump", final_jump=True, byte_size=sizes["wf_ota_ram_jump"]),
            "jump_admission_bound": False, "hardware_verified": False, **extra}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bin", type=Path)
    parser.add_argument("--probe-binding", type=Path)
    args = parser.parse_args()
    try:
        report = dict(passed=True, **check(args.elf,args.bin,args.probe_binding))
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        report = {"passed": False, "error": str(exc)}
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
