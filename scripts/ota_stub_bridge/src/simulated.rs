//! Synthetic peer for the actual official-library calls, with no device import/open.
use serialport::*;
use std::{collections::{HashMap,VecDeque},io::{self,Read,Write},sync::{Arc,Mutex},time::Duration};
const CRC_ALGORITHM:crc::Algorithm<u32>=crc::Algorithm {width:32,poly:0x04c11db7,init:0,refin:true,refout:true,xorout:0,check:0,residue:0};
pub const B_SCENARIOS:&[&str]=&["success","fragmented","lcpu-running","register-read-fail","before-changed","prestate-crc-fail",
    "candidate-mismatch","guard-change","restore-mismatch","write-start-fail","candidate-data-timeout","readback-crc-fail",
    "restore-data-fail","b-hang-read"];

struct State {input:Vec<u8>,output:VecDeque<u8>,nor:Vec<u8>,registers:HashMap<u32,u32>,
    scenario:String,reads:usize,fragments:usize,ready:bool,timeout:Duration,b_sector:bool,pending:bool,sector_writes:usize,
    execution:bool,exec_pending:Option<(usize,usize)>,exec_writes:[usize;2],entries:usize,boot:Vec<u8>,frames:Vec<u8>}
pub struct Peer {state:Arc<Mutex<State>>,writes:Arc<std::sync::atomic::AtomicUsize>}
impl Peer {
    pub fn new(nor:Vec<u8>,scenario:&str)->Self {
        Self {state:Arc::new(Mutex::new(State {input:vec![],output:VecDeque::new(),nor,
            registers:HashMap::new(),scenario:scenario.into(),reads:0,fragments:0,ready:false,timeout:Duration::from_secs(5),b_sector:false,pending:false,sector_writes:0,
            execution:false,exec_pending:None,exec_writes:[0;2],entries:0,boot:vec![],frames:vec![]})),
            writes:Arc::new(std::sync::atomic::AtomicUsize::new(0))}
    }
    #[cfg(test)]
    pub fn write_counter(&self)->Arc<std::sync::atomic::AtomicUsize> {self.writes.clone()}
    pub fn b_sector(nor:Vec<u8>,scenario:&str)->Self {
        let peer=Self::new(nor,scenario);peer.state.lock().unwrap().b_sector=true;peer
    }
    pub fn execution(nor:Vec<u8>,boot:Vec<u8>,frames:Vec<u8>,scenario:&str)->Self {
        let peer=Self::new(nor,scenario);{
            let mut s=peer.state.lock().unwrap();s.execution=true;s.boot=boot;s.frames=frames;
            if scenario=="boot-header-changed" {s.nor[0x1200]^=1;}
            if scenario=="boot-route-changed" {s.nor[0x2c04]^=1;}
        }peer
    }
}
impl State {
    fn response(&mut self,code:u8,data:&[u8]) {
        self.output.extend([0x7e,0x79]);self.output.extend(((data.len()+2) as u16).to_le_bytes());
        self.output.extend([0x10,0,code]);self.output.extend(data);self.output.push_back(6);
    }
    fn value(&self,address:u32)->u32 {
        if self.scenario=="ftab-alias-nonzero" && address==0x50042070 {return 0x400000}
        if self.execution && self.scenario=="alias-retained" && self.entries==2 && (0x5004206c..=0x50042074).contains(&address) {return 0x400000}
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
            if let Some((address,size))=self.exec_pending {
                if self.input.len()<size {return Ok(())}
                let data:Vec<_>=self.input.drain(..size).collect();self.nor[address..address+size].copy_from_slice(&data);self.exec_pending=None;
                self.output.extend(b"OK\r\nmsh >");continue
            }
            if self.pending {
                if self.input.len()<4096 {return Ok(())}
                let data:Vec<_>=self.input.drain(..4096).collect();self.nor[0x420000..0x421000].copy_from_slice(&data);
                self.pending=false;
                if (self.scenario=="candidate-mismatch" && self.sector_writes==1) ||
                    (self.scenario=="restore-mismatch" && self.sector_writes==2) {self.nor[0x420000]^=1}
                if self.scenario=="guard-change" && self.sector_writes==1 {self.nor[0x421000]^=1}
                if self.scenario=="candidate-data-timeout" && self.sector_writes==1 {return Ok(())}
                if self.scenario=="restore-data-fail" && self.sector_writes==2 {self.output.extend(b"Fail\r\n");return Ok(())}
                self.output.extend(b"OK\r\nmsh >");continue
            }
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
                    if self.execution && ((self.scenario=="initial-register-fail" && self.entries==1 && address==0x40040000) ||
                        (self.scenario=="restore-register-fail" && self.entries==2 && address==0x40040000) ||
                        (self.scenario=="restore-alias-fail" && self.entries==2 && address==0x5004206c)) {
                        self.response(0xdf,&[]);continue;
                    }
                    if self.scenario=="ftab-alias-fail" && address==0x5004206c {
                        self.response(0xdf,&[]);continue;
                    }
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
                    ["burn_read",address,size] if self.execution=>{
                        let address=usize::from_str_radix(address.trim_start_matches("0x"),16).map_err(io::Error::other)?;
                        let size=usize::from_str_radix(size.trim_start_matches("0x"),16).map_err(io::Error::other)?;
                        if !((address==0x12000000 && size==12288) || ([0x12010000,0x12420000].contains(&address) && size==65536)) {return Err(io::Error::other("Synthetic unapproved read"))}
                        let offset=address-0x12000000;self.reads+=1;let mut data=self.nor[offset..offset+size].to_vec();
                        if self.scenario=="before-changed" && self.reads==1 {data[0]^=1}
                        if self.scenario=="b-compare" && address==0x12420000 && self.exec_writes[1]==1 && self.entries==1 {data[0]^=1}
                        if self.scenario=="boot-compare" && address==0x12010000 && self.exec_writes[0]==1 && self.entries==1 {data[0]^=1}
                        let checksum=crc::Crc::<u32>::new(&CRC_ALGORITHM).checksum(&data);
                        let checksum=if self.scenario=="restore-read-fail" && self.exec_writes[1]==2 {checksum^1}else{checksum};
                        self.output.extend(b"start_trans\r\n");self.output.extend(data);self.output.extend(format!("\r\ncrc:0x{checksum:08x}\r\nOK\r\nmsh >").bytes());
                    },
                    ["burn_erase_write",address,size] if self.execution=>{
                        let address=usize::from_str_radix(address.trim_start_matches("0x"),16).map_err(io::Error::other)?;
                        let size=usize::from_str_radix(size.trim_start_matches("0x"),16).map_err(io::Error::other)?;
                        let index=if address==0x12010000 && [58496,65536].contains(&size) {0}else if address==0x12420000 && size==4096 {1}else{return Err(io::Error::other("Synthetic unapproved mutation"))};
                        if self.exec_writes[index]>=2 {return Err(io::Error::other("Synthetic third write forbidden"))}
                        self.exec_writes[index]+=1;let offset=address-0x12000000;self.nor[offset..offset+size].fill(255);
                        if (self.scenario=="write-fail" && index==1 && self.exec_writes[1]==1) ||
                            (self.scenario=="restore-boot-fail" && index==0 && size==65536) ||
                            (self.scenario=="restore-b-fail" && index==1 && self.exec_writes[1]==2) {self.output.extend(b"Fail\r\n");}
                        else {self.exec_pending=Some((offset,size));self.output.extend(b"RX_WAIT\r\n");}
                    },
                    ["burn_reset"] if self.execution=>{
                        if (self.scenario=="probe-start-fail" && self.exec_writes==[1,1]) ||
                            (self.scenario=="original-start-fail" && self.exec_writes==[2,2]) {self.output.extend(b"Fail\r\n");continue;}
                        self.output.extend(b"OK\r\n");self.ready=false;
                        if self.nor[0x10000..0x10000+self.boot.len()]==self.boot {
                            let mut frames=self.frames.clone();
                            if self.scenario=="no-frames" {frames.clear();}
                            if self.scenario=="bad-tag" {frames[12]^=1;}
                            if self.scenario=="bad-crc" {frames[79]^=1;}
                            if self.scenario=="short-frames" {frames.truncate(239);}
                            if self.scenario=="duplicate-frames" {frames.extend(self.frames[..80].iter());}
                            self.output.extend(frames);
                        }
                    },
                    ["burn_read","0x12000000","0x00001000"]=>{
                        self.reads+=1;
                        if self.reads!=1 {return Err(io::Error::other("FTAB read cannot retry"))}
                        let mut data=self.nor[..4096].to_vec();
                        if self.scenario=="ftab-changed" {data[0]^=1;}
                        let checksum=crc::Crc::<u32>::new(&CRC_ALGORITHM).checksum(&data);
                        let checksum=if self.scenario=="ftab-crc-fail" {checksum^1}else{checksum};
                        if self.scenario=="ftab-short18" {data.truncate(4078);}
                        self.output.extend(b"start_trans\r\n");self.output.extend(data);
                        // The real fixed stub places CRC immediately after payload.
                        self.output.extend(format!("CRC:0x{checksum:08x}\r\nOK\r\nmsh >").bytes());
                    },
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
                    ["burn_read","0x12420000","0x00010000"] if self.b_sector=>{
                        self.reads+=1;let mut data=self.nor[0x420000..0x430000].to_vec();
                        if self.scenario=="before-changed" && self.reads==1 {data[0]^=1}
                        let crc=crc::Crc::<u32>::new(&CRC_ALGORITHM).checksum(&data);
                        let crc=if (self.scenario=="prestate-crc-fail" && self.reads==1) ||
                            (self.scenario=="readback-crc-fail" && self.reads==2) {crc^1}else{crc};
                        self.output.extend(b"start_trans\r\n");self.output.extend(data);
                        self.output.extend(format!("\r\ncrc:0x{crc:08x}\r\nOK\r\nmsh >").bytes());
                    },
                    ["burn_erase_write","0x12420000","0x00001000"] if self.b_sector=>{
                        if self.sector_writes>=2 {return Err(io::Error::other("Synthetic third write forbidden"))}
                        self.sector_writes+=1;self.nor[0x420000..0x421000].fill(255);
                        if self.scenario=="write-start-fail" {self.output.extend(b"Fail\r\n");}
                        else {self.pending=true;self.output.extend(b"RX_WAIT\r\n");}
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
        if s.scenario=="hang-read" || s.scenario=="b-hang-read" || s.scenario=="execute-hang" {drop(s);loop {std::thread::sleep(Duration::from_secs(1))}}
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
        if value {let mut s=self.state.lock().unwrap();s.output.clear();s.input.clear();s.ready=false;s.entries+=1;}
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
