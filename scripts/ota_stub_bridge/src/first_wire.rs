use crate::{checkpoint, common, hash, media, port, reads, require, save, sector, Config, Result};
use media::{Action, Inputs, Nor, BASE, SIZE};
use serde_json::{json, Value};
use sftool_lib::{BeforeOperation, CancelToken, ReadFlashFile, ReadFlashParams, SifliToolBase,
    common::{ram_command::{Command, DownloadStub, RamCommand, Response}, sifli_debug::{SifliDebug, SifliUartCommand}},
    read_flash::ReadFlashTrait, sf32lb52::SF32LB52Tool, speed::SpeedTrait};
use std::{fs, io::{self,Read,Seek,SeekFrom,Write}, sync::{Arc, atomic::{AtomicBool, Ordering}}, thread, time::{Duration, Instant}};

struct Session<'a> { cfg: &'a Config, tool: SF32LB52Tool, trace: port::Trace }
impl Session<'_> {
    fn check(&self) -> Result<()> {
        self.tool.base.cancel_token.check_cancelled()?;
        require(self.trace.fault().is_none() && !self.cfg.cancel.exists(),"Cancelled or failed wire log")
    }
    fn speed(&mut self, old: u32, new: u32) -> Result<()> {
        self.check()?;
        require(self.tool.port.baud_rate()? == old,"Unexpected baud before handshake")?;
        self.tool.set_speed(new)?;
        require(self.tool.port.baud_rate()? == new,"Baud handshake failed")?;
        self.trace.event(json!({"kind":"speed","from":old,"to":new}))?;
        Ok(())
    }
    fn enter(&mut self) -> Result<()> {
        self.check()?;
        common::pulse(&mut self.tool,&self.trace,"entry")?;
        self.tool.download_stub()?;
        let state=common::snapshot_strict(&mut self.tool)?;
        common::receive_exit(&mut self.tool,&self.trace,"ownership")?;
        self.tool.debug_command(SifliUartCommand::Enter)?;
        let aliases=[self.tool.debug_read_word32(0x5004206c)?,self.tool.debug_read_word32(0x50042070)?,
                     self.tool.debug_read_word32(0x50042074)?];
        if self.cfg.action==Action::TraceOriginalA {
            let prior=self.tool.debug_read_word32(0x500cb044)?;
            self.checkpoint(json!({"ordinary_rtc_word":prior}))?;
            require(prior==0,"Ordinary A requires a clear OTA word")?;
        }
        self.tool.debug_command(SifliUartCommand::Exit)?;
        common::receive_exit(&mut self.tool,&self.trace,"physical")?;
        self.checkpoint(json!({"registers":state,"aliases":aliases}))?;
        sector::ready_guard(&state)?;
        require(aliases == [0;3],"Alias is not zero")?;
        self.speed(1_000_000,500_000)
    }
    fn start(&mut self) -> Result<()> {
        let mut start_receiver = true;
        if self.cfg.action.short_entry() {
            let ready = self.cfg.out.join("phone-ready");
            let decline = self.cfg.out.join("phone-unavailable");
            self.checkpoint(json!({"waiting_for_phone":true}))?;
            save(&self.cfg.out.join("await-phone"),b"")?;
            let until = Instant::now() + Duration::from_secs(175);
            while !ready.exists() && !decline.exists() && Instant::now() < until {
                self.check()?;
                thread::sleep(Duration::from_millis(100));
            }
            start_receiver = ready.exists() && !decline.exists();
            self.checkpoint(json!({"phone_ready":start_receiver}))?;
        }
        self.speed(500_000,1_000_000)?;
        if start_receiver && matches!(self.cfg.action,Action::Install|Action::EnterReceiver|Action::InspectReceiver|
                                    Action::InstallIdentity|Action::EnterIdentity|Action::EnterTrialB|Action::TraceTrialB|Action::TraceJournalDma|Action::TestFixedRollback) {
            // SF32LB52 RTC_BASE + offsetof(RTC_TypeDef, BKP5R), the SDK's
            // RTC_BAKCUP_OTA_FORCE_MODE. Preserve any unknown existing value.
            const REQUEST_REGISTER:u32 = 0x500cb044;
            const ENTER:u32 = 0x57464f54;
            self.tool.debug_command(SifliUartCommand::Enter)?;
            let old=self.tool.debug_read_word32(REQUEST_REGISTER)?;
            require(old==0 || old==ENTER,"OTA backup word already in use")?;
            self.checkpoint(json!({"intent":"one-shot-receiver","prior":old}))?;
            self.tool.debug_write_word32(REQUEST_REGISTER,ENTER)?;
            require(self.tool.debug_read_word32(REQUEST_REGISTER)?==ENTER,"One-shot word readback failed")?;
            self.tool.debug_command(SifliUartCommand::Exit)?;
            common::receive_exit(&mut self.tool,&self.trace,"receiver_request")?;
        }
        self.check()?;
        self.checkpoint(json!({"intent":"soft-reset"}))?;
        require(self.tool.command(Command::SoftReset)?==Response::Ok,"Start response not OK")?;
        self.checkpoint(json!({"soft_reset_ack":true}))
    }
    fn listen(&mut self) -> Result<()> {
        self.tool.port.set_timeout(Duration::from_millis(100))?;
        let stop = Instant::now()+Duration::from_secs(self.cfg.listen_seconds);
        let mut out = fs::File::create_new(self.cfg.out.join("startup-uart.bin"))?;
        let mut buf = [0;4096];
        let mut received = 0usize;
        while Instant::now() < stop {
            self.check()?;
            match self.tool.port.read(&mut buf) {
                Ok(n) => {
                    received += n;
                    require(received <= 8*1024*1024, "Passive log exceeds bound")?;
                    out.write_all(&buf[..n])?;
                }
                Err(e) if e.kind()==io::ErrorKind::TimedOut => (),
                Err(e) => return Err(e.into()),
            }
            if self.cfg.out.join("finish-listening").exists() { break; }
        }
        out.sync_all()?;
        self.checkpoint(json!({"passive_uart_bytes":received}))
    }
    fn read_block(&mut self, name:&str, offset:usize, count:usize, retries:&mut usize) -> Result<Vec<u8>> {
        self.check()?;
        require(count>0 && count<=media::CHUNK && offset<=SIZE-count,"Out-of-bounds NOR block")?;
        let path=self.cfg.out.join(format!("{name}.bin"));
        require(!path.exists(),"Readback path reused")?;
        let request = ReadFlashParams{files:vec![ReadFlashFile{
            file_path:path.to_string_lossy().into_owned(),address:BASE+offset as u32,size:count as u32}]};
        let mut attempt = 0;
        loop {
            self.check()?;
            self.trace.event(json!({"kind":"read_intent","name":name,"offset":offset,"bytes":count,"attempt":attempt}))?;
            let mut raw = fs::File::open(self.cfg.out.join("rx.bin"))?;
            raw.seek(SeekFrom::End(0))?;
            let Err(error) = self.tool.read_flash(&request) else { break; };
            self.trace.event(json!({"kind":"read_error","name":name,"reason":error.to_string()}))?;
            self.check()?;
            let mut span = Vec::new();
            raw.take((count+2049) as u64).read_to_end(&mut span)?;
            let retry = attempt == 0 && *retries > 0 && !path.exists() && span.len() <= count+2048 &&
                        span.windows(13).any(|s| s==b"start_trans\r\n") && reads::ended_at_shell(&span)
                        && self.tool.port.bytes_to_read()? == 0;
            self.trace.event(json!({"kind":"read_failed","name":name,"offset":offset,"bytes":count,"retry":retry}))?;
            if !retry { return Err(error.into()); }
            // Only the same read may repeat, once, after the stub has finished
            // sending its CRC, OK and prompt. No programming request is retried.
            *retries -= 1;
            attempt += 1;
        }
        fs::OpenOptions::new().read(true).write(true).open(&path)?.sync_all()?;
        let data=fs::read(&path)?;
        require(data.len()==count,"Incomplete NOR read")?;
        self.trace.event(json!({"kind":"read_verified","name":name,"bytes":count,"sha256":hash(&data)}))?;
        Ok(data)
    }
}
impl Nor for Session<'_> {
    fn preserve_b(&mut self, bytes:&[u8])->Result<()> {
        let path=self.cfg.out.join("original-b-4MiB.bin");
        save(&path,bytes)?;
        require(fs::read(path)?==bytes,"Fresh B archive differs")
    }
    fn read(&mut self, name:&str, offset:usize, count:usize) -> Result<Vec<u8>> {
        if count != SIZE || offset != 0 {
            return self.read_block(name, offset, count, &mut 0);
        }
        let mut retries = 4;
        let image = reads::snapshot(name, |block, address| self.read_block(block, address, media::CHUNK, &mut retries))?;
        let path=self.cfg.out.join(format!("{name}.bin"));
        save(&path, &image)?;
        require(fs::read(path)? == image, "Assembled NOR snapshot was not archived intact")?;
        Ok(image)
    }
    fn write(&mut self, role:&str, offset:usize, bytes:&[u8]) -> Result<()> {
        self.check()?;
        require(media::writable(self.cfg.action,role,offset,bytes.len()),"Wire mutation exceeds window")?;
        self.trace.event(json!({"kind":"write_intent","role":role,"offset":offset,"bytes":bytes.len(),"sha256":hash(bytes)}))?;
        require(self.tool.command(Command::WriteAndErase{address:BASE+offset as u32,len:bytes.len() as u32})?==Response::RxWait,
                "Write/erase did not enter RX_WAIT")?;
        require(self.tool.send_data(bytes)?==Response::Ok,"Programming response not OK")?;
        self.trace.event(json!({"kind":"write_ack","role":role,"offset":offset,"bytes":bytes.len()}))?;
        Ok(())
    }
    fn checkpoint(&mut self, e:Value) -> Result<()> { checkpoint(&self.cfg.out,e) }
}
pub fn execute(c:&Config, stub:&[u8], images:&Inputs, rescue:Option<&[u8]>) -> Result<()> {
    let frozen=c.out.join("official-stub.bin");
    save(&frozen,stub)?;
    let trace=port::Trace::new(&c.out)?;
    let token=CancelToken::default();
    let opened=serialport::new(&c.port,1_000_000).dtr_on_open(false).timeout(Duration::from_secs(5)).open()?;
    let base=SifliToolBase::new_with_external_stub_and_cancel(c.port.clone(),BeforeOperation::NoReset,"nor".into(),
        1_000_000,1,false,sftool_lib::progress::no_op_progress_sink(),Some(frozen.to_string_lossy().into_owned()),token.clone());
    let mut session=Session{cfg:c,tool:SF32LB52Tool{base,port:Box::new(port::Recorded::new(opened,trace.clone()))},trace:trace.clone()};
    let alive=Arc::new(AtomicBool::new(true));
    let watch=alive.clone(); let cancel=c.cancel.clone(); let limit=Duration::from_secs(c.seconds);
    let monitor=thread::spawn(move || {
        let start=Instant::now();
        while watch.load(Ordering::Relaxed) {
            if cancel.exists() || start.elapsed()>=limit { token.cancel(); break; }
            thread::sleep(Duration::from_millis(50));
        }
    });
    let result=std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        session.enter()?;
        crate::transact(&mut session,c,images,rescue)?;
        session.start()?;
        session.listen()
    })).unwrap_or_else(|_|Err("Vendor library stopped; no further protocol".into()));
    alive.store(false,Ordering::Relaxed);
    let _=monitor.join();
    drop(session);
    let summary=trace.finish()?;
    checkpoint(&c.out,json!({"uart_closed":true,"wire":summary}))?;
    result
}
