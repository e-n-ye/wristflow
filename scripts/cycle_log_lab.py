"""Bounded, explicitly bound CycleLog diagnostics and immutable full-file export."""
import argparse
import json
from pathlib import Path
import re
import secrets
import threading
import time

from battery_lab import Journal
from lab_evidence import file_record,verify_files,write_json
from lab_serial import RecordedClient,RecordedSession,SerialTransport,lease_directory
from serial_lab import prepare_capture
from cycle_protocol import parse_info,parse_page,decode_file,csv_text

class CycleClient(RecordedClient):
    COMMAND_PATTERN=r'wf_cycle (?:status|probe|init|mount|smoke [1-9]\d* \d+|arm(?:-low)? [1-9]\d*|stop [1-9]\d*|prefix [1-9]\d*|info (?:smoke|cycle(?: [1-9]\d*)?)|page (?:smoke|cycle) [1-9]\d* \d+ [1-4])'
    BOOT_MARKERS=(b'[product] storage=',b'SFBL')

def command(client,text,code,*,deadline=None):
    lines=client.request('wf_cycle '+text,lambda line: '[cycle-done]' in line,deadline=deadline)
    done=[re.search(r'\[cycle-done\] cmd=(\d+) result=(-?\d+)$',line) for line in lines if '[cycle-done]' in line]
    accepted=[re.search(r'\[cycle-accepted\] cmd=(\d+) accepted=([01])$',line) for line in lines if '[cycle-accepted]' in line]
    if len(done)!=1 or not done[0] or int(done[0][1])!=code or int(done[0][2])!=0:
        raise ValueError('Cycle command failed: '+repr(lines))
    if len(accepted)!=1 or not accepted[0] or int(accepted[0][1])!=code or accepted[0][2]!='1':
        raise ValueError('Cycle command not uniquely accepted')
    return lines

def status(client):
    lines=client.request('wf_cycle status',lambda line:'[cycle-prefix]' in line)
    matches=[re.search(r'\[cycle-status\] state=(\d+) session=(\d+) accepted=(\d+) saved=(\d+) queued=(\d+) dropped=(\d+) reason=(\d+) inhibit=([01])$',line) for line in lines if '[cycle-status]' in line]
    if len(matches)!=1 or matches[0] is None:raise ValueError('Cycle status malformed')
    values=list(map(int,matches[0].groups()))
    result=dict(zip(('state','session','accepted','saved','queued','dropped','reason','inhibit'),values))
    modes=[re.search(r'\[cycle-mode\] low=([01]) commit_elapsed_ms=(\d+) keep_remaining_ms=(\d+)$',line) for line in lines if '[cycle-mode]' in line]
    if modes:
        if len(modes)!=1 or modes[0] is None:raise ValueError('Cycle mode malformed')
        result.update(low_mode=bool(int(modes[0][1])),commit_elapsed_ms=int(modes[0][2]),keep_remaining_ms=int(modes[0][3]))
    lows=[re.search(r'\[cycle-low\] samples=(\d+) off_samples=(\d+) events=(\d+)$',line) for line in lines if '[cycle-low]' in line]
    if lows:
        if len(lows)!=1 or lows[0] is None:raise ValueError('Cycle low counters malformed')
        result.update(low_samples=int(lows[0][1]),off_samples=int(lows[0][2]),low_events=int(lows[0][3]))
    return result

def recovery_info(lines,session,info):
    values=[re.search(r'\[cycle-recovery\] id=(\d+) file_bytes=(\d+) prefix_bytes=(\d+) damaged_tail=([01])$',line)
        for line in lines if '[cycle-recovery]' in line]
    if len(values)!=1 or values[0] is None:raise ValueError('One recovery manifest required')
    m=values[0];sid,total,prefix=int(m[1]),int(m[2]),int(m[3]);damaged=bool(int(m[4]))
    if sid!=session or prefix!=info['bytes'] or not prefix<=total<=0x400000 or damaged!=(prefix!=total):
        raise ValueError('Recovery manifest range')
    return dict(file_bytes=total,prefix_bytes=prefix,damaged_tail=damaged)

def collect(out,transport,*,binding,identity,locks,action,kind='smoke',session=None,seconds=30,cancel=None):
    if action not in ('probe','init','mount','smoke','arm','arm-low','stop','status','export','recover'):raise ValueError('Cycle action')
    if not 10<=seconds<=120:raise ValueError('Smoke duration 10..120 seconds')
    if session is not None and not 0<session<2**32:raise ValueError('Session uint32 required')
    if binding.get('simulation') is not True:
        build=json.loads(Path(binding['build']['build_result']['path']).read_text(encoding='utf-8-sig'))
        if build.get('cycle_log') is not True or any(build.get(x) for x in ('fs_probe','storage_probe','resource_probe')):
            raise ValueError('Exclusive CycleLog build required')
    if action in ('arm','arm-low','stop','recover') and not session:raise ValueError('Explicit session ID required')
    if action=='recover' and kind!='cycle':raise ValueError('Recovery requires named cycle')
    sid=session or secrets.randbelow(2**32-1)+1
    journal=Journal(out,binding=dict(binding,action=action,kind=kind,session=sid,seconds=seconds),mode='cycle_log')
    cancel=cancel or threading.Event();runner=RecordedSession(journal,transport,cancel,CycleClient,identity=identity,locks=locks,timeout=60)
    result=dict(passed=False,action=action,simulation=binding.get('simulation',False),session=sid)
    try:
        with runner as client:
            if action in ('probe','init','mount'): command(client,action,{'probe':1,'init':2,'mount':3}[action])
            elif action=='status': result['status']=status(client)
            elif action in ('arm','arm-low','stop'): command(client,f'{action} {sid}',{'arm':5,'stop':6,'arm-low':9}[action])
            elif action=='smoke':
                command(client,f'smoke {sid} {seconds}',4)
                deadline=time.monotonic()+seconds+30
                while time.monotonic()<deadline:
                    observed=status(client);result['status']=observed
                    if observed['session']!=sid:raise ValueError('Smoke session changed')
                    if observed['state'] in (5,6):break
                    time.sleep(1)
                if result['status']['state']!=5 or result['status']['dropped'] or result['status']['saved']!=result['status']['accepted']:
                    raise ValueError('Smoke not durably complete')
            elif action in ('export','recover'):
                deadline=time.monotonic()+1200
                selector='info '+kind+(f' {session}' if kind=='cycle' and session is not None else '')
                code=7
                if action=='recover':selector=f'prefix {session}';code=10
                first=command(client,selector,code,deadline=deadline)
                info=parse_info(first);result['info']=info
                recovered=recovery_info(first,session,info) if action=='recover' else None
                if session is not None and session!=info['session']:raise ValueError('Explicit cycle session differs from file')
                result['session']=info['session'];raw=bytearray()
                for offset in range(0,info['rows'],4):
                    if time.monotonic()>=deadline:raise TimeoutError('Cycle whole export 20min deadline')
                    n=min(4,info['rows']-offset)
                    raw.extend(parse_page(command(client,f'page {kind} {info["session"]} {offset} {n}',8,deadline=deadline),info['session'],offset,n))
                final=command(client,selector,code,deadline=deadline)
                if parse_info(final)!=info:raise ValueError('Cycle file changed during export')
                if action=='recover' and recovery_info(final,session,info)!=recovered:raise ValueError('Recovery tail changed')
                rows=decode_file(raw,info)
                if time.monotonic()>=deadline:raise TimeoutError('Cycle whole export completed outside deadline')
                (out/'cycle.bin').write_bytes(raw);(out/'cycle.csv').write_text(csv_text(rows),encoding='utf-8',newline='')
                result.update(record_complete=info['terminal'],rows=len(rows),stop_reason=rows[-1]['stop_reason'] if info['terminal'] else None)
                if recovered is not None:
                    result.update(recovery=recovered,prefix_only=True,record_complete=info['terminal'] and not recovered['damaged_tail'])
            result['passed']=True
    except BaseException as exc:
        result['error']=repr(exc);result['passed']=False
    result['resources_closed']=runner.resources_closed
    result['passed']=result['passed'] and runner.resources_closed
    write_json(out/'report.json',result);journal.event('finished',**result)
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--action',choices=('probe','init','mount','smoke','arm','arm-low','stop','status','export','recover'),required=True)
    p.add_argument('--kind',choices=('smoke','cycle'),default='smoke');p.add_argument('--session',type=int)
    p.add_argument('--seconds',type=int,default=30);p.add_argument('--out',type=Path,required=True)
    for key in ('port','location','expected-bin'):p.add_argument('--'+key,required=True)
    for key in ('result','flash','source-archive','build-archive','recorded-root'):
        p.add_argument('--'+key,type=Path,required=key in ('result','flash'))
    p.add_argument('--confirm-same-board-and-firmware',action='store_true',required=True)
    args=p.parse_args()
    binding,guard=prepare_capture(args.result,args.flash,args.expected_bin,args.port,args.location,args.confirm_same_board_and_firmware,
        source_archive=args.source_archive,build_archive=args.build_archive,recorded_root=args.recorded_root)
    own=[file_record(Path(__file__)),file_record(Path(__file__).with_name('cycle_protocol.py'))]
    def identity():verify_files(own);return guard()
    binding['cycle_tools']=own
    r=collect(args.out,SerialTransport(args.port),binding=binding,identity=identity,locks=lease_directory(),action=args.action,
        kind=args.kind,session=args.session,seconds=args.seconds)
    print(json.dumps(r,ensure_ascii=False,indent=2));return 0 if r['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
