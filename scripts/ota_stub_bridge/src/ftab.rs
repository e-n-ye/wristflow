//! One physical FTAB read using the pinned library, with host RX error evidence.
use crate::{Result,hash,require,port};
use serde_json::{Value,json};
use sftool_lib::{ReadFlashFile,ReadFlashParams,read_flash::ReadFlashTrait,sf32lb52::SF32LB52Tool,
    common::sifli_debug::{SifliDebug,SifliUartCommand}};
use std::{fs,io,path::Path,time::Duration};

pub struct Monitor {
    scenario:String,
    #[cfg(windows)]
    native:Option<serialport::COMPort>,
}
impl Monitor {
    pub fn simulated(scenario:&str)->Self {
        Self {scenario:scenario.into(),#[cfg(windows)] native:None}
    }
    pub fn open(name:&str)->Result<(Box<dyn serialport::SerialPort>,Self)> {
        #[cfg(windows)] {
            let port=serialport::new(name,1_000_000).dtr_on_open(false).timeout(Duration::from_secs(5)).open_native()?;
            // Duplicate the same open endpoint solely for status. No reads, writes,
            // setting changes or new CreateFile call occur on this handle.
            let status=port.try_clone_native()?;
            Ok((Box::new(port),Self {scenario:String::new(),native:Some(status)}))
        }
        #[cfg(not(windows))] {
            let _=name;Err(io::Error::other("FTAB host status requires Windows").into())
        }
    }
    fn sample(&self,phase:&str)->Result<Value> {
        if !self.scenario.is_empty() {
            require(self.scenario!="host-status-fail","Synthetic host receive status unavailable")?;
            let errors=if (self.scenario=="host-error-before" && phase=="before") ||
                (self.scenario=="host-error-after" && phase=="after") {2}else{0};
            return Ok(status(errors,0,0,"synthetic"));
        }
        #[cfg(windows)] {
            use std::os::windows::io::AsRawHandle;
            use winapi::{um::{commapi::ClearCommError,winbase::COMSTAT},shared::minwindef::DWORD};
            let native=self.native.as_ref().ok_or("Missing native status handle")?;
            let mut errors:DWORD=0;
            let mut queues:COMSTAT=unsafe {std::mem::zeroed()};
            // ClearCommError acknowledges error bits; it does not purge RX bytes.
            if unsafe {ClearCommError(native.as_raw_handle().cast(),&mut errors,&mut queues)}==0 {
                return Err(io::Error::last_os_error().into());
            }
            Ok(status(errors,queues.cbInQue,queues.cbOutQue,"Windows ClearCommError"))
        }
        #[cfg(not(windows))] {Err(io::Error::other("Native host status unavailable").into())}
    }
}
fn status(errors:u32,input:u32,output:u32,source:&str)->Value {
    let names:Vec<_>=[(1,"rx_overflow"),(2,"overrun"),(4,"parity"),(8,"framing"),(16,"break")]
        .into_iter().filter_map(|(bit,name)|if errors&bit!=0 {Some(name)}else{None}).collect();
    json!({"source":source,"errors":errors,"flags":names,"input_queued":input,"output_queued":output,
        "acknowledges_error_bits":true,"purges_input":false})
}

pub fn observe(tool:&mut SF32LB52Tool,trace:&port::Trace,out:&Path,baseline:&[u8],monitor:&Monitor,report:&mut Value)->Result<()> {
    // Match the execution package's physical-address gate. Failed debug reads
    // return immediately, with no Exit or Flash command on an unknown protocol.
    tool.debug_command(SifliUartCommand::Enter)?;
    let aliases=[tool.debug_read_word32(0x5004206c)?,tool.debug_read_word32(0x50042070)?,tool.debug_read_word32(0x50042074)?];
    tool.debug_command(SifliUartCommand::Exit)?;
    report["ftab"]=json!({"aliases":aliases,"address":0x12000000u32,"bytes":4096,"read_passed":false});
    trace.event(json!({"kind":"ftab_aliases","aliases":aliases}))?;
    require(aliases==[0,0,0],"FTAB alias not zero; no Flash read")?;
    let ready=&report["ready_registers"];
    require(ready["cpuwait"]==true,"LCPU not waiting; no FTAB read")?;
    let channels=ready["channels"].as_array().ok_or("Missing ready DMA channels")?;
    require(channels.iter().all(|c|c["enabled"]==false || (c["address"]==0x50081080u32 &&
        c["source"]==0x50084024u32 && c["destination"].as_u64().is_some_and(|v|(0x20000000..0x20080000).contains(&v)))),
        "Unexpected enabled DMA; no FTAB read")?;
    let before=monitor.sample("before")?;
    report["ftab"]["host_before"]=before.clone();
    trace.event(json!({"kind":"host_receive_status","phase":"before","status":before}))?;
    require(before["errors"]==0,"Host receive errors before FTAB; no Flash read")?;
    let start=fs::metadata(out.join("rx.bin"))?.len();
    report["ftab"]["rx_start"]=json!(start);
    trace.event(json!({"kind":"ftab_read_intent","address":0x12000000u32,"bytes":4096,"rx_offset":start}))?;
    let path=out.join("ftab4k.bin");
    let read=tool.read_flash(&ReadFlashParams {files:vec![ReadFlashFile {file_path:path.to_string_lossy().into_owned(),address:0x12000000,size:4096}]});
    // Retain both facts even when the official read failed; no retry or protocol
    // recovery is inferred from a clean Windows status sample.
    if let Err(error)=&read {report["ftab"]["library_error"]=json!(error.to_string());}
    let after=monitor.sample("after");
    match &after {
        Ok(value)=>report["ftab"]["host_after"]=value.clone(),
        Err(error)=>report["ftab"]["host_after_error"]=json!(error.to_string()),
    }
    trace.event(json!({"kind":"ftab_read_finished","library_ok":read.is_ok(),"host_after":report["ftab"]["host_after"]}))?;
    report["ftab"]["rx_end"]=json!(fs::metadata(out.join("rx.bin"))?.len());
    read?;
    require(after?["errors"]==0,"Host receive errors during FTAB read")?;
    let data=fs::read(path)?;
    require(data.len()==4096 && data==baseline[..4096],"FTAB differs from bound original bytes")?;
    report["ftab"]["sha256"]=json!(hash(&data));
    report["ftab"]["read_passed"]=json!(true);
    trace.event(json!({"kind":"ftab_read_verified","bytes":4096,"sha256":hash(&data)}))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn host_flags_keep_unknown_bits_and_never_turn_errors_into_clean_state() {
        let value=status(0x8000000f,7,3,"synthetic");
        assert_eq!(value["errors"],0x8000000fu32);
        assert_eq!(value["flags"],json!(["rx_overflow","overrun","parity","framing"]));
        assert_eq!(value["input_queued"],7);assert_eq!(value["purges_input"],false);
    }
}
