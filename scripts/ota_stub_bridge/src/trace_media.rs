//! Ordinary A boot with its signed identity and journal in their production locations.
use crate::{hash, require, save, Result};
use crate::media::{Action, Inputs, Nor, BOOT, CHUNK, FTAB_SHA, META, PAGE};
use serde_json::json;
use std::{fs, path::Path};

pub const CERT: usize = 0x86c000;
pub const ORIGINALS_SHA: &str = "bd6573264067ad33693ed21f2432d3607641709fadb66d8c897784a267b6ed01";
const A_VECTOR_SHA: &str = "82bfd68325cb7a98b1add0f1dd627269d38b61308927e1447c2ab1a903d27015";

pub fn verify_inputs(images: &Inputs) -> Result<()> {
    let boot_hash = hash(&images.boot);
    require(images.boot.len() == BOOT.len() && matches!(boot_hash.as_str(),
        "dac5728228e1a53b929c292d13ab4120bb92bdf48e4988e33188bf220b17df27" |
        "7143e8d49904b86c7fb952faf27ee78c9b6e0e4f4571229ea1658e198ab0b41a"), "Traced boot differs")?;
    require(images.receiver.len() == PAGE && hash(&images.receiver) ==
        "f1236ac70baaa7a327d6872d62e1c1aae6e2c1f810359f1ac6fe9abb05691d76", "Certificate sector differs")?;
    require(images.journal.len() == PAGE && hash(&images.journal) ==
        "0cec98cc451a579cc237715a1c259637a633b07dc9b9ad44cba901a6a3914c52", "Stable A record differs")
}

fn program(io: &mut impl Nor, action: Action, role: &str, offset: usize, current: &[u8], data: &[u8]) -> Result<()> {
    require(current.len() >= data.len() && crate::media::writable(action,role,offset,data.len()), "Trace range differs")?;
    if current[..data.len()] == *data { return Ok(()); }
    io.checkpoint(json!({"intent":role,"offset":offset,"bytes":data.len(),"sha256":hash(data)}))?;
    io.write(role,offset,data)?;
    let back=io.read(&format!("verified-{role}"),offset,current.len())?;
    require(back.len()==current.len() && back[..data.len()]==*data && back[data.len()..]==current[data.len()..],
            "Target or its protected tail differs; stop before the next write")
}

pub fn run(io: &mut impl Nor, action: Action, images: &Inputs, original: Option<&[u8]>, out: &Path) -> Result<()> {
    verify_inputs(images)?;
    require(hash(&io.read("ftab",0,CHUNK)?)==FTAB_SHA, "Wrong FTAB")?;
    require(hash(&io.read("a-vectors",0x20000,PAGE)?)==A_VECTOR_SHA, "Wrong original A vector page")?;
    let boot=io.read("boot-before",BOOT.start,CHUNK)?;
    let cert=io.read("certificate-before",CERT,PAGE)?;
    let meta=io.read("journal-before",META.start,META.len())?;
    let current=[boot.as_slice(),cert.as_slice(),meta.as_slice()].concat();
    match action {
        Action::TraceOriginalA => {
            require(original.is_none() && hash(&current)==ORIGINALS_SHA, "Original boot or reserved sectors differ")?;
            let archive=out.join("originals.bin");
            save(&archive,&current)?;
            require(fs::read(archive)?==current,"Same-window originals archive differs")?;
            io.checkpoint(json!({"trace_originals_archived":true,"sha256":ORIGINALS_SHA}))?;
            program(io,action,"certificate",CERT,&cert,&images.receiver)?;
            program(io,action,"journal",META.start,&meta,&images.journal)?;
            program(io,action,"boot",BOOT.start,&boot,&images.boot)?;
        }
        Action::RestoreTraceA => {
            let src=original.ok_or("Restore needs the same-window originals")?;
            require(hash(src)==ORIGINALS_SHA, "Recovery originals changed")?;
            require(boot.len()==CHUNK && cert.len()==PAGE && meta.len()==META.len() &&
                    boot[BOOT.len()..]==src[BOOT.len()..CHUNK] && meta[PAGE..]==src[CHUNK+2*PAGE..],
                    "Unexpected change beyond a trace write range")?;
            program(io,action,"boot",BOOT.start,&boot,&src[..BOOT.len()])?;
            program(io,action,"certificate",CERT,&cert,&src[CHUNK..CHUNK+PAGE])?;
            program(io,action,"journal",META.start,&meta,&src[CHUNK+PAGE..CHUNK+2*PAGE])?;
        }
        _ => return Err("Unrecognized trace transaction".into())
    }
    io.checkpoint(json!({"three_ranges_verified":true,"action":action}))
}
