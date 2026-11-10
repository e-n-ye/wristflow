//! Separate, explicit fixed-sector writer; the observer binary stays read-only.
mod port;
mod simulated;
mod common;
mod sector;
use serde::Deserialize;
use serde_json::{Value,json};
use sha2::{Digest,Sha256};
use sftool_lib::{BeforeOperation,CancelToken,SifliToolBase};
use sftool_lib::common::ram_command::DownloadStub;
use sftool_lib::sf32lb52::SF32LB52Tool;
use std::{fs,io,path::PathBuf,thread,time::{Duration,Instant}};
type Result<T>=std::result::Result<T,Box<dyn std::error::Error>>;
const STUB_SHA:&str="d4db9c27a0539da431fd253ee4b09c8d3ddb033bf0dc1ae4cc3bf322fa069e43";
const REV:&str="be2adab37bfb0ef4eb623b7e47e7b8693918b1e1";
fn require(ok:bool,message:&str)->Result<()> {if ok {Ok(())}else{Err(io::Error::other(message).into())}}
fn hash(data:&[u8])->String {format!("{:x}",Sha256::digest(data))}
fn can_start_original(protocol_clean:bool,report:&Value,trace:&port::Trace)->bool {
    protocol_clean && report["restored_verified"]==true && trace.fault().is_none()
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Input {path:PathBuf,bytes:usize,sha256:String}
fn input(v:&Input)->Result<Vec<u8>> {let b=fs::read(&v.path)?;require(b.len()==v.bytes && hash(&b)==v.sha256,"Bound input changed")?;Ok(b)}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Config {
    schema:u32,simulation:bool,port:String,out:PathBuf,stub:Input,baseline:Input,candidate:Input,
    cancel:PathBuf,seconds:u64,scenario:String,
    entry_reset_authorized:bool,ram_stub_authorized:bool,register_observation_authorized:bool,
    b_sector_write_authorized:bool,b_sector_restore_authorized:bool,original_start_authorized:bool,exit_reset_authorized:bool,
}
fn validate(c:&Config)->Result<(Vec<u8>,Vec<u8>,Vec<u8>)> {
    require(c.schema==1 && (15..=180).contains(&c.seconds),"Bounded B-sector config required")?;
    require(c.entry_reset_authorized && c.ram_stub_authorized && c.register_observation_authorized && c.b_sector_write_authorized &&
        c.b_sector_restore_authorized && c.original_start_authorized,"Explicit B write AND same-window restore authorization required")?;
    require(simulated::B_SCENARIOS.contains(&c.scenario.as_str()),"Unknown B-sector scenario")?;
    require(c.simulation || c.scenario=="success","Fault injection is simulation-only")?;
    if c.simulation {require(c.port=="SIMULATED-STUB","Simulation cannot name a device")?;}
    else {let n=c.port.strip_prefix("COM").unwrap_or("");require(cfg!(windows) && !n.is_empty() && !n.starts_with('0') && n.bytes().all(|b|b.is_ascii_digit()),"Canonical Windows COM required")?;}
    require(!c.out.exists(),"New output directory required")?;
    require(c.stub.bytes==132816 && c.stub.sha256==STUB_SHA,"Pinned original RAM stub only")?;
    require(c.candidate.bytes==sector::SECTOR && c.candidate.sha256==sector::CANDIDATE_SHA,"Only the fixed reviewed B candidate is allowed")?;
    require(c.baseline.bytes==0x1000000,"Complete bound NOR input size required")?;
    let stub=input(&c.stub)?;let baseline=input(&c.baseline)?;let candidate=input(&c.candidate)?;
    require(baseline.len()==0x1000000,"Complete bound NOR baseline required")?;Ok((stub,baseline,candidate))
}
fn session(c:&Config,stub:Vec<u8>,baseline:Vec<u8>,candidate:Vec<u8>,sha:&str)->Result<Value> {
    fs::create_dir(&c.out)?;let frozen=c.out.join("official-stub.bin");fs::write(&frozen,stub)?;
    let trace=port::Trace::new(&c.out)?;let token=CancelToken::default();
    let started=Instant::now();let seconds=c.seconds;let cancel=c.cancel.clone();let watched=token.clone();
    let running=std::sync::Arc::new(std::sync::atomic::AtomicBool::new(true));let active=running.clone();
    let watch=thread::spawn(move|| {while active.load(std::sync::atomic::Ordering::Relaxed) {
        if cancel.exists() || started.elapsed()>=Duration::from_secs(seconds) {watched.cancel();break;}
        thread::sleep(Duration::from_millis(20));
    }});
    let opened=(|| ->Result<Box<dyn serialport::SerialPort>> {
        require(!c.cancel.exists(),"Cancelled before open")?;token.check_cancelled()?;
        if c.simulation {Ok(Box::new(simulated::Peer::b_sector(baseline.clone(),&c.scenario)))}
        else {Ok(serialport::new(&c.port,1_000_000).dtr_on_open(false).timeout(Duration::from_secs(5)).open()?)}
    })();
    let mut report=json!({"schema":1,"kind":"ota-b-sector","simulation":c.simulation,"config_sha":sha,"source_rev":REV,"stub_sha256":STUB_SHA,
        "baud":1000000,"same_port_object":true,"entry_attempted":false,"candidate_write_attempted":false,"candidate_write_completed":false,
        "restore_write_attempted":false,"restore_write_completed":false,"candidate_verified":false,"candidate_guard_unchanged":false,
        "restored_verified":false,"restored_guard_unchanged":false,"b_may_be_changed":false,"normal_start_command_completed":false,
        "fallback_attempted":false,"rtc_time_write":false,"all_flash_users_controlled":false,"physical_erase_envelope_verified":false,"errors":[]});
    let work=(|| ->Result<()> {
        let opened=opened?;
        let base=SifliToolBase::new_with_external_stub_and_cancel(c.port.clone(),BeforeOperation::NoReset,"nor".into(),1_000_000,1,false,
            sftool_lib::progress::no_op_progress_sink(),Some(frozen.to_string_lossy().into_owned()),token);
        let mut tool=SF32LB52Tool {base,port:Box::new(port::Recorded::new(opened,trace.clone()))};
        let work=std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| ->Result<()> {
            tool.base.cancel_token.check_cancelled()?;require(!c.cancel.exists(),"Cancelled before entry")?;
            tool.port.write_request_to_send(false)?;report["entry_attempted"]=json!(true);sector::checkpoint(&c.out,&report)?;
            common::pulse(&mut tool,&trace,"entry")?;tool.download_stub()?;trace.event(json!({"kind":"stub_ready"}))?;
            let ready=common::snapshot(&mut tool)?;report["ready_registers"]=ready.clone();sector::checkpoint(&c.out,&report)?;
            sector::ready_guard(&ready)?;
            if !sector::execute(&mut tool,&trace,&c.out,&baseline,&candidate,&mut report)? {
                report["errors"].as_array_mut().unwrap().push(json!("B experiment comparison failed; completed recovery retained"));
            }
            Ok(())
        })).unwrap_or_else(|_|Err(io::Error::other("Pinned library panicked; no further protocol").into()));
        let protocol_clean=work.is_ok();
        if let Err(e)=work {
            report["entry_attempted"]=match trace.high_attempts() {Some(n)=>json!(n>0),None=>Value::Null};
            report["errors"].as_array_mut().unwrap().push(json!(e.to_string()));
        }
        // A comparison failure after a fully verified restore may still exit
        // normally. Unknown protocol/write state never sends burn_reset.
        if can_start_original(protocol_clean,&report,&trace) {
            use sftool_lib::SifliToolTrait;
            let reset=(|| ->Result<()> {tool.base.cancel_token.check_cancelled()?;
                trace.event(json!({"kind":"original_start_intent","command":"burn_reset"}))?;tool.soft_reset()?;Ok(())})();
            match reset {Ok(())=>report["normal_start_command_completed"]=json!(true),Err(e)=>report["errors"].as_array_mut().unwrap().push(json!(e.to_string()))}
        }
        if report["normal_start_command_completed"]!=true && report["entry_attempted"]==true && c.exit_reset_authorized {
            report["fallback_attempted"]=json!(true);
            match common::pulse(&mut tool,&trace,"fallback") {Ok(())=>report["fallback_completed"]=json!(true),Err(e)=>report["errors"].as_array_mut().unwrap().push(json!(e.to_string()))}
        }
        drop(tool);Ok(())
    })();
    if let Err(e)=work {report["errors"].as_array_mut().unwrap().push(json!(e.to_string()))}
    running.store(false,std::sync::atomic::Ordering::Relaxed);let _=watch.join();
    match trace.finish() {Ok(v)=>report["trace"]=v,Err(e)=>report["errors"].as_array_mut().unwrap().push(json!(e.to_string()))}
    report["port_drop_completed"]=json!(true);report["elapsed_seconds"]=json!(started.elapsed().as_secs_f64());
    report["passed"]=json!(report["errors"].as_array().unwrap().is_empty() && report["restored_verified"]==true && report["normal_start_command_completed"]==true);
    fs::write(c.out.join("sector.json"),serde_json::to_vec_pretty(&report)?)?;Ok(report)
}
fn main() {
    let args:Vec<_>=std::env::args().skip(1).collect();
    let result=(|| ->Result<Value> {require(args.len()==3 && args[0]=="--config","Explicit hashed config required; no default device action")?;
        let bytes=fs::read(&args[1])?;require(bytes.len()<=65536 && hash(&bytes)==args[2],"Config identity changed")?;
        let c:Config=serde_json::from_slice(&bytes)?;let (s,b,p)=validate(&c)?;session(&c,s,b,p,&args[2])})();
    match result {Ok(v)=>{println!("{v}");if v["passed"]!=true {std::process::exit(1)}},Err(e)=>{eprintln!("{e}");std::process::exit(2)}}
}
