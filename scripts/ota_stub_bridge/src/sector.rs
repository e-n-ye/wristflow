//! One fixed B sector transaction using the original library RAM commands.
use crate::{Result,require,hash};
use crate::port::Trace;
use serde_json::{Value,json};
use sftool_lib::{sf32lb52::SF32LB52Tool,ReadFlashFile,ReadFlashParams};
use sftool_lib::read_flash::ReadFlashTrait;
use sftool_lib::common::ram_command::{RamCommand,Command,Response};
use std::{fs::{self,File},io::Write,path::Path};

pub const ADDRESS:u32=0x12420000;
pub const SECTOR:usize=4096;
pub const WINDOW:usize=65536;
pub const CANDIDATE_SHA:&str="14102c47df5a8078fd934f9efaea7a271ef790eb7ffd265546abf4cf5b4440fe";

pub fn checkpoint(out:&Path,report:&Value)->Result<()> {
    // Independent append-only states survive a missing final report. Persist the
    // possible-mutation marker before any mutating command leaves this process.
    let index=fs::read_dir(out)?.filter_map(|v|v.ok()).filter(|v|v.file_name().to_string_lossy().starts_with("state-")).count();
    let mut f=File::create_new(out.join(format!("state-{index:03}.json")))?;
    f.write_all(&serde_json::to_vec_pretty(report)?)?;f.sync_all()?;Ok(())
}
fn read(tool:&mut SF32LB52Tool,trace:&Trace,out:&Path,name:&str)->Result<Vec<u8>> {
    trace.event(json!({"kind":"sector_read_intent","name":name,"address":ADDRESS,"bytes":WINDOW}))?;
    let path=out.join(name);require(!path.exists(),"Never reuse a readback")?;
    tool.read_flash(&ReadFlashParams {files:vec![ReadFlashFile {file_path:path.to_string_lossy().into_owned(),address:ADDRESS,size:WINDOW as u32}]})?;
    let data=fs::read(path)?;require(data.len()==WINDOW,"Incomplete fixed B window")?;
    trace.event(json!({"kind":"sector_read_complete","name":name,"sha256":hash(&data)}))?;
    Ok(data)
}
fn write(tool:&mut SF32LB52Tool,trace:&Trace,data:&[u8],role:&str)->Result<()> {
    require(data.len()==SECTOR,"Only one exact sector is writable")?;
    trace.event(json!({"kind":"sector_write_intent","role":role,"address":ADDRESS,"bytes":SECTOR,"sha256":hash(data)}))?;
    // Use the pinned library's command/response and raw data APIs unchanged.
    // No incremental CRC shortcut: a successful experiment must actually write.
    require(tool.command(Command::WriteAndErase {address:ADDRESS,len:SECTOR as u32})?==Response::RxWait,
        "Unknown write-start state; no further protocol")?;
    require(tool.send_data(data)?==Response::Ok,"Unknown write-completion state; no further protocol")?;
    trace.event(json!({"kind":"sector_write_complete","role":role}))?;Ok(())
}
pub fn ready_guard(value:&Value)->Result<()> {
    require(value["cpuwait"]==true,"LCPU CPUWAIT not observed; stop before first write")?;
    let channels=value["channels"].as_array().ok_or("Missing ready channels")?;
    for c in channels {
        if c["enabled"]==true {
            for key in ["source","destination"] {
                let address=c[key].as_u64().ok_or("Missing DMA endpoint")?;
                require(!(0x12000000..0x13000000).contains(&address),"Observed DMA NOR endpoint; stop before first write")?;
            }
            // This board's only accepted enabled sample is the known UART1 RX
            // channel. Unknown enabled hardware is not silently classified safe.
            require(c["address"]==0x50081080u32 && c["source"]==0x50084024u32 &&
                c["destination"].as_u64().is_some_and(|v|(0x20000000..0x20080000).contains(&v)),
                "Unexplained enabled DMA; stop before first write")?;
        }
    }
    Ok(())
}
pub fn execute(tool:&mut SF32LB52Tool,trace:&Trace,out:&Path,baseline:&[u8],candidate:&[u8],report:&mut Value)->Result<bool> {
    execute_with(tool,trace,out,baseline,candidate,report,checkpoint)
}
fn execute_with<F:FnMut(&Path,&Value)->Result<()>>(tool:&mut SF32LB52Tool,trace:&Trace,out:&Path,baseline:&[u8],candidate:&[u8],report:&mut Value,mut persist:F)->Result<bool> {
    require(baseline.len()==0x1000000 && candidate.len()==SECTOR,"Bound input sizes changed")?;
    let before=read(tool,trace,out,"before-b64k.bin")?;
    require(before==baseline[0x420000..0x430000],"Fresh B64KiB differs from bound baseline; no write")?;
    require(before[..SECTOR]!=*candidate,"Candidate equals prestate; no write experiment")?;
    let restore=&before[..SECTOR];
    let mut f=File::create_new(out.join("fresh-restore-b4k.bin"))?;f.write_all(restore)?;f.sync_all()?;
    report["fresh_prestate_verified"]=json!(true);report["before_sha256"]=json!(hash(&before));
    report["restore_sha256"]=json!(hash(restore));persist(out,report)?;
    tool.base.cancel_token.check_cancelled()?;
    report["candidate_write_attempted"]=json!(true);report["b_may_be_changed"]=json!(true);
    persist(out,report)?;
    write(tool,trace,candidate,"candidate")?;
    report["candidate_write_completed"]=json!(true);persist(out,report)?;
    let after=read(tool,trace,out,"candidate-b64k.bin")?;
    report["candidate_verified"]=json!(after[..SECTOR]==*candidate);
    report["candidate_guard_unchanged"]=json!(after[SECTOR..]==before[SECTOR..]);persist(out,report)?;
    // A complete CRC-checked read leaves the protocol synchronized even if its
    // bytes fail comparison. Restore once in that case, then keep the failure.
    // Any I/O, trace, cancellation or CRC failure above returns immediately.
    tool.base.cancel_token.check_cancelled()?;
    report["restore_write_attempted"]=json!(true);persist(out,report)?;
    write(tool,trace,restore,"restore")?;
    report["restore_write_completed"]=json!(true);persist(out,report)?;
    let restored=read(tool,trace,out,"restored-b64k.bin")?;
    report["restored_verified"]=json!(restored==before);
    report["restored_guard_unchanged"]=json!(restored[SECTOR..]==before[SECTOR..]);
    if report["restored_verified"]==true {report["b_may_be_changed"]=json!(false)}
    persist(out,report)?;
    Ok(report["candidate_verified"]==true && report["candidate_guard_unchanged"]==true && report["restored_verified"]==true)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn ready_guard_allows_known_uart_dma_and_refuses_flash_or_unknown_enabled_dma() {
        let value=json!({"cpuwait":true,"channels":[{"enabled":true,"address":0x50081080u32,"source":0x50084024u32,"destination":0x2004396cu32}]});
        ready_guard(&value).unwrap();
        let mut nor=value.clone();nor["channels"][0]["destination"]=json!(0x12420000u32);assert!(ready_guard(&nor).is_err());
        let mut unknown=value.clone();unknown["channels"][0]["address"]=json!(0x50081008u32);assert!(ready_guard(&unknown).is_err());
        let mut lcpu=value;lcpu["cpuwait"]=json!(false);assert!(ready_guard(&lcpu).is_err());
    }
    #[test]
    fn failed_final_checkpoint_returns_protocol_error_after_valid_restore() {
        let root=std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../artifacts");
        let path=root.join(format!("sector-checkpoint-fault-{}-{}",std::process::id(),std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_nanos()));
        fs::create_dir(&path).unwrap();let baseline=vec![0xff;0x1000000];let candidate=vec![0x5a;SECTOR];
        let trace=Trace::new(&path).unwrap();let peer=crate::simulated::Peer::b_sector(baseline.clone(),"success");
        let base=sftool_lib::SifliToolBase::new_with_external_stub_and_cancel("SIMULATED-STUB".into(),sftool_lib::BeforeOperation::NoReset,"nor".into(),1000000,1,false,sftool_lib::progress::no_op_progress_sink(),None,sftool_lib::CancelToken::default());
        let mut tool=SF32LB52Tool {base,port:Box::new(crate::port::Recorded::new(Box::new(peer),trace.clone()))};let mut report=json!({});
        let result=execute_with(&mut tool,&trace,&path,&baseline,&candidate,&mut report,|out,v| {
            if v["restored_verified"]==true {Err(std::io::Error::other("Synthetic checkpoint sync failure").into())}else{checkpoint(out,v)}
        });
        assert!(result.is_err());assert_eq!(report["restored_verified"],true);
        assert!(!crate::can_start_original(result.is_ok(),&report,&trace));
        drop(tool);trace.finish().unwrap();
        assert!(!fs::read(path.join("tx.bin")).unwrap().windows(10).any(|p|p==b"burn_reset"));
    }
}
