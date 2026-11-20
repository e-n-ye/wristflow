//! Fixed original-board provisioning and independent recovery. No arbitrary addresses.
#[path = "../first_media.rs"]
mod media;
#[path = "../trace_media.rs"]
mod trace_media;
#[path = "../first_reads.rs"]
mod reads;
#[path = "../port.rs"]
#[allow(dead_code)]
mod port;
#[path = "../common.rs"]
#[allow(dead_code)]
mod common;
#[cfg(test)]
#[path = "../simulated.rs"]
#[allow(dead_code)]
mod simulated;
#[cfg(not(test))]
#[path = "../sector.rs"]
#[allow(dead_code)]
mod sector;
#[cfg(not(test))]
#[path = "../first_wire.rs"]
mod wire;
use media::{Action, Inputs, Nor};
use serde::Deserialize;
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{fs,io::Write,path::{Path,PathBuf}};
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
fn require(ok: bool, why: &str) -> Result<()> {
    if ok { Ok(()) } else { Err(std::io::Error::other(why).into()) }
}
fn hash(bytes: &[u8]) -> String { format!("{:x}", Sha256::digest(bytes)) }
fn save(path: &Path, data: &[u8]) -> Result<()> {
    let mut f = fs::File::create_new(path)?;
    f.write_all(data)?;
    f.sync_all()?;
    Ok(())
}
fn checkpoint(out: &Path, event: Value) -> Result<()> {
    let mut f = fs::OpenOptions::new().append(true).create(true).open(out.join("media-events.jsonl"))?;
    writeln!(f, "{event}")?;
    f.sync_all()?;
    Ok(())
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Config {
    action: Action, simulation: bool, port: String, out: PathBuf, bundle: PathBuf,
    before: Option<PathBuf>, before_sha256: Option<String>, fixture: Option<PathBuf>,
    cancel: PathBuf, stub: PathBuf, seconds: u64, listen_seconds: u64, authorization: String,
    #[serde(default)]
    signature_then_body: bool,
    trace_boot: Option<PathBuf>,
    display_pair_sha256: Option<[String; 2]>,
}
impl Config {
    fn validate(&self) -> Result<()> {
        let (max_window, max_listen) = if self.signature_then_body {
            require(self.action == Action::EnterIdentity, "Paired phone window requires read-only entry")?;
            (1740, 1620)
        } else if self.action.display_pair() {
            (180, 35)
        } else if self.action.identity() {
            (480, 320)
        } else {
            (1800, 900)
        };
        require(!self.out.exists() && !self.cancel.exists() && (30..=max_window).contains(&self.seconds) &&
                self.listen_seconds <= max_listen && self.listen_seconds < self.seconds, "Invalid window or reused output")?;
        let restore = matches!(self.action, Action::Restore | Action::RestoreWithB | Action::RestoreTrialB |
                              Action::RestoreHandoffB | Action::RestoreTraceA);
        if self.action.traced() {
            require(self.seconds <= 90 && self.listen_seconds <= 35,"Trace time limit exceeded")?;
        }
        require(self.trace_boot.is_some() == (self.action.short_entry() || self.action == Action::TraceTrialB), "Unexpected boot image argument")?;
        let health_model = self.action.health_model();
        let has_reference = restore || self.action.key_regions() || self.action.trial_read() || self.action.handoff() || self.action.journal_dma() || health_model.is_some() || self.action.display_pair();
        require(self.before.is_some() == has_reference && self.before_sha256.is_some() == has_reference, "Missing or unexpected NOR reference")?;
        if self.action == Action::InspectReceiver {
            require(self.seconds <= 240 && self.listen_seconds <= 160, "Receiver discovery time limit exceeded")?;
        }
        if self.action.key_regions() || self.action.trial_read() || self.action == Action::TraceTrialB {
            require(self.before_sha256.as_deref() == Some(media::INSTALLED_SHA), "Wrong installed reference binding")?;
        }
        if self.action.journal_dma() {
            require(self.before_sha256.as_deref() == Some(media::RESTORED_SHA) && self.seconds <= 800 &&
                    self.listen_seconds <= 420, "Wrong journal DMA reference or time limit")?;
        }
        if health_model.is_some() {
            require(self.before_sha256.as_deref() == health_model && self.seconds <= 800 &&
                    self.listen_seconds <= 420, "Wrong 06 model or time limit")?;
        }
        require(self.display_pair_sha256.is_some() == self.action.display_pair(), "Display-pair digest binding missing or unexpected")?;
        if self.action.display_pair() {
            require(self.before_sha256.as_deref() == self.action.display_model(), "Wrong stable display reference")?;
        }
        let scope = match self.action {
            Action::Install => "first-install-receiver-journal-boot-and-enter",
            Action::VerifyOriginal => "verify-original-nor-and-start-original",
            Action::EnterReceiver => "enter-pinned-receiver-without-flash-write",
            Action::InspectReceiver => "read-installed-key-regions-enter-for-gatt-and-return",
            Action::InstallIdentity => "replace-installed-receiver-and-boot-enter-identity",
            Action::EnterIdentity => "enter-installed-identity-receiver-read-only",
            Action::RestoreIdentity => "restore-provisioned-pair-after-identity-window",
            Action::Restore => "restore-first-install-and-start-original",
            Action::RestoreWithB => "restore-first-install-and-owned-b-extents-and-start-original",
            Action::RestoreTrialB => "restore-05-b-and-stable-a-keep-installed-identity-pair",
            Action::EnterTrialB => "enter-for-android-05-trial-and-passive-return",
            Action::InspectTrialB => "read-05-after-autonomous-return-and-start-stable-a",
            Action::TraceTrialB => "temporary-handoff-boot-and-android-05",
            Action::RestoreHandoffB => "restore-pretrace-boot-stable-a-and-original-b",
            Action::TraceJournalDma => "trace-05-journal-dma-with-key-readback",
            Action::RestoreJournalDma => "restore-05-journal-dma-key-regions",
            Action::TestFixedRollback => "install-corrected-boot-and-try-05-once",
            Action::InspectFixedRollback => "inspect-corrected-05-return-keys-and-restart-a",
            Action::EnterHealthB => "verify-post-05-keys-and-enter-for-manual-06",
            Action::InspectHealthB => "verify-confirmed-06-keys-and-ordinary-restart",
            Action::RestoreHealthB => "restore-post-05-journal-and-b-pages-restart-a",
            Action::InstallDisplayPair => "install-512k-display-pair-restart-stable-b",
            Action::RestoreDisplayPair => "restore-original-display-pair-restart-stable-b",
            Action::InstallDisplayPairA => "install-512k-display-pair-restart-stable-a",
            Action::RestoreDisplayPairA => "restore-original-display-pair-restart-stable-a",
            Action::TraceOriginalA => "ordinary-a-three-range-trace",
            Action::RestoreTraceA => "restore-three-trace-ranges",
        };
        if self.simulation {
            require(self.fixture.is_some() && self.port == "SIMULATED" && self.authorization == "simulation", "Simulation cannot address a serial port")
        } else {
            require(cfg!(windows) && self.fixture.is_none() && self.port == "COM5" && self.authorization == scope,
                    "Original COM5 and matching window authorization required")
        }
    }
}
struct Memory<'a> { bytes: Vec<u8>, out: &'a Path, action: Action }
impl Nor for Memory<'_> {
    fn preserve_b(&mut self, bytes:&[u8])->Result<()> { save(&self.out.join("original-b-4MiB.bin"),bytes) }
    fn read(&mut self, name: &str, off: usize, count: usize) -> Result<Vec<u8>> {
        let bytes = if count == media::SIZE && off == 0 {
            reads::snapshot(name, |_, offset| Ok(self.bytes.get(offset..offset+media::CHUNK)
                .ok_or("Incomplete simulated NOR block")?.to_vec()))?
        } else {
            self.bytes.get(off..off+count).ok_or("Incomplete simulated NOR")?.to_vec()
        };
        save(&self.out.join(format!("{name}.bin")), &bytes)?;
        Ok(bytes)
    }
    fn write(&mut self, role: &str, off: usize, data: &[u8]) -> Result<()> {
        require(media::writable(self.action, role, off, data.len()), "Simulation write outside scope")?;
        self.bytes[off..off+data.len()].copy_from_slice(data);
        Ok(())
    }
    fn checkpoint(&mut self, e: Value) -> Result<()> { checkpoint(self.out,e) }
}
fn transact(io: &mut impl Nor,c: &Config, images: &Inputs, source: Option<&[u8]>) -> Result<()> {
    if c.action.traced() { trace_media::run(io,c.action,images,source,&c.out) }
    else { media::run(io,c.action,images,source) }
}
fn apply(c: &Config) -> Result<()> {
    c.validate()?;
    let receiver = if c.action.traced() { "certificate.bin" } else { "receiver.bin" };
    let trace_boot = if let Some(path) = &c.trace_boot {
        let mut bytes = fs::read(path)?;
        let size = match c.action {
            Action::TraceJournalDma => 58272,
            Action::TestFixedRollback => 57508,
            _ => 58264,
        };
        require(bytes.len() == size, "Wrong diagnostic BIN length")?;
        bytes.resize(media::BOOT.len(), 255);
        Some(bytes)
    } else { None };
    let images = Inputs { receiver: fs::read(c.bundle.join(receiver))?,
        journal: if c.action.display_pair() { Vec::new() } else { fs::read(c.bundle.join("journal.bin"))? },
        boot: fs::read(c.bundle.join("boot.bin"))?, trace_boot };
    if let Some(expected) = &c.display_pair_sha256 {
        require(*expected == [hash(&images.boot), hash(&images.receiver)], "Display-pair digest mismatch")?;
    }
    if c.action.traced() { trace_media::verify_inputs(&images)?; } else { images.verify(c.action)?; }
    let source = c.before.as_ref().map(fs::read).transpose()?;
    if let Some(d) = &source {
        require(Some(hash(d)) == c.before_sha256, "Previous window original changed")?;
        if c.action.traced() { require(hash(d)==trace_media::ORIGINALS_SHA,"Invalid compact originals")?; }
        else if !c.action.key_regions() && !c.action.trial_read() && !c.action.handoff() && !c.action.journal_dma() && c.action.health_model().is_none() && !c.action.display_pair() { media::original(d)?; }
    }
    let stub = fs::read(&c.stub)?;
    require(stub.len() == 132816 && hash(&stub) == "d4db9c27a0539da431fd253ee4b09c8d3ddb033bf0dc1ae4cc3bf322fa069e43",
            "Pinned official RAM stub required")?;
    fs::create_dir(&c.out)?;
    let result = if c.simulation {
        let mut mem = Memory {bytes:fs::read(c.fixture.as_ref().unwrap())?,out:&c.out,action:c.action};
        transact(&mut mem,c,&images,source.as_deref()).and_then(|()| {
            if c.action.traced() || c.action.identity() || c.action == Action::TraceTrialB || c.action.journal_dma() || c.action.display_pair() { save(&c.out.join("simulated-nor.bin"),&mem.bytes)?; }
            Ok(())
        })
    } else {
        #[cfg(not(test))]
        { wire::execute(c,&stub,&images,source.as_deref()) }
        #[cfg(test)]
        { Err("Test binary cannot open a port".into()) }
    };
    checkpoint(&c.out, json!({"terminal":true,"ok":result.is_ok(),"error":result.as_ref().err().map(|e|e.to_string()),
                               "simulation":c.simulation,"action":c.action}))?;
    result
}
fn main() {
    let args: Vec<_> = std::env::args_os().skip(1).collect();
    let result = (|| -> Result<()> {
        require(args.len()==3 && args[0]=="--config","Explicit config and digest required")?;
        let raw=fs::read(&args[1])?;
        require(raw.len()<=32768 && args[2].to_str()==Some(hash(&raw).as_str()),"Config digest mismatch")?;
        apply(&serde_json::from_slice(&raw)?)
    })();
    if let Err(e)=result { eprintln!("{e}"); std::process::exit(1); }
}
