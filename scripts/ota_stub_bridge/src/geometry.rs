//! Fixed RAM observations derived from the pinned stub, never an erase permit.
use crate::Result;
use crate::port::Trace;
use serde::Serialize;
use serde_json::{Value,json};
use sftool_lib::{sf32lb52::SF32LB52Tool,common::sifli_debug::{SifliDebug,SifliUartCommand}};
use std::{fs,path::Path};

pub const IMAGE_START:u32=0x2005a000;
pub const IMAGE_END:u32=IMAGE_START+132816;
// The RTOS UART object is distinct from the debug-only UART at 0x20000ad8.
// The pinned UART ISR adds 0x158 to this base before calling the serial ISR.
pub const UART:u32=0x20042260;
pub const BLOCKS:[(u32,usize);5]=[(0x20041ff4,90),(0x200007ac,18),(0x20000464,2),(0x20000730,2),(UART,30)];
pub const HANDLES:[u32;6]=[0x200007ac,0x20041ff4,0x2004203c,0x20042084,0x200420cc,0x20042114];

#[derive(Serialize)]
struct Block {address:u32,words:Vec<u32>}
fn image_record(address:u32)->Option<u32> {
    let aligned=address&!3;
    if aligned>=IMAGE_START && aligned.checked_add(12)?<=IMAGE_END {Some(aligned)}else{None}
}
fn dma_handle(address:u32)->bool {
    // Only one UART-derived 64B structure in the fixed stub's low-RAM window.
    address%4==0 && (0x20000000..=IMAGE_START-64).contains(&address)
}
fn word(blocks:&[Block],address:u32)->Result<u32> {
    for block in blocks {
        if address>=block.address && (address-block.address)%4==0 {
            if let Some(value)=block.words.get(((address-block.address)/4) as usize) {return Ok(*value)}
        }
    }
    Err(std::io::Error::other("Missing fixed geometry word").into())
}
fn read(tool:&mut SF32LB52Tool,trace:&Trace,address:u32,count:usize)->Result<Block> {
    trace.event(json!({"kind":"geometry_read_intent","address":address,"bytes":count*4}))?;
    let mut words=Vec::with_capacity(count);
    for offset in 0..count {words.push(tool.debug_read_word32(address+offset as u32*4)?)}
    Ok(Block {address,words})
}
pub fn observe(tool:&mut SF32LB52Tool,trace:&Trace,out:&Path)->Result<Value> {
    tool.debug_command(SifliUartCommand::Enter)?;
    let result=(|| ->Result<Value> {
        let mut blocks=Vec::new();
        for (address,count) in BLOCKS {
            let block=read(tool,trace,address,count)?;
            fs::write(out.join(format!("geometry-{address:08x}.json")),serde_json::to_vec_pretty(&block)?)?;
            blocks.push(block);
        }
        let pointer=word(&blocks,UART+0x6c)?;
        let uart_dma=if dma_handle(pointer) {
            Some(read(tool,trace,pointer,16)?)
        }else{None};
        let mut tables=Vec::new();
        for handle in HANDLES {
            let pointer=word(&blocks,handle+0x0c)?;
            let mut records=Vec::new();
            for (index,offset) in [(27,0x103u32),(41,0x181u32)] {
                if let Some(address)=pointer.checked_add(offset).and_then(image_record) {
                    records.push(json!({"index":index,"block":read(tool,trace,address,3)?}));
                }
            }
            tables.push(json!({"handle":handle,"pointer":pointer,"records":records}));
        }
        let value=json!({"blocks":blocks,"uart_address":UART,"uart_dma_pointer":pointer,"uart_dma":uart_dma,"tables":tables,
            "all_flash_users_controlled":false,"physical_erase_envelope_verified":false,
            "limits":"Fixed RAM facts only; pointers outside bounded windows are recorded but never dereferenced"});
        fs::write(out.join("geometry.json"),serde_json::to_vec_pretty(&value)?)?;
        Ok(value)
    })();
    let exit=tool.debug_command(SifliUartCommand::Exit);
    match result {Ok(value)=>{exit?;Ok(value)},Err(error)=>Err(error)}
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn pointer_windows_cannot_escape_original_stub_ram() {
        for address in [0,0x12000000,0x40000000,u32::MAX,IMAGE_START-1,IMAGE_END] {
            assert!(image_record(address).is_none());
        }
        assert_eq!(image_record(IMAGE_START+3),Some(IMAGE_START));
        assert!(dma_handle(0x20002000));assert!(dma_handle(IMAGE_START-64));
        for address in [0,0x20002001,IMAGE_START-60,0x40000000] {assert!(!dma_handle(address))}
    }
}
