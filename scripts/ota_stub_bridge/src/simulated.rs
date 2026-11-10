//! Synthetic peer for the actual official-library calls, with no device import/open.
use serialport::*;
use std::{collections::{HashMap,VecDeque},io::{self,Read,Write},sync::{Arc,Mutex},time::Duration};
const CRC_ALGORITHM:crc::Algorithm<u32>=crc::Algorithm {width:32,poly:0x04c11db7,init:0,refin:true,refout:true,xorout:0,check:0,residue:0};

struct State {input:Vec<u8>,output:VecDeque<u8>,nor:Vec<u8>,registers:HashMap<u32,u32>,
    scenario:String,reads:usize,fragments:usize,ready:bool,timeout:Duration}
pub struct Peer {state:Arc<Mutex<State>>,writes:Arc<std::sync::atomic::AtomicUsize>}
impl Peer {
    pub fn new(nor:Vec<u8>,scenario:&str)->Self {
        Self {state:Arc::new(Mutex::new(State {input:vec![],output:VecDeque::new(),nor,
            registers:HashMap::new(),scenario:scenario.into(),reads:0,fragments:0,ready:false,timeout:Duration::from_secs(5)})),
            writes:Arc::new(std::sync::atomic::AtomicUsize::new(0))}
    }
    #[cfg(test)]
    pub fn write_counter(&self)->Arc<std::sync::atomic::AtomicUsize> {self.writes.clone()}
}
impl State {
    fn response(&mut self,code:u8,data:&[u8]) {
        self.output.extend([0x7e,0x79]);self.output.extend(((data.len()+2) as u16).to_le_bytes());
        self.output.extend([0x10,0,code]);self.output.extend(data);self.output.push_back(6);
    }
    fn value(&self,address:u32)->u32 {
        if address==0x40040000 {return if self.scenario=="lcpu-running" {0}else{4}}
        if address==0x50081080 {return 0x10af} // A deliberately active synthetic UART RX DMA.
        if address==0x50081084 {return 32362}
        if address==0x50081088 {return 0x50084024}
        if address==0x5008108c {return 0x2004396c}
        let values=[(0x20042260,0x50084000),(0x200422b4,0x2004396c),(0x200422b8,32768),
            (0x200422cc,if self.scenario=="geometry-bad-pointer" {0x12000000}else{0x20002000}),
            (0x20002000,0x50081080),(0x20002030,0x20042260),(0x20002034,0x20062321),
            (0x20002038,0x20062355),(0x2000203c,0x200622cb),
            (0x20041ff4,0x50000000),(0x20042000,if self.scenario=="geometry-bad-pointer" {0x50000000}else{0x20076000}),
            (0x20042004,0x12000000),(0x20042008,0x01000000),(0x20042030,0x12000000),(0x20042034,0x01000000)];
        if let Some((_,value))=values.iter().find(|(key,_)|*key==address) {return *value}
        if address==0xe000edf0 {return 0x00030003}
        *self.registers.get(&address).unwrap_or(&0)
    }
    fn consume(&mut self)->io::Result<()> {
        loop {
            if self.input.is_empty() {return Ok(())}
            if self.input[0]==b'\n' {self.input.remove(0);continue}
            if self.input[0]==0x7e {
                if self.input.len()<8 {return Ok(())}
                if self.input[1]!=0x79 {return Err(io::Error::other("Synthetic unknown frame"))}
                let declared=u16::from_le_bytes(self.input[2..4].try_into().unwrap()) as usize;
                let payload=if self.input[6..8]==[0x40,0x77] {
                    if self.input.len()<14 {return Ok(())}
                    8+4*u16::from_le_bytes(self.input[12..14].try_into().unwrap()) as usize
                } else {declared};
                if self.input.len()<6+payload {return Ok(())}
                let frame:Vec<_>=self.input.drain(..6+payload).collect();let p=&frame[6..];
                if p==[0x41,0x54,0x53,0x46,0x33,0x32,5,0x21] {self.response(0xd1,&[])}
                else if p==[0x41,0x54,0x53,0x46,0x33,0x32,0x18,0x21] {if self.ready {self.output.extend(b"msh >")}}
                else if p.starts_with(&[0x40,0x72]) {
                    let address=u32::from_le_bytes(p[2..6].try_into().unwrap());
                    let words=u16::from_le_bytes(p[6..8].try_into().unwrap());
                    if self.scenario=="register-read-fail" && address==0x40040000 {
                        return Err(io::Error::other("Synthetic inaccessible LP register"))
                    }
                    let mut data=vec![];for i in 0..words {data.extend(self.value(address+u32::from(i)*4).to_le_bytes())}
                    self.response(0xd2,&data)
                } else if p.starts_with(&[0x40,0x77]) {
                    let address=u32::from_le_bytes(p[2..6].try_into().unwrap());
                    if (0x12000000..0x13000000).contains(&address) {return Err(io::Error::other("No simulated Flash writes allowed"))}
                    for (index,word) in p[8..].chunks_exact(4).enumerate() {
                        let value=u32::from_le_bytes(word.try_into().unwrap());
                        let target=address+index as u32*4;
                        self.registers.insert(target,value);
                        if target==0xe000edf0 && value&3==1 {self.ready=true}
                    }
                    self.response(0xd3,&[])
                } else {return Err(io::Error::other("Synthetic unknown DAP command"))}
            } else {
                let Some(end)=self.input.iter().position(|&v|v==b'\r') else {return Ok(())};
                let command=String::from_utf8(self.input.drain(..=end).collect()).map_err(io::Error::other)?;
                let parts:Vec<_>=command.split_whitespace().collect();
                match parts.as_slice() {
                    []=>{if self.ready {self.output.extend(b"msh >")}},
                    ["burn_read","0x12000000","0x01000000"]=>{
                        self.reads+=1;
                        let mut data=self.nor.clone();
                        if self.scenario=="immutable-changed" {data[0x10000]^=1}
                        if self.scenario=="pair-changed" && self.reads==2 {data[0x9a0000]^=1}
                        let checksum=crc::Crc::<u32>::new(&CRC_ALGORITHM).checksum(&data);
                        let checksum=if self.scenario=="crc-fail" {checksum^1}else{checksum};
                        if self.scenario=="short-read" {data.drain(0x503d60..0x503d60+152);}
                        self.output.extend(b"start_trans\r\n");self.output.extend(data);
                        self.output.extend(format!("\r\ncrc:0x{checksum:08x}\r\nOK\r\nmsh >").bytes());
                    },
                    ["burn_reset"]=>{self.output.extend(b"OK\r\n");self.ready=false},
                    _=>return Err(io::Error::other("Observer tried an unknown or mutating stub command")),
                }
            }
        }
    }
}
impl Read for Peer {
    fn read(&mut self,buf:&mut [u8])->io::Result<usize> {
        let mut s=self.state.lock().unwrap();
        if s.scenario=="hang-read" {drop(s);loop {std::thread::sleep(Duration::from_secs(1))}}
        if s.output.is_empty() {return Err(io::Error::new(io::ErrorKind::TimedOut,"Synthetic empty queue"))}
        let limit=if s.scenario=="fragmented" {s.fragments+=1;if s.fragments%2==1 {1}else{31}}else{4096};
        let count=buf.len().min(s.output.len()).min(limit);
        for value in &mut buf[..count] {*value=s.output.pop_front().unwrap()}
        Ok(count)
    }
}
impl Write for Peer {
    fn write(&mut self,buf:&[u8])->io::Result<usize> {self.writes.fetch_add(1,std::sync::atomic::Ordering::Relaxed);let mut s=self.state.lock().unwrap();s.input.extend(buf);s.consume()?;Ok(buf.len())}
    fn flush(&mut self)->io::Result<()> {Ok(())}
}
macro_rules! constant {($($name:ident -> $kind:ty = $value:expr);* $(;)?)=>{$(fn $name(&self)->$kind {$value})*};}
impl SerialPort for Peer {
    constant! {name->Option<String>=Some("SIMULATED-STUB".into());baud_rate->serialport::Result<u32>=Ok(1_000_000);
        data_bits->serialport::Result<DataBits>=Ok(DataBits::Eight);flow_control->serialport::Result<FlowControl>=Ok(FlowControl::None);
        parity->serialport::Result<Parity>=Ok(Parity::None);stop_bits->serialport::Result<StopBits>=Ok(StopBits::One);
        bytes_to_write->serialport::Result<u32>=Ok(0)}
    fn timeout(&self)->Duration {self.state.lock().unwrap().timeout}
    fn set_timeout(&mut self,value:Duration)->serialport::Result<()> {self.state.lock().unwrap().timeout=value;Ok(())}
    fn set_baud_rate(&mut self,_:u32)->serialport::Result<()> {Err(serialport::Error::new(ErrorKind::InvalidInput,"Observer must retain1M"))}
    fn set_data_bits(&mut self,_:DataBits)->serialport::Result<()> {Ok(())}
    fn set_flow_control(&mut self,_:FlowControl)->serialport::Result<()> {Ok(())}
    fn set_parity(&mut self,_:Parity)->serialport::Result<()> {Ok(())}
    fn set_stop_bits(&mut self,_:StopBits)->serialport::Result<()> {Ok(())}
    fn write_request_to_send(&mut self,value:bool)->serialport::Result<()> {
        if value {let mut s=self.state.lock().unwrap();s.output.clear();s.input.clear();s.ready=false}
        Ok(())
    }
    fn write_data_terminal_ready(&mut self,_:bool)->serialport::Result<()> {Ok(())}
    fn read_clear_to_send(&mut self)->serialport::Result<bool> {Ok(false)}
    fn read_data_set_ready(&mut self)->serialport::Result<bool> {Ok(false)}
    fn read_ring_indicator(&mut self)->serialport::Result<bool> {Ok(false)}
    fn read_carrier_detect(&mut self)->serialport::Result<bool> {Ok(false)}
    fn bytes_to_read(&self)->serialport::Result<u32> {Ok(self.state.lock().unwrap().output.len() as u32)}
    fn clear(&self,which:ClearBuffer)->serialport::Result<()> {
        if matches!(which,ClearBuffer::All|ClearBuffer::Input) {self.state.lock().unwrap().output.clear()}
        Ok(())
    }
    fn try_clone(&self)->serialport::Result<Box<dyn SerialPort>> {Ok(Box::new(Self {state:self.state.clone(),writes:self.writes.clone()}))}
    fn set_break(&self)->serialport::Result<()> {Ok(())}
    fn clear_break(&self)->serialport::Result<()> {Ok(())}
}
