"""Bounded preparation for the explicit low-voltage/physical KEY experiment.

Default is UI simulation. Never flash, format, reboot, or manufacture a physical
screen-failure result. A passed handoff is not a completed discharge.
"""
import argparse
from pathlib import Path
import time

from battery_console import Console, Cancelled
from battery_lab import Journal
from cycle_short_lab import Device
from lab_evidence import file_record, verify_files, write_json

PHASES=[('prepare','确认版本与USB准备'),('arm','保存独立实验起点'),
        ('off','关闭常亮并自然息屏'),('off_timer','USB下息屏采样20秒'),
        ('wake','KEY唤醒与20分钟常亮'),('qualify','核对息屏采样已保存'),
        ('unplug','拔USB开启手电筒'),('handoff','交接低压观察与循环')]

class LowDevice(Device):
    def __init__(self,args):
        super().__init__(args)
        source=file_record(Path(__file__));guard=self.identity
        def identity():
            verify_files([source]);return guard()
        self.identity=identity;self.binding=dict(self.binding,low_adapter=source)

class Simulation:
    binding={'simulation':True}
    def action(self,action,out,session,cancel):
        self.calls=getattr(self,'calls',[])+[action]
        if cancel.is_set():raise Cancelled('Simulation cancelled')
        result=dict(passed=True,resources_closed=True)
        if action=='status':
            qualified=self.calls.count('status')>1
            result['status']=dict(state=2,session=session,accepted=12 if qualified else 1,
                saved=12 if qualified else 1,queued=0,dropped=0,reason=0,inhibit=0,
                low_mode=True,low_samples=4 if qualified else 0,off_samples=3 if qualified else 0,
                low_events=7 if qualified else 0,keep_remaining_ms=1198000 if qualified else 0)
        return result

def run(args,backend=None,console_type=Console):
    if not 0<args.session<2**32:raise ValueError('Explicit nonzero uint32 session required')
    simulated=args.port is None
    backend=backend or (Simulation() if simulated else LowDevice(args))
    journal=Journal(args.out,binding=dict(backend.binding,session=args.session,
        sample_seconds=5,operator_protocol='flashlight then repeat20minute keep; physical KEY failure -> immediate USB',
        adapter=file_record(Path(__file__))),mode='low-voltage-prepare')
    possible=owned=qualified=handed_off=False
    outcome,detail,phase='passed','','prepare'
    with console_type(journal,PHASES,mode='低压可用终点准备'+('模拟（无设备）' if simulated else '')) as console:
        print(console.url,flush=True)
        def prompt(key,text,timeout=180):
            nonlocal phase
            phase=key;console.prompt(key,text,timeout);console.stage(key,'complete')
        def action(name,directory):
            result=backend.action(name,args.out/directory,args.session,console.cancelled)
            if not result['passed'] or not result['resources_closed']:
                raise ValueError('No retry after uncertain device action: '+repr(result))
            return result
        def own_status(directory):
            nonlocal owned
            state=action('status',directory)['status']
            if state['session']!=args.session or state['state']!=2 or not state.get('low_mode') or state['inhibit']:
                raise ValueError('Owned low-mode ARMED state not confirmed')
            owned=True
            if state['dropped']:raise ValueError('Low experiment dropped records')
            return state
        try:
            prompt('prepare','保持原电池＋USB；已刷本次低压诊断候选且仅mount已有日志区。息屏显示AOD须关闭，若原来开启请记下，实验结束恢复原设置。显示／触摸正常，临时常亮已开启，在电池页等待6秒刷新ADC。此轮不要求FULL，也不会按3.6V主动压黑。现在不要拔线。')
            phase='arm';console.stage(phase,'running');possible=True
            journal.event('arm_intent',session=args.session,low_mode=True)
            action('arm-low','arm')
            for attempt in range(10):
                state=own_status(f'armed-status-{attempt}')
                if state['saved']>=1 and state['saved']==state['accepted'] and not state['queued']:break
                console.cancelled.wait(.25)
            else:raise ValueError('START not durably saved')
            armed_at=time.monotonic();console.stage(phase,'complete')
            prompt('off','USB保持连接，退出手电筒／提醒覆盖。关闭临时常亮，停在普通电池页，等自然黑屏；确认屏幕已黑后继续。暂不按KEY。',45)
            phase='off_timer'
            console.automatic(phase,'保持USB及自然黑屏20秒，不按键。独立模式每约5秒请求ADC并逐条提交；不把它当正常待机功耗。若意外亮屏或异常，请取消并告诉助手。',20)
            prompt('wake','确认刚才阶段屏幕持续黑。现在按KEY1，确认屏幕／触摸正常；在持续亮屏设置选择20分钟，再回电池页。准备好后确认。若KEY无法恢复，不要拔USB，立即取消。',60)
            phase='qualify';console.stage(phase,'running')
            state=own_status('off-qualified-status')
            if state.get('off_samples',0)<2 or state['saved']!=state['accepted'] or state['queued'] or not 1100000<=state.get('keep_remaining_ms',0)<=1200000:
                raise ValueError('OFF ADC/persistence or20minute keep qualification failed')
            qualified=True;console.stage(phase,'complete')
            prompt('unplug','串口已关闭，USB息屏采样及唤醒准备通过。现在拔USB，仅原电池，打开手电筒；白屏持续全亮后继续。不要改变电池接线。',120)
            if time.monotonic()-armed_at>300:raise TimeoutError('Preparation exceeded armed deadline')
            prompt('handoff','设备独立继续每约5秒记录，包括息屏，样本与事件逐条提交；实验会持有MCU IDLE，不是正常待机续航。你自行决定何时退出手电筒，再进入电池页开启20分钟常亮。正常到期后能按KEY唤醒，就重新开启20分钟，在同一会话循环；正常到期不是失效。若提前黑屏且KEY不能点亮，或到期后KEY不能点亮，立即接回USB并告诉助手。不要等待保护板切断，不重启／重刷。若你需要离开或出现异常，立即回USB。设备可能无法保存失效瞬间，只能核验最后成功提交的前缀；保持有人及时观察。整轮结果待回读。')
            handed_off=True;journal.event('low_handoff',session=args.session,device_running_verified=False)
        except BaseException as exc:
            outcome='cancelled' if isinstance(exc,(Cancelled,KeyboardInterrupt)) else 'timeout' if isinstance(exc,TimeoutError) else 'error'
            detail=repr(exc);journal.event('low_prepare_error',phase=phase,error=detail);console.stage(phase,outcome)
        outcome=console.seal(outcome);handed_off=handed_off and outcome=='passed'
        result=dict(passed=outcome=='passed',outcome=outcome,detail=detail,simulation=simulated,
            session=args.session,possible_arm=possible,owned=owned,off_sampling_qualified=qualified,
            handed_off=handed_off,device_may_be_active=possible,device_running_verified=False,
            recovery_required=possible and not handed_off,discharge_complete=False,
            limits='No reset/flash/format/charger/brightness control. Human physical failure and last validated prefix are separate facts; no chemical-empty/current/capacity proof. Errors never reopen or blindly stop; operator returns USB and assistant verifies.')
        write_json(args.out/'experiment.json',result)
        console.finish(outcome,'准备完成，设备独立记录；物理按键不能点亮后立即回USB。' if handed_off else
            '准备未通过。退出手电筒、保持／接回USB并告知助手；不要重启或重刷。主机关口不证明设备停止。'+detail)
        return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--session',type=int,required=True)
    p.add_argument('--port');p.add_argument('--location',default='1-1.1')
    for name in ('result','flash','build-archive','recorded-root'):p.add_argument('--'+name,type=Path)
    p.add_argument('--expected-bin');p.add_argument('--confirmed',action='store_true')
    args=p.parse_args()
    if args.port and not all((args.result,args.flash,args.build_archive,args.recorded_root,args.expected_bin,args.confirmed)):
        p.error('Device requires explicit build/flash/archive/root/hash and current field confirmation')
    return 0 if run(args)['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
