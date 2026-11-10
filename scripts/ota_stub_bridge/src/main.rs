//! Read-only observer using the unmodified, pinned official SF52 library/stub.
//! There is no user-selected address, flash write/erase, baud change or reconnect.
mod port;
mod simulated;
mod geometry;

use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use sftool_lib::{BeforeOperation, CancelToken, ReadFlashFile, ReadFlashParams, SifliToolBase, SifliToolTrait};
use sftool_lib::common::ram_command::DownloadStub;
use sftool_lib::common::sifli_debug::{SifliDebug, SifliUartCommand};
use sftool_lib::read_flash::ReadFlashTrait;
use sftool_lib::sf32lb52::SF32LB52Tool;
use sha2::{Digest, Sha256};
use std::{fs, io, path::PathBuf, thread, time::{Duration, Instant}};

const STUB_SHA: &str = "d4db9c27a0539da431fd253ee4b09c8d3ddb033bf0dc1ae4cc3bf322fa069e43";
const SOURCE_REV: &str = "be2adab37bfb0ef4eb623b7e47e7b8693918b1e1";
const NOR_BYTES: usize = 0x1000000;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Input { path: PathBuf, bytes: usize, sha256: String }

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Config {
    schema: u32, simulation: bool, port: String, out: PathBuf, plan:String,
    stub: Input, baseline: Input, cancel: PathBuf, seconds: u64,
    entry_reset_authorized: bool, ram_stub_authorized: bool,
    register_observation_authorized: bool, original_start_authorized: bool,
    exit_reset_authorized: bool, scenario: String,
}

fn require(ok: bool, reason: &str) -> Result<()> {
    if ok { Ok(()) } else { Err(io::Error::other(reason).into()) }
}
fn hash(bytes: &[u8]) -> String { format!("{:x}", Sha256::digest(bytes)) }
fn input(value: &Input) -> Result<Vec<u8>> {
    let data = fs::read(&value.path)?;
    require(data.len()==value.bytes && hash(&data)==value.sha256,"Input identity changed")?;
    Ok(data)
}
fn validate(config: &Config) -> Result<(Vec<u8>, Vec<u8>)> {
    require(config.schema==1 && (1..=600).contains(&config.seconds),"Invalid bounded config")?;
    require(["dual-nor","geometry"].contains(&config.plan.as_str()),"Unknown fixed observation plan")?;
    require(config.entry_reset_authorized && config.ram_stub_authorized &&
        config.register_observation_authorized && config.original_start_authorized,
        "Explicit reset, official RAM stub, register observation and original-start scope required")?;
    require(["success","fragmented","short-read","geometry-bad-pointer","lcpu-running","register-read-fail","immutable-changed","pair-changed","crc-fail","hang-read"].contains(&config.scenario.as_str()),"Unknown scenario")?;
    require(config.scenario=="success" || config.simulation,"Fault injection is simulation-only")?;
    if config.simulation {
        require(config.port=="SIMULATED-STUB","Simulation cannot name a device")?;
    } else {
        require(cfg!(windows),"Device observer supports Windows only")?;
        let suffix=config.port.strip_prefix("COM").unwrap_or("");
        require(!suffix.is_empty() && !suffix.starts_with('0') && suffix.bytes().all(|b|b.is_ascii_digit()),
            "Canonical COM endpoint required")?;
    }
    require(!config.out.exists(),"Never replace an observation directory")?;
    require(config.stub.bytes==132816 && config.stub.sha256==STUB_SHA,"Only the pinned original SF52 NOR stub is allowed")?;
    let stub=input(&config.stub)?;
    let baseline=input(&config.baseline)?;
    require(baseline.len()==NOR_BYTES,"A bound complete original NOR baseline is required")?;
    Ok((stub,baseline))
}

#[derive(Serialize)]
struct Channel { address: u32, enabled: bool, ccr: u32, count: u32, source: u32, destination: u32 }

// Addresses and offsets are from the locked SDK SF32LB52 register definitions.
// A snapshot reports hardware values; it does not classify an enabled DMA as owned.
fn snapshot(tool: &mut SF32LB52Tool) -> Result<Value> {
    tool.debug_command(SifliUartCommand::Enter)?;
    let result=(|| -> Result<Value> {
        let pmr=tool.debug_read_word32(0x40040000)?;
        let rstr=tool.debug_read_word32(0x40000000)?;
        let mut channels=Vec::new();
        for base in [0x50081008u32,0x40001008u32] {
            for index in 0..8 {
                let address=base+index*0x14;
                let ccr=tool.debug_read_word32(address)?;
                // Record endpoints even when disabled; never turn a failed read into zero.
                let count=tool.debug_read_word32(address+4)?;
                let peripheral=tool.debug_read_word32(address+8)?;
                let memory=tool.debug_read_word32(address+12)?;
                let (source,destination)=if ccr&0x10!=0 {(memory,peripheral)}else{(peripheral,memory)};
                channels.push(Channel {address,enabled:ccr&1!=0,ccr,count,source,destination});
            }
        }
        let address=0x50001008;
        let ccr=tool.debug_read_word32(address)?;
        channels.push(Channel {address,enabled:ccr&1!=0,ccr,
            count:tool.debug_read_word32(address+4)?,
            source:tool.debug_read_word32(address+8)?,
            destination:tool.debug_read_word32(address+12)?});
        Ok(json!({"pmr":pmr,"cpuwait":pmr&4!=0,"rstr1":rstr,"channels":channels,
            "all_flash_users_controlled":false,"continuous_quiescence_proven":false}))
    })();
    // Exit is a single command, not an ACK-based proof that the application runs.
    let exit=tool.debug_command(SifliUartCommand::Exit);
    match result { Ok(value)=>{exit?;Ok(value)}, Err(error)=>Err(error) }
}

fn check_immutable(data: &[u8], baseline: &[u8]) -> Result<()> {
    require(data.len()==NOR_BYTES,"Short NOR read")?;
    // Current Product may change FS/KVDB; only immutable experiment inputs are bound.
    for (start,end) in [(0,0x10000),(0x10000,0x20000),(0x20000,0x420000),(0x420000,0x421000)] {
        require(data[start..end]==baseline[start..end],"Immutable FTAB/boot/A/B-prefix changed; stop before next read")?;
    }
    Ok(())
}

fn pulse(tool: &mut SF32LB52Tool, trace: &port::Trace, role: &str) -> Result<()> {
    let intent=trace.event(json!({"kind":"rts_pulse_intent","role":role}));
    if role=="entry" {intent?;} else {trace.allow_cleanup_rts();}
    let high=tool.port.write_request_to_send(true);
    if high.is_ok() {thread::sleep(Duration::from_millis(100));}
    // Releasing the line must still happen after cancellation or a failed log.
    let low=tool.port.write_request_to_send(false);
    thread::sleep(Duration::from_millis(100));
    let _=trace.event(json!({"kind":"rts_pulse_complete","role":role,"high_ok":high.is_ok(),"low_ok":low.is_ok()}));
    high?;low?;
    Ok(())
}

fn session(config: &Config, stub:Vec<u8>, baseline: Vec<u8>, request_sha:&str) -> Result<Value> {
    fs::create_dir(&config.out)?;
    let frozen_stub=config.out.join("official-stub.bin");
    fs::write(&frozen_stub,stub)?;
    let trace=port::Trace::new(&config.out)?;
    let token=CancelToken::default();
    let cancel_token=token.clone(); let cancel_path=config.cancel.clone();
    let started=Instant::now(); let seconds=config.seconds;
    let monitor=std::sync::Arc::new(std::sync::atomic::AtomicBool::new(true));
    let monitor_thread=monitor.clone();
    let watch=thread::spawn(move || {
        while monitor_thread.load(std::sync::atomic::Ordering::Relaxed) {
            if cancel_path.exists() || started.elapsed()>=Duration::from_secs(seconds) { cancel_token.cancel(); break; }
            thread::sleep(Duration::from_millis(20));
        }
    });
    let opened=(|| -> Result<Box<dyn serialport::SerialPort>> {
        require(!config.cancel.exists(),"Cancelled before port open")?;
        require(started.elapsed()<Duration::from_secs(seconds),"Budget expired before port open")?;
        token.check_cancelled()?;
        if config.simulation { Ok(Box::new(simulated::Peer::new(baseline.clone(),&config.scenario))) }
        else { Ok(serialport::new(&config.port,1_000_000).dtr_on_open(false).timeout(Duration::from_secs(5)).open()?) }
    })();
    let opened=match opened { Ok(value)=>value,Err(error)=>{
        monitor.store(false,std::sync::atomic::Ordering::Relaxed);let _=watch.join();return Err(error);
    }};
    let base=SifliToolBase::new_with_external_stub_and_cancel(config.port.clone(),BeforeOperation::NoReset,
        "nor".into(),1_000_000,1,false,sftool_lib::progress::no_op_progress_sink(),
        Some(frozen_stub.to_string_lossy().into_owned()),token);
    let mut tool=SF32LB52Tool {base,port:Box::new(port::Recorded::new(opened,trace.clone()))};
    let mut report=json!({"schema":1,"simulation":config.simulation,"plan":config.plan,"source_rev":SOURCE_REV,"config_sha":request_sha,
        "flash_write":false,"erase":false,"rtc_time_write":false,"stub_sha256":STUB_SHA,
        "same_port_object":true,"baud":1000000,"read_pair_passed":false,
        "snapshots_complete":false,"entry_attempted":false,"normal_start_attempted":false,
        "fallback_attempted":false,"fallback_completed":false,"errors":[]});
    let work=std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| -> Result<()> {
        tool.base.cancel_token.check_cancelled()?;
        require(!config.cancel.exists(),"Cancelled before control-line normalization")?;
        require(started.elapsed()<Duration::from_secs(seconds),"Budget expired before control-line normalization")?;
        tool.port.write_request_to_send(false)?;
        require(!config.cancel.exists(),"Cancelled before entry")?;
        require(started.elapsed()<Duration::from_secs(seconds),"Budget expired before entry")?;
        tool.base.cancel_token.check_cancelled()?;
        report["entry_attempted"]=json!(true);
        pulse(&mut tool,&trace,"entry")?;
        // NoReset means the official connection loop cannot add another RTS pulse.
        // Its at-most-two Enter requests and one SYSRESETREQ remain original source behavior.
        tool.download_stub()?;
        trace.event(json!({"kind":"stub_ready"}))?;
        let ready=snapshot(&mut tool)?;
        fs::write(config.out.join("ready-registers.json"),serde_json::to_vec_pretty(&ready)?)?;
        report["ready_registers"]=ready;
        if config.plan=="geometry" {
            report["geometry"]=geometry::observe(&mut tool,&trace,&config.out)?;
            report["geometry_snapshot_complete"]=json!(true);
        } else {
        for name in ["nor-first.bin","nor-second.bin"] {
            require(!config.cancel.exists(),"Cancelled before NOR request")?;
            let path=config.out.join(name);
            require(!path.exists(),"Never reuse an old read output")?;
            trace.event(json!({"kind":"nor_read_intent","name":name,"address":0x12000000u32,"bytes":NOR_BYTES}))?;
            tool.read_flash(&ReadFlashParams {files:vec![ReadFlashFile {file_path:path.to_string_lossy().into_owned(),
                address:0x12000000,size:NOR_BYTES as u32}]})?;
            let data=fs::read(&path)?;
            check_immutable(&data,&baseline)?;
            trace.event(json!({"kind":"nor_read_verified","name":name,"sha256":hash(&data)}))?;
            if name=="nor-second.bin" {
                require(fs::read(config.out.join("nor-first.bin"))?==data,"NOR pair changed inside the owned stub session")?;
                report["read_pair_passed"]=json!(true);
                report["nor_sha256"]=json!(hash(&data));
            }
        }
        let after=snapshot(&mut tool)?;
        fs::write(config.out.join("after-registers.json"),serde_json::to_vec_pretty(&after)?)?;
        report["after_registers"]=after;
        report["snapshots_complete"]=json!(true);
        }
        require(!config.cancel.exists(),"Cancelled before original-start command")?;
        require(started.elapsed()<Duration::from_secs(seconds),"Budget expired before original-start command")?;
        tool.base.cancel_token.check_cancelled()?;
        report["normal_start_attempted"]=json!(true);
        trace.event(json!({"kind":"original_start_intent","command":"burn_reset"}))?;
        tool.soft_reset()?;
        report["normal_start_command_completed"]=json!(true);
        Ok(())
    })).unwrap_or_else(|_|Err(io::Error::other("Official-library operation panicked; session is not reusable").into()));
    if let Err(error)=work {
        report["entry_attempted"]=match trace.high_attempts() {Some(count)=>json!(count>0),None=>Value::Null};
        report["errors"].as_array_mut().unwrap().push(json!(error.to_string()));
        if report["entry_attempted"]!=false && config.exit_reset_authorized {
            report["fallback_attempted"]=json!(true);
            match pulse(&mut tool,&trace,"fallback") {
                Ok(())=>report["fallback_completed"]=json!(true),
                Err(error)=>report["errors"].as_array_mut().unwrap().push(json!(format!("Fallback RTS: {error}"))),
            }
        }
    }
    drop(tool);
    monitor.store(false,std::sync::atomic::Ordering::Relaxed);let _=watch.join();
    let _=trace.event(json!({"kind":"port_drop_completed"}));
    match trace.finish() {
        Ok(value)=>report["trace"]=value,
        Err(error)=>report["errors"].as_array_mut().unwrap().push(json!(format!("Trace flush failed: {error}"))),
    }
    if let Some(error)=trace.fault() {report["errors"].as_array_mut().unwrap().push(json!(format!("Trace incomplete: {error}")))}
    report["port_drop_completed"]=json!(true);
    report["elapsed_seconds"]=json!(started.elapsed().as_secs_f64());
    report["passed"]=json!(report["errors"].as_array().unwrap().is_empty());
    report["physical_erase_envelope_verified"]=json!(false);
    report["all_flash_users_controlled"]=json!(false);
    fs::write(config.out.join("observer.json"),serde_json::to_vec_pretty(&report)?)?;
    Ok(report)
}

fn main() {
    let args:Vec<_>=std::env::args().skip(1).collect();
    let result=(|| -> Result<Value> {
        require(args.len()==3 && args[0]=="--config","Usage: wristflow-ota-stub-observer --config <JSON> <SHA256>; no default device mode")?;
        let bytes=fs::read(&args[1])?;
        require(bytes.len()<=65536 && hash(&bytes)==args[2],"Bound config identity changed")?;
        let config:Config=serde_json::from_slice(&bytes)?;
        let (stub,baseline)=validate(&config)?;
        session(&config,stub,baseline,&args[2])
    })();
    match result {
        Ok(report)=>{println!("{report}");if report["passed"]!=true {std::process::exit(1)}},
        Err(error)=>{eprintln!("{error}");std::process::exit(2)},
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn immutable_gate_allows_only_mutable_changes() {
        let before=vec![0xff;NOR_BYTES];let mut after=before.clone();
        after[0x129a0000usize-0x12000000]=1;check_immutable(&after,&before).unwrap();
        for address in [0usize,0x10000,0x20000,0x420000] {
            let mut data=before.clone();data[address]=1;assert!(check_immutable(&data,&before).is_err());
        }
        assert!(check_immutable(&before[..4096],&before).is_err());
    }
}
