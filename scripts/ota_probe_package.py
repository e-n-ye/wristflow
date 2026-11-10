"""Offline review package and tagged UART decoder. No serial/Flash entry point."""
import argparse
import hashlib
import json
from pathlib import Path
import secrets
import shutil
import struct
import zipfile
import zlib

from lab_evidence import file_record, verify_files
from lab_check import check_sdk
from ota_backup_review import review_pair
from ota_boot_check import check as check_boot
from ota_slot_check import check as check_slot

ROOT=Path(__file__).resolve().parents[1]
MAGIC=0x504f4657

def require(value,reason):
    if not value:raise ValueError(reason)

def load(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def save(path,value):
    with Path(path).open('x',encoding='utf-8') as stream:
        json.dump(value,stream,indent=2);stream.write('\n')

def pad(data,size):
    require(8<=len(data)<=size,'Image exceeds exact compatibility size')
    return data+b'\xff'*(size-len(data))

def freeze(result_path,directory,kind):
    result=load(result_path)
    lock=load(ROOT/'sdk.lock.json');check_sdk(ROOT,lock)
    require(result['kind']==kind and result['artifact_validation_passed'] is True and
        result['exit_code']==0 and result['deployable'] is False and not result['sdk_changes'],
        'A successful locked non-deployable build is required')
    require(result['board']==lock['board'],'Build board differs from lock')
    require(result['sdk_commit']==lock['sdk']['commit'],'Build SDK commit differs from lock')
    require(result['project_sources'] and result['artifacts'],'Empty build inputs/artifacts')
    verify_files(result['project_sources']+result['artifacts'])
    names={Path(item['path']).name for item in result['artifacts']}
    require(names=={'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','link_copy.lds'},'Unexpected artifact set')
    require(len(names)==len(result['artifacts']),'Duplicate artifact names')
    directory.mkdir(exist_ok=False)
    for item in result['artifacts']:
        target=directory/Path(item['path']).name
        shutil.copyfile(item['path'],target)
        require(hashlib.sha256(target.read_bytes()).hexdigest()==item['sha256'],'Frozen artifact changed during copy')
    original=Path(result_path).read_bytes()
    require(json.loads(original.decode('utf-8-sig'))==result,'Build record changed during freeze')
    (directory/'original-result.json').write_bytes(original)
    save(directory/'frozen-manifest.json',dict(original_result=file_record(directory/'original-result.json'),
        files=[file_record(directory/name) for name in sorted(names)]))
    return result

def prepare(out,first,second):
    require(not out.exists(),'Output directory must be new')
    review=review_pair(first,second)
    require(review['boot']['length']==58496,'Only the measured58496B boot contract is supported')
    out.mkdir(parents=True)
    data=first.read_bytes();tag=secrets.token_hex(16)
    require(len(data)==0x1000000 and hashlib.sha256(data).hexdigest()==review['flash_sha256'],'Backup changed before slicing')
    for name,start,size in [('restore-boot-64k.bin',0x10000,0x10000),('restore-b-first4k.bin',0x420000,0x1000)]:
        piece=data[start:start+size];require(len(piece)==size,'Incomplete restore range')
        (out/name).write_bytes(piece)
    save(out/'plan.json',dict(schema=1,tag_hex=tag,tag=[int(tag[i:i+8],16) for i in range(0,32,8)],
        backups=[file_record(first),file_record(second)],backup_review=review,
        restore=[file_record(out/name) for name in ('restore-boot-64k.bin','restore-b-first4k.bin')],
        deployable=False,device_write_authorized=False))

def bind(out):
    plan=load(out/'plan.json');verify_files(plan['backups']+plan['restore'])
    result=freeze(out/'slot-build/result.json',out/'frozen-slot','ota-slot-execution-probe')
    check=check_slot(out/'frozen-slot/main.elf',out/'frozen-slot/main.bin')
    require(result['probe_tag']==plan['tag_hex'] and check['tag']==plan['tag'],'Slot build tag differs')
    save(out/'slot-check.json',check)
    save(out/'binding.json',dict(schema=1,mode='literal-probe-only',tag=plan['tag'],
        probe=dict(path=str(out/'frozen-slot/main.bin'),bytes=check['bytes'],sha256=check['sha256'],
                   marker_address=check['symbols']['wf_ota_slot_marker'])))

def finish(out):
    plan=load(out/'plan.json');binding=load(out/'binding.json')
    verify_files(plan['backups']+plan['restore'])
    require(plan['tag']==binding['tag'],'Binding changed tag')
    slot=check_slot(out/'frozen-slot/main.elf',out/'frozen-slot/main.bin')
    require(slot['tag']==plan['tag'] and slot['sha256']==binding['probe']['sha256'],'Frozen slot changed')
    result=freeze(out/'boot-build/result.json',out/'frozen-boot','ram-ota-boot-prototype')
    require(result['probe_execution_enabled'] is True,'Experimental boot mode missing')
    boot=check_boot(out/'frozen-boot/main.elf',out/'frozen-boot/main.bin',out/'binding.json')
    require(boot['probe_execution_bound'],'Literal identity admission missing')
    save(out/'boot-check.json',boot)
    raw=(out/'frozen-boot/main.bin').read_bytes()
    compatible=pad(raw,plan['backup_review']['boot']['length'])
    (out/'boot-compatible-58496.bin').write_bytes(compatible)
    (out/'probe-b-sector.bin').write_bytes(pad((out/'frozen-slot/main.bin').read_bytes(),4096))
    review=dict(schema=1,tag=plan['tag'],deployable=False,hardware_verified=False,
        deployment_authorized=False,rom_rescue_verified=False,flash_commands_generated=False,
        mode='literal-probe-only',terminal_behavior='Send one EXECUTED frame then halt; original application requires explicit wired restoration of original boot.',
        boot=boot,probe=dict(bytes=slot['bytes'],sha256=slot['sha256'],marker_address=slot['symbols']['wf_ota_slot_marker']),
        files=[file_record(out/name) for name in ('boot-compatible-58496.bin','probe-b-sector.bin','restore-boot-64k.bin','restore-b-first4k.bin')],
        proposed_ranges=[dict(name='boot',start='0x12010000',allowed_bytes=65536,file_bytes=58496,nominal_4k_erase_bytes=61440),
                         dict(name='b_probe',start='0x12420000',allowed_bytes=4096,file_bytes=4096,nominal_4k_erase_bytes=4096)],
        preserved_regions=['FTAB','original application A','loader','FS','KVDB'],
        capture_contract=dict(frame_bytes=80,kinds=[1,2,3],uart='USART1 PA19 TX via CH340',baud=1000000,
                              format='8N1 TX only',total_flag_polls_per_frame=2000000),
        limits=['No production signature/trial journal or automatic rollback path.',
                'Polling work is bounded; physical baud, mapping, reset domains and forced ROM rescue remain unverified.',
                'Erase accounting assumes locked SDK4KiB NOR geometry; transport behavior still needs review before any device command.'])
    save(out/'review.json',review)
    source_dir=out/'source-snapshot';source_dir.mkdir()
    sources={item['path']:item for name in ('slot-build','boot-build') for item in load(out/name/'result.json')['project_sources']}
    for name in ('ota_probe_package.py','Prepare-Ota-Probe.ps1','ota_backup_review.py','lab_check.py','lab_evidence.py'):
        item=file_record(ROOT/'scripts'/name);sources[item['path']]=item
    verify_files(list(sources.values()))
    for index,item in enumerate(sources.values()):
        data=Path(item['path']).read_bytes()
        require(hashlib.sha256(data).hexdigest()==item['sha256'],'Source changed during snapshot')
        (source_dir/(str(index)+'-'+Path(item['path']).name)).write_bytes(data)
    save(out/'source-manifest.json',list(sources.values()))
    archive=out.with_suffix('.zip')
    members=[p for p in out.rglob('*') if p.is_file()]
    with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED) as target:
        for path in members:target.write(path,path.relative_to(out).as_posix())
    with zipfile.ZipFile(archive) as target:
        require(target.testzip() is None,'ZIP CRC failure')
        for path in members:require(target.read(path.relative_to(out).as_posix())==path.read_bytes(),'ZIP member changed')
    save(out/'archive.json',dict(file=file_record(archive),members=len(members),readback_equal=True))
    return review

def parse_capture(raw,review):
    require(len(raw)<=1048576,'UART capture exceeds1MiB')
    frames=[];offset=0;magic=struct.pack('<I',MAGIC)
    while True:
        start=raw.find(magic,offset)
        if start<0:break
        require(start+80<=len(raw),'Truncated result frame')
        blob=raw[start:start+80];words=struct.unpack('<20I',blob)
        require(words[19]==zlib.crc32(blob[:76]),'Result CRC mismatch')
        require(words[1]>>16==1 and list(words[2:6])==review['tag'],'Result version/tag mismatch')
        frames.append(words);offset=start+80
    require([f[1]&0xffff for f in frames]==[1,2,3],'Missing, duplicate, error or reordered stages')
    for kind,f in enumerate(frames,1):
        require(f[6]==kind and f[7]==1 and f[18]==review['probe']['bytes'],'Result phase/status/length mismatch')
        require(not f[11]&0x30000 and f[12]==1,'Cache/IRQ contract differs')
        require(list(f[13:16])==([0,0,0] if kind==1 else [0x20000,0x420000,0x400000]),'Alias readback differs')
        require(f[17]==review['probe']['marker_address'],'Marker address differs')
        if kind in (2,3):require(f[16]==0x42584950,'Mapped marker differs')
        if kind==3:
            require(0x12020400<=f[8]<0x12020000+review['probe']['bytes'] and
                    0x20001000<f[9]<=0x20002000 and not f[9]&7 and f[10]==0x12020000,'Probe execution context differs')
        else:
            require(0x20020000<=f[8]<0x20030000 and 0x20040000<f[9]<=0x20050000 and
                    0x20020000<=f[10]<0x20030000,'Boot context differs')
    return dict(passed=True,frame_count=3,frames=[list(f) for f in frames],hardware_verified=False,
                deployment_authorized=False,limits='Tagged recorded bytes only; device admission and identity are separate.')

def main():
    parser=argparse.ArgumentParser(description=__doc__);commands=parser.add_subparsers(dest='action',required=True)
    for action in ('prepare','bind','finish'):
        sub=commands.add_parser(action);sub.add_argument('--out',type=Path,required=True)
        if action=='prepare':
            sub.add_argument('--first',type=Path,required=True);sub.add_argument('--second',type=Path,required=True)
    sub=commands.add_parser('parse');sub.add_argument('--manifest',type=Path,required=True);sub.add_argument('--uart',type=Path,required=True);sub.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.action=='parse':save(args.output,parse_capture(args.uart.read_bytes(),load(args.manifest)))
    elif args.action=='prepare':prepare(args.out.resolve(),args.first.resolve(),args.second.resolve())
    elif args.action=='bind':bind(args.out.resolve())
    else:finish(args.out.resolve())
    return 0
if __name__=='__main__':raise SystemExit(main())
