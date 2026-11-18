//! Fixed original-board provisioning and independent recovery. No arbitrary addresses.
#[path = "../first_media.rs"]
mod media;
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
}
impl Config {
    fn validate(&self) -> Result<()> {
        require(!self.out.exists() && !self.cancel.exists() && (30..=1800).contains(&self.seconds) &&
                self.listen_seconds <= 900 && self.listen_seconds < self.seconds, "Invalid window or reused output")?;
        let restore = matches!(self.action, Action::Restore | Action::RestoreWithB);
        require(self.before.is_some() == restore && self.before_sha256.is_some() == restore, "Prior original is required only for recovery")?;
        let scope = match self.action {
            Action::Install => "first-install-receiver-journal-boot-and-enter",
            Action::VerifyOriginal => "verify-original-nor-and-start-original",
            Action::EnterReceiver => "enter-pinned-receiver-without-flash-write",
            Action::Restore => "restore-first-install-and-start-original",
            Action::RestoreWithB => "restore-first-install-and-owned-b-extents-and-start-original",
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
fn apply(c: &Config) -> Result<()> {
    c.validate()?;
    let images = Inputs { receiver: fs::read(c.bundle.join("receiver.bin"))?,
        journal: fs::read(c.bundle.join("journal.bin"))?, boot: fs::read(c.bundle.join("boot.bin"))? };
    images.verify()?;
    let source = c.before.as_ref().map(fs::read).transpose()?;
    if let Some(d) = &source {
        require(Some(hash(d)) == c.before_sha256, "Previous window original changed")?;
        media::original(d)?;
    }
    let stub = fs::read(&c.stub)?;
    require(stub.len() == 132816 && hash(&stub) == "d4db9c27a0539da431fd253ee4b09c8d3ddb033bf0dc1ae4cc3bf322fa069e43",
            "Pinned official RAM stub required")?;
    fs::create_dir(&c.out)?;
    let result = if c.simulation {
        let mut mem = Memory {bytes:fs::read(c.fixture.as_ref().unwrap())?,out:&c.out,action:c.action};
        media::run(&mut mem,c.action,&images,source.as_deref())
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
