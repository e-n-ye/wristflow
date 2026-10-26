"""Execute the SDK MEMSIC candidate in an isolated M33 emulator; no device I/O."""
from __future__ import annotations

import argparse
from io import BytesIO
import json
import math
from pathlib import Path
import random
import struct
import subprocess
import time

from elftools.elf.elffile import ELFFile
import unicorn
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import (UC_CPU_ARM_CORTEX_M33, UC_ARM_REG_C1_C0_2,
    UC_ARM_REG_FPEXC, UC_ARM_REG_PC, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_S0)

from lab_check import check_sdk, load
from lab_evidence import file_record, write_json

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "vendor/SiFli-SDK/customer/peripherals/sensor/mmc36x0kj"
IDENTITY = [1,0,0,0,1,0,0,0,1]
EARTH = [0, .35, -.30]
BIAS = [.30, -.25, .15]
RADIUS = math.sqrt(sum(value*value for value in EARTH))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def error_angle(actual, expected):
    return abs((actual-expected+180) % 360-180)


def output_record(path):
    path = path.resolve()
    return file_record(path, ROOT if path.is_relative_to(ROOT) else None)


def rotate(matrix, vector):
    return [sum(row[index]*vector[index] for index in range(3)) for row in matrix]


def rotation(yaw, pitch=0, roll=0):
    # Body->world: right/forward/up, positive yaw clockwise from world north.
    y,p,r = map(math.radians, (yaw,pitch,roll))
    cy,sy,cp,sp,cr,sr = math.cos(y),math.sin(y),math.cos(p),math.sin(p),math.cos(r),math.sin(r)
    world = [[cy*cr+sy*sp*sr, sy*cp, cy*sr-sy*sp*cr],
             [-sy*cr+cy*sp*sr, cy*cp, -sy*sr-cy*sp*cr],
             [-cp*sr, sp, cp*cr]]
    return [list(row) for row in zip(*world)]  # World->body.


def random_rotation(rng):
    u,v,w = rng.random(),rng.random(),rng.random()
    x,y = math.sqrt(1-u)*math.sin(2*math.pi*v),math.sqrt(1-u)*math.cos(2*math.pi*v)
    z,q = math.sqrt(u)*math.sin(2*math.pi*w),math.sqrt(u)*math.cos(2*math.pi*w)
    return [[1-2*(y*y+z*z),2*(x*y-z*q),2*(x*z+y*q)],
            [2*(x*y+z*q),1-2*(x*x+z*z),2*(y*z-x*q)],
            [2*(x*z-y*q),2*(y*z+x*q),1-2*(x*x+y*y)]]


class Emulator:
    def __init__(self, blob, parameters=None, matrix=None):
        elf = ELFFile(BytesIO(blob))
        self.symbols = {s.name:s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M33)
        self.uc.mem_map(0,0x200000)
        self.uc.mem_map(0x20000000,0x40000)
        for segment in elf.iter_segments():
            if segment["p_type"] == "PT_LOAD":
                require(segment["p_vaddr"]+segment["p_memsz"] < 0x1ff000, "Unexpected probe segment")
                self.uc.mem_write(segment["p_vaddr"],segment.data())
        self.uc.reg_write(UC_ARM_REG_C1_C0_2,0xf << 20)
        self.uc.reg_write(UC_ARM_REG_FPEXC,1 << 30)
        self.stop = 0x1ff000
        self.uc.mem_write(self.stop,b"\x00\xbe")
        self.mapping_inputs = []
        address = self.symbols["OctagonMapping"] & ~1
        def mapping_entry(uc, address, size, data):
            self.mapping_inputs.append(struct.unpack("<f",struct.pack("<I",uc.reg_read(UC_ARM_REG_S0)))[0])
        self.uc.hook_add(UC_HOOK_CODE,mapping_entry,begin=address,end=address)
        self.put("probe_matrix",matrix or IDENTITY)
        self.put("probe_initial_parameters",parameters or [0,0,0,.5])
        self.call("probe_init")
        require(self.integer("probe_initial") == self.integer("probe_compass_initial") == 1,"Initialization failed")

    def call(self,name):
        self.uc.reg_write(UC_ARM_REG_SP,0x2003fff0)
        self.uc.reg_write(UC_ARM_REG_LR,self.stop|1)
        self.uc.emu_start(self.symbols[name]|1,self.stop,timeout=1000000,count=2000000)
        require(self.uc.reg_read(UC_ARM_REG_PC) == self.stop,"Emulator call did not return: "+name)

    def put(self,name,values):
        self.uc.mem_write(self.symbols[name],struct.pack("<"+"f"*len(values),*values))

    def floats(self,name,count):
        return list(struct.unpack("<"+"f"*count,bytes(self.uc.mem_read(self.symbols[name],count*4))))

    def integer(self,name):
        return struct.unpack("<i",bytes(self.uc.mem_read(self.symbols[name],4)))[0]

    def step(self,acc,mag,enabled=0):
        self.put("probe_acceleration",acc)
        self.put("probe_magnetic",mag)
        self.uc.mem_write(self.symbols["probe_enabled"],struct.pack("<i",enabled))
        self.mapping_inputs.clear()
        self.call("probe_step")
        value = dict(processed=self.integer("probe_processed"),accuracy=self.integer("probe_accuracy"),
                     saturation=self.integer("probe_saturation"),updated=self.integer("probe_updated"),
                     oriented=self.integer("probe_oriented"),calibrated=self.floats("probe_calibrated",3),
                     orientation=self.floats("probe_orientation",3),mapping_inputs=list(self.mapping_inputs))
        require(value["processed"] == value["oriented"] == 1,"Processing/orientation failed")
        require(all(math.isfinite(v) for v in value["calibrated"]+value["orientation"]),"Nonfinite output")
        if value["updated"] == 1:
            value["reliable_parameters"] = self.floats("probe_parameters",4)
            require(all(math.isfinite(v) for v in value["reliable_parameters"]),"Nonfinite reliable parameters")
        return value


def directions(blob, record):
    rows = []
    for yaw in [0,1,44,45,46,89,90,91,134,135,136,179,180,181,224,225,226,269,270,271,314,315,316,359]:
        for pitch,roll in [(0,0),(30,0),(0,-30),(30,-20)]:
            matrix = rotation(yaw,pitch,roll)
            record("direction_input",dict(yaw=yaw,pitch=pitch,roll=roll))
            peer = Emulator(blob)
            result = peer.step(rotate(matrix,[0,0,1]),rotate(matrix,EARTH))
            expected = math.degrees(math.atan2(matrix[1][0],matrix[1][1])) % 360
            result.update(yaw=yaw,pitch=pitch,roll=roll,expected=expected,
                          heading_error=error_angle(result["orientation"][0],expected))
            require(len(result["mapping_inputs"]) == 1,"Expected one internal yaw mapping")
            result["before_mapping_error"] = error_angle(result["mapping_inputs"][0],expected)
            rows.append(result)
            record("direction_output",result)
    return dict(rows=rows,max_error=max(row["heading_error"] for row in rows),
                before_mapping_max_error=max(row["before_mapping_error"] for row in rows),
                within_one_degree=all(row["heading_error"] <= 1 for row in rows))


def calibration(blob,kind,record,noise=0):
    peer = Emulator(blob)
    rng = random.Random(20261026)
    updates = []
    accuracies = set()
    count = 512
    for index in range(count):
        if kind == "three_dimensional":
            matrix = random_rotation(rng)
        elif kind == "smooth_three_dimensional":
            matrix = rotation(index*3,70*math.sin(index*.071),65*math.sin(index*.047))
        else:
            matrix = rotation(index*7 if kind == "horizontal" else 17)
        acc = rotate(matrix,[0,0,1])
        field = rotate(matrix,EARTH)
        magnetic = [field[h]+BIAS[h]+rng.gauss(0,noise) for h in range(3)]
        record("calibration_input",dict(kind=kind,noise=noise,sample=index+1,acceleration=acc,magnetic=magnetic))
        result = peer.step(acc,magnetic,1)
        record("calibration_output",result)
        accuracies.add(result["accuracy"])
        if result["updated"] == 1:
            updates.append(dict(sample=index+1,accuracy=result["accuracy"],parameters=result["reliable_parameters"]))
    parameters = updates[-1]["parameters"] if updates else None
    output = dict(kind=kind,noise_gauss=noise,samples=count,updates=updates,accuracy_levels=sorted(accuracies),
                  last_accuracy=result["accuracy"],last_reliable_parameters=parameters)
    if parameters:
        output.update(bias_error=[parameters[i]-BIAS[i] for i in range(3)],
                      radius_relative_error=abs(parameters[3]-RADIUS)/RADIUS)
        output["parameters_within_threshold"] = max(abs(v) for v in output["bias_error"]) <= .01 and output["radius_relative_error"] <= .02
        restored = Emulator(blob,parameters)
        residuals = []
        for yaw in (13,103,193,283):
            for pitch,roll in ((0,0),(25,-20),(-30,15)):
                matrix = rotation(yaw,pitch,roll)
                acc,field = rotate(matrix,[0,0,1]),rotate(matrix,EARTH)
                record("restore_input",dict(kind=kind,noise=noise,yaw=yaw,pitch=pitch,roll=roll,
                                            parameters=parameters,acceleration=acc,
                                            magnetic=[field[h]+BIAS[h] for h in range(3)]))
                result = restored.step(acc,[field[h]+BIAS[h] for h in range(3)],0)
                record("restore_output",result)
                residuals.append(max(abs(result["calibrated"][h]-field[h]) for h in range(3)))
        output["fresh_restore_max_field_error_gauss"] = max(residuals)
        output["restored_field_within_threshold"] = max(residuals) <= .01
    return output


def soft_matrix(blob,record):
    matrix = [1.1,.08,-.02,.03,.9,.04,-.05,.02,1.05]
    bias = [.1,-.08,.05]
    peer = Emulator(blob,bias+[.5],matrix)
    rows = []
    for field in ([.2,.3,-.25],[-.3,.18,.4],[.15,-.35,-.2]):
        record("soft_matrix_input",dict(matrix=matrix,bias=bias,field=field))
        result = peer.step([0,0,1],[field[h]+bias[h] for h in range(3)],0)
        record("soft_matrix_output",result)
        row_major = rotate([matrix[h:h+3] for h in (0,3,6)],field)
        transposed = rotate([list(row) for row in zip(*[matrix[h:h+3] for h in (0,3,6)])],field)
        transposed_raw = rotate([list(row) for row in zip(*[matrix[h:h+3] for h in (0,3,6)])],
                                [field[h]+bias[h] for h in range(3)])
        transposed_then_offset = [transposed_raw[h]-bias[h] for h in range(3)]
        rows.append(dict(field=field,output=result["calibrated"],row_major=row_major,transposed=transposed,
                         row_major_error=max(abs(a-b) for a,b in zip(result["calibrated"],row_major)),
                         transposed_error=max(abs(a-b) for a,b in zip(result["calibrated"],transposed)),
                         transposed_then_offset=transposed_then_offset,
                         transposed_then_offset_error=max(abs(a-b) for a,b in zip(result["calibrated"],transposed_then_offset))))
    return dict(rows=rows,input_matrix_preserved=peer.floats("probe_matrix",9) == list(struct.unpack("<9f",struct.pack("<9f",*matrix))))


def run(args):
    args.out.mkdir(parents=True,exist_ok=False)
    started = time.monotonic()
    report = dict(hardware_verified=False,product_build=False,sdk_changed=False,
                  limits="Synthetic M33 execution only; no physical timing, MMC5603 compatibility, license or Product memory proof. Numerical thresholds are project preflight gates, not vendor specifications.")
    def record(kind,value):
        report["last_context"] = dict(kind=kind,value=value)
        with (args.out/"trace.jsonl").open("a",encoding="utf-8") as log:
            log.write(json.dumps(report["last_context"],allow_nan=False)+"\n")
    try:
        report["sdk"] = check_sdk(ROOT,load(ROOT/"sdk.lock.json"))
        cc = args.cc.resolve()
        source = ROOT/"scripts/memsic_preflight"
        elf = args.out/"probe.elf"
        command = [str(cc),"-mcpu=cortex-m33","-mthumb","-mfpu=fpv5-sp-d16","-mfloat-abi=hard",
                   "-fshort-enums","-fshort-wchar","-Os","-ffunction-sections","-fdata-sections",
                   "-nostartfiles","--specs=nosys.specs","-Wl,-e,probe_init,--gc-sections",
                   "-Wl,-u,probe_step,-u,probe_math",f"-Wl,-Map={args.out/'probe.map'}",f"-I{PACKAGE}",
                   str(source/"probe.c"),str(source/"math_bridge.c"),str(PACKAGE/"MemsicAlgo.lib"),"-lm","-o",str(elf)]
        report.update(command=command,unicorn_version=unicorn.__version__,compiler_version=subprocess.check_output([str(cc),"--version"],text=True).splitlines()[0],
                      inputs=[file_record(path,ROOT) for path in (Path(__file__),source/"probe.c",source/"math_bridge.c",ROOT/"scripts/requirements-memsic-preflight.txt",PACKAGE/"MemsicAlgo.lib",PACKAGE/"MemsicAlgo.h",PACKAGE/"MemsicCompass.h")])
        with (args.out/"build.log").open("w",encoding="utf-8") as log:
            built = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=60)
        require(built.returncode == 0,"Standalone probe compilation failed")
        blob = elf.read_bytes()
        peer = Emulator(blob)
        peer.call("probe_math")
        actual = struct.unpack("<6d",bytes(peer.uc.mem_read(peer.symbols["probe_math_output"],48)))
        expected = [1.5,math.acos(.3),math.asin(-.4),math.sin(1.2),math.cos(-.7),math.atan2(-.3,.4)]
        require(all(math.isfinite(v) for v in actual),"Nonfinite math bridge output")
        report["math_bridge"] = dict(actual=list(actual),expected=expected,max_error=max(abs(a-b) for a,b in zip(actual,expected)))
        require(report["math_bridge"]["max_error"] <= 1e-12,"Math bridge numerical mismatch")
        print("Math bridge checked",flush=True)
        report["directions"] = directions(blob,record)
        print("Direction matrix checked",flush=True)
        report["calibration"] = []
        for kind,noise in [("stationary",0),("horizontal",0),("three_dimensional",0),("three_dimensional",.001),("smooth_three_dimensional",0)]:
            report["calibration"].append(calibration(blob,kind,record,noise))
            print(f"Calibration observed: {kind}, noise={noise}",flush=True)
        report["soft_matrix"] = soft_matrix(blob,record)
        report["execution_completed"] = True
        report["heading_gate_passed"] = report["directions"]["within_one_degree"]
        report["calibration_gate_passed"] = all(row.get("parameters_within_threshold") and row.get("restored_field_within_threshold") for row in report["calibration"] if row["kind"] in ("three_dimensional","smooth_three_dimensional"))
        report["passed"] = report["heading_gate_passed"] and report["calibration_gate_passed"]
        report["artifacts"] = [output_record(path) for path in (elf,args.out/"probe.map",args.out/"build.log")]
    except Exception as exc:
        report.update(passed=False,error=repr(exc))
    finally:
        report["elapsed_host_seconds"] = time.monotonic()-started
        if (args.out/"trace.jsonl").exists():
            report["trace"] = output_record(args.out/"trace.jsonl")
        write_json(args.out/"result.json",report)
    print(json.dumps({key:report.get(key) for key in ("passed","execution_completed","heading_gate_passed","calibration_gate_passed","error")}),flush=True)
    return 0 if report.get("passed") else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--cc",type=Path,required=True)
    raise SystemExit(run(parser.parse_args()))
