//! Recording wraps the original serial object and all of its duplicate handles.
use serde_json::{Value,json};
use serialport::*;
use std::{fs::File,io::{self,BufWriter,Read,Write},path::Path,sync::{Arc,Mutex},time::{Duration,Instant}};

const RAW_CAPACITY:usize=64*1024;
const EVENT_CAPACITY:usize=256*1024;
struct Sink {file:File,writes:u64}
impl Write for Sink {
    fn write(&mut self,data:&[u8])->io::Result<usize> {self.writes+=1;self.file.write(data)}
    fn flush(&mut self)->io::Result<()> {self.file.flush()}
}
fn writer(path:&Path,capacity:usize)->io::Result<BufWriter<Sink>> {
    Ok(BufWriter::with_capacity(capacity,Sink {file:File::create_new(path)?,writes:0}))
}
struct Files {events:BufWriter<Sink>,rx:BufWriter<Sink>,tx:BufWriter<Sink>,intents:BufWriter<Sink>,rx_offset:u64,tx_offset:u64,intent_offset:u64,start:Instant,
    fault:Option<String>,cleanup_rts:bool,high_attempts:u32}
#[derive(Clone)]
pub struct Trace(Arc<Mutex<Files>>);
impl Trace {
    pub fn new(path:&Path)->io::Result<Self> {
        Ok(Self(Arc::new(Mutex::new(Files {events:writer(&path.join("wire-events.jsonl"),EVENT_CAPACITY)?,
            rx:writer(&path.join("rx.bin"),RAW_CAPACITY)?,tx:writer(&path.join("tx.bin"),RAW_CAPACITY)?,
            intents:writer(&path.join("tx-intents.bin"),RAW_CAPACITY)?,rx_offset:0,tx_offset:0,intent_offset:0,start:Instant::now(),
            fault:None,cleanup_rts:false,high_attempts:0}))))
    }
    fn flush_files(files:&mut Files)->io::Result<()> {
        // Try every stream, including after an earlier failure. Never rely on Drop.
        let mut first=None;
        for result in [files.rx.flush(),files.tx.flush(),files.intents.flush(),files.events.flush()] {
            if let Err(error)=result {files.fault=Some(error.to_string());if first.is_none() {first=Some(error)}}
        }
        match first {Some(error)=>Err(error),None=>Ok(())}
    }
    pub fn finish(&self)->io::Result<Value> {
        let mut f=self.0.lock().map_err(|_|io::Error::other("Trace poisoned"))?;
        Self::flush_files(&mut f)?;
        if let Some(error)=&f.fault {return Err(io::Error::other(error.clone()))}
        Ok(json!({"flushed":true,"raw_capacity":RAW_CAPACITY,"event_capacity":EVENT_CAPACITY,
            "writes":{"rx":f.rx.get_ref().writes,"tx":f.tx.get_ref().writes,
            "intents":f.intents.get_ref().writes,"events":f.events.get_ref().writes},
            "limits":"OS write completion, not power-loss durability; forced termination can lose buffered tails"}))
    }
    pub fn event(&self,mut value:Value)->io::Result<()> {
        let mut files=self.0.lock().map_err(|_|io::Error::other("Trace poisoned"))?;
        value["elapsed_ns"]=json!(files.start.elapsed().as_nanos().min(u64::MAX as u128) as u64);
        let written=writeln!(files.events,"{value}");
        let flushed=Self::flush_files(&mut files);
        let result=written.and(flushed);
        if let Err(error)=&result {files.fault=Some(error.to_string())}
        result
    }
    pub fn fault(&self)->Option<String> {
        self.0.lock().map(|files|files.fault.clone()).unwrap_or(Some("Trace mutex poisoned".into()))
    }
    fn mark_fault(&self,error:&io::Error) {if let Ok(mut files)=self.0.lock() {files.fault=Some(error.to_string())}}
    pub fn allow_cleanup_rts(&self) {if let Ok(mut files)=self.0.lock() {files.cleanup_rts=true}}
    fn cleanup_rts(&self)->bool {self.0.lock().map(|files|files.cleanup_rts).unwrap_or(false)}
    pub fn high_attempts(&self)->Option<u32> {self.0.lock().ok().map(|files|files.high_attempts)}
    fn bytes(&self,kind:&str,data:&[u8])->io::Result<u64> {
        let mut f=self.0.lock().map_err(|_|io::Error::other("Trace poisoned"))?;
        let offset=match kind {
            "rx"=>{let n=f.rx_offset;f.rx.write_all(data)?;f.rx_offset+=data.len() as u64;n},
            "tx"=>{let n=f.tx_offset;f.tx.write_all(data)?;f.tx_offset+=data.len() as u64;n},
            _=>{let n=f.intent_offset;f.intents.write_all(data)?;f.intent_offset+=data.len() as u64;n},
        };
        let elapsed=f.start.elapsed().as_nanos().min(u64::MAX as u128) as u64;
        writeln!(f.events,"{}",json!({"kind":kind,"offset":offset,"bytes":data.len(),"elapsed_ns":elapsed}))?;
        // Persist TX intent before sending, and detect post-send log failures without
        // reporting consumed serial bytes as unsent. RX alone stays buffered.
        if kind!="rx" {Self::flush_files(&mut f)?;}
        Ok(offset)
    }
}

pub struct Recorded {inner:Box<dyn SerialPort>,trace:Trace}
impl Recorded {pub fn new(inner:Box<dyn SerialPort>,trace:Trace)->Self {Self {inner,trace}}}
impl Recorded {
    fn check(&self)->io::Result<()> {match self.trace.fault() {Some(error)=>Err(io::Error::other(error)),None=>Ok(())}}
}
impl Read for Recorded {
    fn read(&mut self,data:&mut [u8])->io::Result<usize> {
        self.check()?;
        let n=self.inner.read(data)?;
        if n>0 {if let Err(error)=self.trace.bytes("rx",&data[..n]) {self.trace.mark_fault(&error);return Err(error)}}
        Ok(n)
    }
}
impl Write for Recorded {
    fn write(&mut self,data:&[u8])->io::Result<usize> {
        self.check()?;
        let intent=match self.trace.bytes("tx_intent",data) {Ok(value)=>value,Err(error)=>{
            self.trace.mark_fault(&error);return Err(error);
        }};
        match self.inner.write(data) {
            Ok(n)=>{
                // Tell BufWriter that the transmitted bytes were consumed, even if
                // the evidence write fails. A sticky fault forbids every later send.
                if let Err(error)=self.trace.bytes("tx",&data[..n]) {self.trace.mark_fault(&error)}
                Ok(n)
            },
            Err(error)=>{
                let _=self.trace.event(json!({"kind":"tx_error","intent_offset":intent,"error":error.to_string()}));
                self.trace.mark_fault(&error);Err(error)
            },
        }
    }
    fn flush(&mut self)->io::Result<()> {self.check()?;self.inner.flush()}
}
macro_rules! get {
    ($($name:ident -> $result:ty);* $(;)?)=>{$(fn $name(&self)->$result {self.inner.$name()})*};
}
macro_rules! set {
    ($($name:ident($value:ident:$argument:ty));* $(;)?)=>{$(fn $name(&mut self,$value:$argument)->serialport::Result<()> {self.inner.$name($value)})*};
}
impl SerialPort for Recorded {
    get! {name->Option<String>;baud_rate->serialport::Result<u32>;data_bits->serialport::Result<DataBits>;
        flow_control->serialport::Result<FlowControl>;parity->serialport::Result<Parity>;
        stop_bits->serialport::Result<StopBits>;timeout->Duration;bytes_to_read->serialport::Result<u32>;
        bytes_to_write->serialport::Result<u32>}
    set! {set_baud_rate(value:u32);set_data_bits(value:DataBits);set_flow_control(value:FlowControl);
        set_parity(value:Parity);set_stop_bits(value:StopBits);set_timeout(value:Duration)}
    fn write_request_to_send(&mut self,value:bool)->serialport::Result<()> {
        if value && !self.trace.cleanup_rts() {self.check().map_err(serialport::Error::from)?;}
        if let Err(error)=self.trace.event(json!({"kind":"rts_intent","value":value})) {
            if value && !self.trace.cleanup_rts() {return Err(serialport::Error::from(error))}
        }
        if value {if let Ok(mut files)=self.trace.0.lock() {files.high_attempts+=1}}
        let result=self.inner.write_request_to_send(value);
        let _=self.trace.event(json!({"kind":"rts_result","value":value,"ok":result.is_ok()}));
        result
    }
    fn write_data_terminal_ready(&mut self,value:bool)->serialport::Result<()> {self.inner.write_data_terminal_ready(value)}
    fn read_clear_to_send(&mut self)->serialport::Result<bool> {self.inner.read_clear_to_send()}
    fn read_data_set_ready(&mut self)->serialport::Result<bool> {self.inner.read_data_set_ready()}
    fn read_ring_indicator(&mut self)->serialport::Result<bool> {self.inner.read_ring_indicator()}
    fn read_carrier_detect(&mut self)->serialport::Result<bool> {self.inner.read_carrier_detect()}
    fn clear(&self,value:ClearBuffer)->serialport::Result<()> {
        self.trace.event(json!({"kind":"clear_buffers","buffer":format!("{value:?}")})).map_err(serialport::Error::from)?;
        self.inner.clear(value)
    }
    fn try_clone(&self)->serialport::Result<Box<dyn SerialPort>> {
        self.trace.event(json!({"kind":"duplicate_handle","new_port_open":false})).map_err(serialport::Error::from)?;
        Ok(Box::new(Self::new(self.inner.try_clone()?,self.trace.clone())))
    }
    fn set_break(&self)->serialport::Result<()> {self.inner.set_break()}
    fn clear_break(&self)->serialport::Result<()> {self.inner.clear_break()}
}

#[cfg(test)]
mod tests {
    use super::*;
    fn directory()->std::path::PathBuf {
        static NEXT:std::sync::atomic::AtomicUsize=std::sync::atomic::AtomicUsize::new(0);
        let root=std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../artifacts");
        let path=root.join(format!("stub-trace-fault-{}-{}-{}",std::process::id(),NEXT.fetch_add(1,std::sync::atomic::Ordering::Relaxed),std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_nanos()));
        std::fs::create_dir(&path).unwrap();path
    }
    #[test]
    fn logging_failure_after_transmission_cannot_resend_buffer() {
        let path=directory();let trace=Trace::new(&path).unwrap();
        trace.0.lock().unwrap().tx.get_mut().file=File::open(path.join("tx.bin")).unwrap();
        let peer=crate::simulated::Peer::new(vec![],"success");let count=peer.write_counter();
        let mut writer=BufWriter::new(Recorded::new(Box::new(peer),trace.clone()));
        writer.write_all(b"burn_reset\r").unwrap();assert!(writer.flush().is_err());drop(writer);
        assert_eq!(count.load(std::sync::atomic::Ordering::Relaxed),1);
        assert!(trace.fault().is_some());
    }
    #[test]
    fn logging_failure_before_transmission_never_sends_on_drop() {
        let path=directory();let trace=Trace::new(&path).unwrap();
        trace.0.lock().unwrap().intents.get_mut().file=File::open(path.join("tx-intents.bin")).unwrap();
        let peer=crate::simulated::Peer::new(vec![],"success");let count=peer.write_counter();
        let mut writer=BufWriter::new(Recorded::new(Box::new(peer),trace));
        writer.write_all(b"burn_reset\r").unwrap();assert!(writer.flush().is_err());drop(writer);
        assert_eq!(count.load(std::sync::atomic::Ordering::Relaxed),0);
    }
    #[test]
    fn recovered_logging_cannot_reenable_entry_after_sticky_fault() {
        let path=directory();let trace=Trace::new(&path).unwrap();
        trace.0.lock().unwrap().events.get_mut().file=File::open(path.join("wire-events.jsonl")).unwrap();
        let peer=crate::simulated::Peer::new(vec![],"success");
        let mut port=Recorded::new(Box::new(peer),trace.clone());
        port.write_request_to_send(false).unwrap();
        trace.0.lock().unwrap().events.get_mut().file=File::options().append(true).open(path.join("wire-events.jsonl")).unwrap();
        assert!(port.write_request_to_send(true).is_err());assert_eq!(trace.high_attempts(),Some(0));
        trace.allow_cleanup_rts();port.write_request_to_send(true).unwrap();
        port.write_request_to_send(false).unwrap();assert_eq!(trace.high_attempts(),Some(1));
    }
    #[test]
    fn fragmented_rx_keeps_bytes_and_offsets_with_bounded_file_writes() {
        let path=directory();let trace=Trace::new(&path).unwrap();let data=vec![0xa5;20_000];
        let mut offset=0;
        while offset<data.len() {
            for size in [1,31] {let end=(offset+size).min(data.len());
                if end>offset {trace.bytes("rx",&data[offset..end]).unwrap();offset=end;}}
        }
        assert_eq!(trace.0.lock().unwrap().rx.get_ref().writes,0);
        let stats=trace.finish().unwrap();assert_eq!(stats["writes"]["rx"],1);
        assert_eq!(stats["writes"]["events"],1);
        assert_eq!(std::fs::read(path.join("rx.bin")).unwrap(),data);
        let mut next=0;
        for line in std::fs::read_to_string(path.join("wire-events.jsonl")).unwrap().lines() {
            let event:Value=serde_json::from_str(line).unwrap();assert_eq!(event["offset"],next);
            next+=event["bytes"].as_u64().unwrap();
        }
        assert_eq!(next,data.len() as u64);
    }
    #[test]
    fn buffered_tail_flush_failure_is_sticky_and_blocks_protocol() {
        let path=directory();let trace=Trace::new(&path).unwrap();
        trace.0.lock().unwrap().rx.get_mut().file=File::open(path.join("rx.bin")).unwrap();
        trace.bytes("rx",b"pending").unwrap();assert!(trace.finish().is_err());
        let peer=crate::simulated::Peer::new(vec![],"success");let count=peer.write_counter();
        let mut port=Recorded::new(Box::new(peer),trace.clone());
        assert!(port.write(b"burn_reset\r").is_err());assert_eq!(count.load(std::sync::atomic::Ordering::Relaxed),0);
        assert!(trace.fault().is_some());
    }
    #[test]
    fn full_rx_buffer_failure_aborts_before_next_read() {
        let path=directory();let trace=Trace::new(&path).unwrap();
        trace.0.lock().unwrap().rx.get_mut().file=File::open(path.join("rx.bin")).unwrap();
        let mut peer=crate::simulated::Peer::new(vec![],"success");
        peer.write_all(b"burn_read 0x12000000 0x01000000\r").unwrap();
        let probe=peer.try_clone().unwrap();
        let mut port=Recorded::new(Box::new(peer),trace.clone());
        trace.bytes("rx",&vec![0;RAW_CAPACITY-1]).unwrap();
        // The real Read wrapper, not its caller, must latch overflow errors.
        assert!(port.read(&mut [0;2]).is_err());assert!(trace.fault().is_some());
        let remaining=probe.bytes_to_read().unwrap();
        assert!(port.read(&mut [0;1]).is_err());assert!(trace.finish().is_err());
        assert_eq!(probe.bytes_to_read().unwrap(),remaining);
    }
}
