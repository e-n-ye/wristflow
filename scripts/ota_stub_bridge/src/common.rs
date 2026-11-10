//! Shared finite snapshot and owned reset; no Flash write operations.
use crate::Result;
use crate::port;
use serde::Serialize;
use serde_json::{Value,json};
use sftool_lib::{sf32lb52::SF32LB52Tool,common::sifli_debug::{SifliDebug,SifliUartCommand}};
use std::{thread,time::Duration};

#[derive(Serialize)]
struct Channel { address: u32, enabled: bool, ccr: u32, count: u32, source: u32, destination: u32 }

// Addresses and offsets are from the locked SDK SF32LB52 register definitions.
// A snapshot reports hardware values; it does not classify an enabled DMA as owned.
pub fn snapshot(tool: &mut SF32LB52Tool) -> Result<Value> {
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


pub fn pulse(tool: &mut SF32LB52Tool, trace: &port::Trace, role: &str) -> Result<()> {
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
