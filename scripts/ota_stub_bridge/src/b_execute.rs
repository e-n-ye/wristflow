//! Fixed probe boot/install/restore; all Flash operations use the pinned library.
mod port;
mod simulated;
mod common;
mod sector;
use serde::Deserialize;
use serde_json::{Value,json};
use sha2::{Digest,Sha256};
use sftool_lib::{BeforeOperation,CancelToken,SifliToolBase,ReadFlashFile,ReadFlashParams};
use sftool_lib::common::ram_command::{DownloadStub,RamCommand,Command,Response};
use sftool_lib::common::sifli_debug::{SifliDebug,SifliUartCommand};
use sftool_lib::read_flash::ReadFlashTrait;
use sftool_lib::sf32lb52::SF32LB52Tool;
use std::{fs,io::{self,Write},path::{Path,PathBuf},thread,time::{Duration,Instant}};
type Result<T>=std::result::Result<T,Box<dyn std::error::Error>>;
const STUB_SHA:&str="d4db9c27a0539da431fd253ee4b09c8d3ddb033bf0dc1ae4cc3bf322fa069e43";
const BOOT_SHA:&str="db747d9070aeb3642a64e765045a0b6671b922d16f72d5860e6e3bbb334c2303";
const ORIGINAL_BOOT_SHA:&str="f59f948f987362f1270b1da541405e82bd887c1a16c306364c04913c9c07f667";
const ORIGINAL_B_SHA:&str="154ff4e9b4b26b0a5f13ce0f4ec94f32418d48cbabdb276ae48f842879713070";
const BASE_SHA:&str="9140bb4e1bb700c8a7b1d06f29f510c3f18b13fb6da86e34b7f453fec639ab74";
const REV:&str="be2adab37bfb0ef4eb623b7e47e7b8693918b1e1";
const TAG:[u32;4]=[2306271408,2453024267,2305287774,1289714410];
const BOOT:u32=0x12010000;
const B:u32=0x12420000;
const K64:usize=65536;
// The11288B original FTAB includes image headers at0x1000 and running
// pointers at0x2c00. Its first4KiB alone cannot bind boot length/routing.
const FTAB_BYTES:usize=12288;
pub const SCENARIOS:&[&str]=&["success","fragmented","before-changed","b-compare","boot-compare","write-fail",
    "restore-boot-fail","restore-b-fail","restore-read-fail","alias-retained","no-frames","bad-tag","bad-crc","short-frames","duplicate-frames","execute-hang",
    "initial-register-fail","restore-register-fail","restore-alias-fail","probe-start-fail","original-start-fail",
    "boot-header-changed","boot-route-changed"];
fn require(ok:bool,message:&str)->Result<()> {if ok {Ok(())}else{Err(io::Error::other(message).into())}}
fn hash(data:&[u8])->String {format!("{:x}",Sha256::digest(data))}
fn can_start_original(clean:bool,r:&Value,t:&port::Trace)->bool {clean && r["restored_verified"]==true && t.fault().is_none()}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Input {path:PathBuf,bytes:usize,sha256:String}
fn input(v:&Input)->Result<Vec<u8>> {let b=fs::read(&v.path)?;require(b.len()==v.bytes && hash(&b)==v.sha256,"Bound input changed")?;Ok(b)}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Config {
    schema:u32,simulation:bool,port:String,out:PathBuf,stub:Input,baseline:Input,candidate:Input,boot:Input,
    restore_boot:Option<Input>,restore_b:Option<Input>,recovery_only:bool,cancel:PathBuf,seconds:u64,listen_seconds:u64,scenario:String,
    entry_reset_authorized:bool,ram_stub_authorized:bool,register_observation_authorized:bool,
    b_sector_write_authorized:bool,boot_write_authorized:bool,probe_start_authorized:bool,
    restore_entry_authorized:bool,restore_writes_authorized:bool,original_start_authorized:bool,
}
fn validate(c:&Config)->Result<(Vec<u8>,Vec<u8>,Vec<u8>,Vec<u8>)> {
    require(c.schema==1 && (20..=180).contains(&c.seconds) && (1..=15).contains(&c.listen_seconds),"Finite execution config required")?;
    require(c.entry_reset_authorized && c.ram_stub_authorized && c.register_observation_authorized &&
        c.restore_writes_authorized && c.original_start_authorized,"Explicit entry, restoration and original-start scope required")?;
    require(c.recovery_only || (c.b_sector_write_authorized && c.boot_write_authorized && c.probe_start_authorized && c.restore_entry_authorized),
        "Explicit B/boot write, probe start AND second restoration entry required")?;
    require(SCENARIOS.contains(&c.scenario.as_str()) && (c.simulation || c.scenario=="success"),"Unknown or real fault injection")?;
    if c.simulation {require(c.port=="SIMULATED-STUB","Simulation cannot name a device")?;}
    else {let n=c.port.strip_prefix("COM").unwrap_or("");require(cfg!(windows) && !n.is_empty() && !n.starts_with('0') && n.bytes().all(|v|v.is_ascii_digit()),"Canonical Windows COM required")?;}
    require(!c.out.exists(),"Fresh output required")?;
    require(c.stub.bytes==132816 && c.stub.sha256==STUB_SHA && c.baseline.bytes==0x1000000 && c.baseline.sha256==BASE_SHA,"Pinned stub and original complete backup required")?;
    require(c.candidate.bytes==4096 && c.candidate.sha256==sector::CANDIDATE_SHA && c.boot.bytes==58496 && c.boot.sha256==BOOT_SHA,"Fixed006 probe and compatible boot only")?;
    if c.recovery_only {
        let boot=c.restore_boot.as_ref().ok_or("Missing prior-window original boot")?;
        let b=c.restore_b.as_ref().ok_or("Missing prior-window original B")?;
        require(boot.bytes==K64 && boot.sha256==ORIGINAL_BOOT_SHA && b.bytes==4096 && b.sha256==ORIGINAL_B_SHA,"Exact prior-window original ranges required")?;
        require(hash(&input(boot)?)==ORIGINAL_BOOT_SHA && hash(&input(b)?)==ORIGINAL_B_SHA,"Prior-window originals changed")?;
    } else {require(c.restore_boot.is_none() && c.restore_b.is_none(),"Exercise uses only its own fresh originals")?;}
    Ok((input(&c.stub)?,input(&c.baseline)?,input(&c.candidate)?,input(&c.boot)?))
}
fn save(path:&Path,data:&[u8])->Result<()> {let mut f=fs::File::create_new(path)?;f.write_all(data)?;f.sync_all()?;Ok(())}
fn checkpoint(c:&Config,r:&Value)->Result<()> {sector::checkpoint(&c.out,r)}
fn read(tool:&mut SF32LB52Tool,t:&port::Trace,c:&Config,name:&str,address:u32,bytes:usize)->Result<Vec<u8>> {
    require((address==0x12000000 && bytes==FTAB_BYTES) || ([BOOT,B].contains(&address) && bytes==K64),"Fixed read range only")?;
    t.event(json!({"kind":"execution_read_intent","name":name,"address":address,"bytes":bytes}))?;
    let path=c.out.join(name);require(!path.exists(),"No reused readback")?;
    tool.read_flash(&ReadFlashParams {files:vec![ReadFlashFile {file_path:path.to_string_lossy().into_owned(),address,size:bytes as u32}]})?;
    let data=fs::read(path)?;require(data.len()==bytes,"Incomplete readback")?;
    t.event(json!({"kind":"execution_read_complete","name":name,"sha256":hash(&data)}))?;Ok(data)
}
fn write(tool:&mut SF32LB52Tool,t:&port::Trace,c:&Config,r:&mut Value,role:&str,address:u32,data:&[u8])->Result<()> {
    require((address==B && data.len()==4096) || (address==BOOT && ((role=="boot_install" && data.len()==58496) || (role=="boot_restore" && data.len()==K64))),"Fixed write shape only")?;
    tool.base.cancel_token.check_cancelled()?;
    r[format!("{role}_attempted")]=json!(true);
    r[if address==BOOT {"boot_may_be_changed"}else{"b_may_be_changed"}]=json!(true);
    checkpoint(c,r)?;
    t.event(json!({"kind":"execution_write_intent","role":role,"address":address,"bytes":data.len(),"sha256":hash(data)}))?;
    require(tool.command(Command::WriteAndErase {address,len:data.len() as u32})?==Response::RxWait,"Unknown write-start; no further protocol")?;
    require(tool.send_data(data)?==Response::Ok,"Unknown data state; no further protocol")?;
    t.event(json!({"kind":"execution_write_complete","role":role}))?;
    r[format!("{role}_completed")]=json!(true);checkpoint(c,r)
}
fn ready(tool:&mut SF32LB52Tool,t:&port::Trace,c:&Config,r:&mut Value,role:&str)->Result<()> {
    tool.base.cancel_token.check_cancelled()?;require(!c.cancel.exists(),"Cancelled before entry")?;
    t.event(json!({"kind":"execution_entry_intent","role":role}))?;
    // Both entries are planned protocol operations. A logging fault never grants
    // the special cleanup-RTS exemption used by the separate observer.
    let high=tool.port.write_request_to_send(true);if high.is_ok() {thread::sleep(Duration::from_millis(100));}
    let low=tool.port.write_request_to_send(false);thread::sleep(Duration::from_millis(100));high?;low?;
    tool.download_stub()?;
    let snapshot=common::snapshot_strict(tool)?;
    tool.debug_command(SifliUartCommand::Enter)?;
    let aliases=vec![tool.debug_read_word32(0x5004206c)?,tool.debug_read_word32(0x50042070)?,tool.debug_read_word32(0x50042074)?];
    tool.debug_command(SifliUartCommand::Exit)?;
    r[format!("{role}_ready")]=json!({"registers":snapshot,"aliases":aliases});checkpoint(c,r)?;
    sector::ready_guard(&snapshot)?;require(aliases==[0,0,0],"MPI2 alias not zero after entry; no Flash read or write")?;
    t.event(json!({"kind":"execution_ready","role":role}))?;Ok(())
}
fn crc(data:&[u8])->u32 {const A:crc::Algorithm<u32>=crc::Algorithm {width:32,poly:0x04c11db7,init:0xffffffff,refin:true,refout:true,xorout:0xffffffff,check:0,residue:0};crc::Crc::<u32>::new(&A).checksum(data)}
fn frames(raw:&[u8])->Result<Value> {
    require(raw.len()<=1048576,"Probe capture exceeds1MiB")?;
    let magic=0x504f4657u32.to_le_bytes();let mut offset=0;let mut parsed=vec![];
    while let Some(pos)=raw[offset..].windows(4).position(|v|v==magic) {
        let start=offset+pos;require(start+80<=raw.len(),"Truncated probe frame")?;
        let data=&raw[start..start+80];let w:Vec<u32>=data.chunks_exact(4).map(|v|u32::from_le_bytes(v.try_into().unwrap())).collect();
        require(w[19]==crc(&data[..76]) && w[1]>>16==1 && w[2..6]==TAG,"Probe CRC/version/tag mismatch")?;
        let kind=parsed.len() as u32+1;
        require(kind<=3 && w[1]&0xffff==kind && w[6]==kind && w[7]==1 && w[18]==1684,"Probe stages/status/size mismatch")?;
        require(w[11]&0x30000==0 && w[12]==1,"Probe cache/IRQ mismatch")?;
        require(w[13..16]==if kind==1 {[0,0,0]}else{[0x20000,0x420000,0x400000]},"Probe alias mismatch")?;
        require(w[17]==0x12020674 && (kind==1 || w[16]==0x42584950),"Probe marker mismatch")?;
        if kind==3 {require((0x12020400..0x12020694).contains(&w[8]) && w[9]>0x20001000 && w[9]<=0x20002000 && w[9]&7==0 && w[10]==0x12020000,"Probe execution context mismatch")?;}
        else {require((0x20020000..0x20030000).contains(&w[8]) && w[9]>0x20040000 && w[9]<=0x20050000 && (0x20020000..0x20030000).contains(&w[10]),"RAM boot context mismatch")?;}
        parsed.push(w);offset=start+80;
    }
    require(parsed.len()==3,"Missing probe stages")?;Ok(json!(parsed))
}
fn observe(tool:&mut SF32LB52Tool,t:&port::Trace,c:&Config,r:&mut Value,start:u64)->Result<bool> {
    let end=Instant::now()+Duration::from_secs(c.listen_seconds);tool.port.set_timeout(Duration::from_millis(50))?;
    let mut block=[0;4096];let mut result=None;let mut complete_at=None;
    while Instant::now()<end {
        tool.base.cancel_token.check_cancelled()?;
        match tool.port.read(&mut block) {
            Ok(_)=>{},Err(e) if e.kind()==io::ErrorKind::TimedOut=>{},Err(e)=>return Err(e.into())
        }
        t.event(json!({"kind":"probe_receive_progress"}))?;
        let raw=fs::read(c.out.join("rx.bin"))?;let raw=&raw[start as usize..];
        require(raw.len()<=1048576,"Probe capture limit")?;
        if let Ok(parsed)=frames(raw) {
            result=Some(parsed);
            if complete_at.is_none() {complete_at=Some(Instant::now());}
            if complete_at.is_some_and(|at:Instant|at.elapsed()>=Duration::from_millis(250)) {break;}
        } else {result=None;complete_at=None;}
        // A real serial read blocks up to50ms. Bound synthetic empty peers too.
        thread::sleep(Duration::from_millis(5));
    }
    t.event(json!({"kind":"probe_receive_complete"}))?;
    let raw=fs::read(c.out.join("rx.bin"))?;save(&c.out.join("probe-uart.bin"),&raw[start as usize..])?;
    if let Some(parsed)=result {r["probe_frames"]=parsed;r["probe_verified"]=json!(true);}
    else {r["errors"].as_array_mut().unwrap().push(json!(frames(&raw[start as usize..]).unwrap_err().to_string()));}
    checkpoint(c,r)?;Ok(r["probe_verified"]==true)
}
fn restore(tool:&mut SF32LB52Tool,t:&port::Trace,c:&Config,r:&mut Value,original_boot:&[u8],original_b:&[u8])->Result<bool> {
    if r["boot_may_be_changed"]==true {
        write(tool,t,c,r,"boot_restore",BOOT,original_boot)?;
        let readback=read(tool,t,c,"restored-boot64k.bin",BOOT,K64)?;
        r["boot_restored"]=json!(readback==original_boot);checkpoint(c,r)?;
        require(r["boot_restored"]==true,"Boot restore mismatch; stop before B or application start")?;
        r["boot_may_be_changed"]=json!(false);checkpoint(c,r)?;
    } else {r["boot_restored"]=json!(true);}
    if r["b_may_be_changed"]==true {
        write(tool,t,c,r,"b_restore",B,&original_b[..4096])?;
        let readback=read(tool,t,c,"restored-b64k.bin",B,K64)?;
        r["b_restored"]=json!(readback==original_b);checkpoint(c,r)?;
        if r["b_restored"]==true {r["b_may_be_changed"]=json!(false);}
    } else {r["b_restored"]=json!(true);}
    r["restored_verified"]=json!(r["boot_restored"]==true && r["b_restored"]==true);
    checkpoint(c,r)?;Ok(r["restored_verified"]==true)
}
fn execute(tool:&mut SF32LB52Tool,t:&port::Trace,c:&Config,r:&mut Value,baseline:&[u8],candidate:&[u8],boot:&[u8])->Result<()> {
    ready(tool,t,c,r,"initial")?;
    let ftab=read(tool,t,c,"before-ftab12k.bin",0x12000000,FTAB_BYTES)?;
    require(ftab==baseline[..FTAB_BYTES],"Fresh FTAB differs; boot length/routing not admitted")?;
    let before_boot=read(tool,t,c,"before-boot64k.bin",BOOT,K64)?;
    let before_b=read(tool,t,c,"before-b64k.bin",B,K64)?;
    if c.recovery_only {
        let original_boot=input(c.restore_boot.as_ref().unwrap())?;
        let original_b=&baseline[0x420000..0x430000];
        require(before_b[4096..]==original_b[4096..],"B guard changed; fixed recovery cannot repair outside4KiB")?;
        r["boot_may_be_changed"]=json!(true);r["b_may_be_changed"]=json!(true);checkpoint(c,r)?;
        restore(tool,t,c,r,&original_boot,original_b)?;return Ok(());
    }
    require(before_boot==baseline[0x10000..0x20000] && before_b==baseline[0x420000..0x430000],"Fresh boot/B differs; preserve unknown changes")?;
    require(before_boot[58496..].iter().all(|v|*v==255),"Original boot compatibility tail differs")?;
    save(&c.out.join("boot-restore64k.bin"),&before_boot)?;save(&c.out.join("b-restore4k.bin"),&before_b[..4096])?;
    r["fresh_prestate_verified"]=json!(true);checkpoint(c,r)?;
    write(tool,t,c,r,"b_install",B,candidate)?;
    let b=read(tool,t,c,"installed-b64k.bin",B,K64)?;
    r["b_candidate_verified"]=json!(b[..4096]==*candidate && b[4096..]==before_b[4096..]);checkpoint(c,r)?;
    if r["b_candidate_verified"]!=true {r["errors"].as_array_mut().unwrap().push(json!("B comparison failed; restore without installing boot"));restore(tool,t,c,r,&before_boot,&before_b)?;return Ok(());}
    write(tool,t,c,r,"boot_install",BOOT,boot)?;
    let installed=read(tool,t,c,"installed-boot64k.bin",BOOT,K64)?;
    r["boot_candidate_verified"]=json!(installed[..58496]==*boot && installed[58496..]==before_boot[58496..]);checkpoint(c,r)?;
    if r["boot_candidate_verified"]!=true {r["errors"].as_array_mut().unwrap().push(json!("Boot comparison failed; restore without starting probe"));restore(tool,t,c,r,&before_boot,&before_b)?;return Ok(());}
    tool.base.cancel_token.check_cancelled()?;
    r["probe_start_attempted"]=json!(true);checkpoint(c,r)?;t.event(json!({"kind":"probe_start_intent"}))?;
    let start=fs::metadata(c.out.join("rx.bin"))?.len();
    require(tool.command(Command::SoftReset)?==Response::Ok,"Probe start response not OK; stop without second entry")?;
    r["probe_start_completed"]=json!(true);checkpoint(c,r)?;
    observe(tool,t,c,r,start)?;
    // Missing/bad frames end the finite observation. The separately authorized
    // planned ROM entry is still allowed; cancellation or I/O errors return above.
    ready(tool,t,c,r,"restore")?;
    let now_boot=read(tool,t,c,"reentry-boot64k.bin",BOOT,K64)?;
    let now_b=read(tool,t,c,"reentry-b64k.bin",B,K64)?;
    require(now_boot==installed && now_b==b,"Bytes changed during execution; no automatic restore over unknown changes")?;
    restore(tool,t,c,r,&before_boot,&before_b)?;Ok(())
}
fn session(c:&Config,stub:Vec<u8>,baseline:Vec<u8>,candidate:Vec<u8>,boot:Vec<u8>,sha:&str)->Result<Value> {
    fs::create_dir(&c.out)?;let frozen=c.out.join("official-stub.bin");save(&frozen,&stub)?;
    let trace=port::Trace::new(&c.out)?;let token=CancelToken::default();let start=Instant::now();
    let active=std::sync::Arc::new(std::sync::atomic::AtomicBool::new(true));let running=active.clone();let watched=token.clone();let cancel=c.cancel.clone();let seconds=c.seconds;
    let watcher=thread::spawn(move||while running.load(std::sync::atomic::Ordering::Relaxed) {if cancel.exists() || start.elapsed()>=Duration::from_secs(seconds) {watched.cancel();break;}thread::sleep(Duration::from_millis(20));});
    let mut r=json!({"schema":1,"kind":"ota-b-execute","config_sha":sha,"simulation":c.simulation,"recovery_only":c.recovery_only,"source_rev":REV,"stub_sha256":STUB_SHA,
        "same_port_object":true,"baud":1000000,"rtc_time_write":false,"entry_attempted":false,"boot_may_be_changed":c.recovery_only,"b_may_be_changed":c.recovery_only,
        "b_install_attempted":false,"b_install_completed":false,"boot_install_attempted":false,"boot_install_completed":false,
        "boot_restore_attempted":false,"boot_restore_completed":false,"b_restore_attempted":false,"b_restore_completed":false,
        "fresh_prestate_verified":false,"b_candidate_verified":false,"boot_candidate_verified":false,"probe_start_attempted":false,"probe_start_completed":false,
        "probe_verified":false,"boot_restored":false,"b_restored":false,"restored_verified":false,"original_start_completed":false,"fallback_attempted":false,"errors":[]});
    let work=(|| ->Result<()> {
        require(!c.cancel.exists(),"Cancelled before open")?;token.check_cancelled()?;
        let opened:Box<dyn serialport::SerialPort>=if c.simulation {
            let mut nor=baseline.clone();
            if c.recovery_only {nor[0x10000..0x10000+boot.len()].copy_from_slice(&boot);nor[0x420000..0x421000].copy_from_slice(&candidate);}
            Box::new(simulated::Peer::execution(nor,boot.clone(),synthetic_frames(),&c.scenario))}
            else {serialport::new(&c.port,1_000_000).dtr_on_open(false).timeout(Duration::from_secs(5)).open()?};
        let base=SifliToolBase::new_with_external_stub_and_cancel(c.port.clone(),BeforeOperation::NoReset,"nor".into(),1_000_000,1,false,sftool_lib::progress::no_op_progress_sink(),Some(frozen.to_string_lossy().into_owned()),token);
        let mut tool=SF32LB52Tool {base,port:Box::new(port::Recorded::new(opened,trace.clone()))};
        let result=std::panic::catch_unwind(std::panic::AssertUnwindSafe(||execute(&mut tool,&trace,c,&mut r,&baseline,&candidate,&boot)))
            .unwrap_or_else(|_|Err(io::Error::other("Pinned library panicked; no further protocol").into()));
        let clean=result.is_ok();if let Err(e)=result {r["errors"].as_array_mut().unwrap().push(json!(e.to_string()));}
        r["entry_attempted"]=match trace.high_attempts() {Some(n)=>json!(n>0),None=>Value::Null};
        if can_start_original(clean,&r,&trace) {
            tool.base.cancel_token.check_cancelled()?;trace.event(json!({"kind":"original_start_intent"}))?;
            require(tool.command(Command::SoftReset)?==Response::Ok,"Original start response not OK; no retry")?;
            r["original_start_completed"]=json!(true);
        }
        // No fallback reset: after boot mutation that could boot the probe again.
        // Every error is fail-stop. Drop releases the UART; another recovery
        // invocation always needs its own explicit authorization and lease.
        drop(tool);Ok(())
    })();
    if let Err(e)=work {r["errors"].as_array_mut().unwrap().push(json!(e.to_string()));}
    active.store(false,std::sync::atomic::Ordering::Relaxed);let _=watcher.join();
    match trace.finish() {Ok(v)=>r["trace"]=v,Err(e)=>r["errors"].as_array_mut().unwrap().push(json!(e.to_string()))}
    r["entry_attempted"]=match trace.high_attempts() {Some(n)=>json!(n>0),None=>Value::Null};
    r["port_drop_completed"]=json!(true);r["elapsed_seconds"]=json!(start.elapsed().as_secs_f64());
    r["passed"]=json!(r["errors"].as_array().unwrap().is_empty() && r["restored_verified"]==true && r["original_start_completed"]==true && (c.recovery_only || r["probe_verified"]==true));
    save(&c.out.join("execution.json"),&serde_json::to_vec_pretty(&r)?)?;Ok(r)
}
fn synthetic_frames()->Vec<u8> {
    let mut raw=vec![];
    for kind in 1..=3 {let alias=if kind==1 {[0,0,0]}else{[0x20000,0x420000,0x400000]};
        let mut w=vec![0x504f4657,0x10000|kind];w.extend(TAG);w.extend([kind,1,if kind==3 {0x120204c4}else{0x20021000},if kind==3 {0x20002000}else{0x2004fff0},if kind==3 {0x12020000}else{0x20020000},0,1]);
        w.extend(alias);w.extend([0x42584950,0x12020674,1684]);let mut frame:Vec<u8>=w.into_iter().flat_map(u32::to_le_bytes).collect();frame.extend(crc(&frame).to_le_bytes());raw.extend(frame);}
    raw
}
fn main() {
    let args:Vec<_>=std::env::args().skip(1).collect();let result=(|| ->Result<Value> {
        require(args.len()==3 && args[0]=="--config","Only explicit hashed config")?;let raw=fs::read(&args[1])?;
        require(raw.len()<=65536 && hash(&raw)==args[2],"Config changed")?;let c:Config=serde_json::from_slice(&raw)?;
        let (s,b,p,boot)=validate(&c)?;session(&c,s,b,p,boot,&args[2])})();
    match result {Ok(v)=>{println!("{v}");if v["passed"]!=true {std::process::exit(1)}},Err(e)=>{eprintln!("{e}");std::process::exit(2)}}
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn fixed_frames_validate_and_refuse_crc_tag_truncation_or_duplicates() {
        let raw=synthetic_frames();frames(&raw).unwrap();
        let mut bad=raw.clone();bad[12]^=1;assert!(frames(&bad).is_err());
        assert!(frames(&raw[..239]).is_err());let mut duplicate=raw.clone();duplicate.extend(&raw[..80]);assert!(frames(&duplicate).is_err());
    }
}
