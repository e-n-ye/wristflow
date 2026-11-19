//! Fixed storage contract for the original CycleLog board. Offsets are relative
//! to the 16 MiB NOR. The operator and the memory simulator use this same path.
use crate::{hash, require, Result};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::ops::Range;

pub const SIZE: usize = 0x1000000;
pub const BASE: u32 = 0x12000000;
pub const PAGE: usize = 4096;
pub const CHUNK: usize = 65536;
pub const BOOT: Range<usize> = 0x10000..0x1f000;
pub const RX: Range<usize> = 0x820000..0x878000;
pub const META: Range<usize> = 0xda0000..0xda4000;
const A: Range<usize> = 0x20000..0x420000;
const B: Range<usize> = 0x420000..0x820000;
const B_BODY_END: usize = 0x420000 + 3035136;
const B_TAIL: usize = 0x820000 - PAGE;
const A_SHA: &str = "6ebed703bad44f8715b75ec6aecf59f00ba13ce2076464b8667300c95cb0345d";
const B_SHA: &str = "4205c2f63f4417b614d34edf9422e9f3adb3d9899cd5d9db379c7fce3ccda0a0";
pub const FTAB_SHA: &str = "1b1bfcd48a69bcef2da5fd314f2aef0a2fd5953934c504c75c3fce83dab8d905";
const BOOT_ORIGINAL_SHA: &str = "f59f948f987362f1270b1da541405e82bd887c1a16c306364c04913c9c07f667";

#[derive(Clone, Copy, Deserialize, Serialize, Debug, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum Action { Install, VerifyOriginal, EnterReceiver, InspectReceiver, Restore, RestoreWithB, TraceOriginalA, RestoreTraceA }
impl Action {
    pub fn traced(self) -> bool { matches!(self, Self::TraceOriginalA | Self::RestoreTraceA) }
}

pub struct Inputs {
    pub receiver: Vec<u8>,
    pub journal: Vec<u8>,
    pub boot: Vec<u8>,
}
impl Inputs {
    pub fn verify(&self, action: Action) -> Result<()> {
        require(self.receiver.len() == RX.len() && self.boot.len() == BOOT.len(), "Wrong boot/receiver span")?;
        require(self.journal.len() == PAGE && hash(&self.journal) ==
            "0cec98cc451a579cc237715a1c259637a633b07dc9b9ad44cba901a6a3914c52", "Journal input changed")?;
        let boot = hash(&self.boot);
        let receiver = hash(&self.receiver);
        let corrected = boot == "20e2b126a64945a883e440cb966d41f607efd00fd9875eb35b9f4f533436004f" &&
                       receiver == "6c5865b8b114b9eae51dec7239760918f57fe097d5889a39906b63c082815603";
        let historical = matches!(action, Action::Restore | Action::RestoreWithB | Action::VerifyOriginal) &&
                         boot == "5f6e8aca810ae7397c988ca14728075b9715f9d801c195650f83350df5d20939" &&
                         receiver == "8c446afb99e98c644019beb157859cfdfa17596390cb232a83c31bc67d4e80eb";
        require(corrected || historical, "Unbound or superseded boot/receiver pair")
    }
}

pub trait Nor {
    fn preserve_b(&mut self, bytes: &[u8]) -> Result<()>;
    fn read(&mut self, name: &str, offset: usize, count: usize) -> Result<Vec<u8>>;
    fn write(&mut self, role: &str, offset: usize, data: &[u8]) -> Result<()>;
    fn checkpoint(&mut self, event: Value) -> Result<()>;
}

fn same_board(d: &[u8]) -> Result<()> {
    require(d.len() == SIZE && hash(&d[..0x10000]) == FTAB_SHA && hash(&d[A]) == A_SHA,
            "Wrong FTAB, original A or device size")
}
pub fn original(d: &[u8]) -> Result<()> {
    same_board(d)?;
    require(hash(&d[0x10000..0x20000]) == BOOT_ORIGINAL_SHA && hash(&d[B]) == B_SHA &&
            d[0x820000..0x880000].iter().all(|&v| v == 255) && d[META].iter().all(|&v| v == 255),
            "Original boot, B or reserved ranges differ")
}

fn initial_stable(record: &[u8], initial: &[u8]) -> bool {
    let other = &record[56..96];
    let other_valid = other.iter().all(|&b| b == 0) ||
        (other[..4].iter().any(|&b| b != 0) && other[8..].iter().any(|&b| b != 0));
    other_valid && record[..8] == initial[..8] && record[12..16] == initial[12..16] &&
        record[16..56] == initial[16..56] && record[96..124].iter().all(|&v| v == 0)
}
fn generation(record: &[u8]) -> Option<u32> {
    let number = u32::from_le_bytes(record[8..12].try_into().unwrap());
    let crc = u32::from_le_bytes(record[124..128].try_into().unwrap());
    (number > 0 && &record[..4] == b"WFAB" && crc::Crc::<u32>::new(&crc::CRC_32_ISO_HDLC).checksum(&record[..124]) == crc)
        .then_some(number)
}
fn permit_entry(d: &[u8], images: &Inputs) -> Result<()> {
    require(d[BOOT] == images.boot && d[RX] == images.receiver, "Installed boot/receiver differs")?;
    let a = &d[META.start..META.start+128];
    let b = &d[META.start+8192..META.start+8192+128];
    let selected = match (generation(a), generation(b)) {
        (Some(x), Some(y)) if x == y && a != b => return Err("Conflicting journal".into()),
        (Some(x), Some(y)) if y > x => b,
        (Some(_), _) => a,
        (_, Some(_)) => b,
        _ => return Err("No intact journal".into()),
    };
    require(initial_stable(selected, &images.journal), "Receiver re-entry requires stable original A")
}

pub fn writable(action: Action, role: &str, offset: usize, n: usize) -> bool {
    if action.traced() {
        return matches!((role, offset, n), ("boot", 0x10000, 61440) |
            ("certificate", 0x86c000, 4096) | ("journal", 0xda0000, 4096));
    }
    let range = match (action, role) {
        (Action::Install | Action::Restore | Action::RestoreWithB, "boot") => BOOT,
        (Action::Install | Action::Restore | Action::RestoreWithB, "receiver") => RX,
        (Action::Install, "journal") => META.start..META.start+PAGE,
        (Action::Restore | Action::RestoreWithB, "journal") => META,
        (Action::RestoreWithB, "b-body") => B.start..B_BODY_END,
        (Action::RestoreWithB, "b-tail") => B_TAIL..B.end,
        _ => return false,
    };
    offset % PAGE == 0 && n > 0 && n <= CHUNK && n % PAGE == 0 &&
        offset >= range.start && offset.checked_add(n).is_some_and(|end| end <= range.end)
}

fn replace(io: &mut impl Nor, action: Action, expected: &mut [u8], role: &str,
           start: usize, target: &[u8]) -> Result<()> {
    for (i, chunk) in target.chunks(CHUNK).enumerate() {
        let off = start+i*CHUNK;
        require(writable(action, role, off, chunk.len()), "Write violates region contract")?;
        let dst = &mut expected[off..off + chunk.len()];
        if dst == chunk { continue; }
        io.checkpoint(json!({"intent":role,"offset":off,"bytes":chunk.len(),"sha256":hash(chunk)}))?;
        io.write(role, off, chunk)?;
        let back = io.read(&format!("{role}-{off:08x}"), off, chunk.len())?;
        require(back == chunk, "Independent chunk readback differs; stop before next write")?;
        dst.copy_from_slice(chunk);
    }
    Ok(())
}

pub fn run(io: &mut impl Nor, action: Action, images: &Inputs, rescue: Option<&[u8]>) -> Result<()> {
    require(!action.traced(), "Use the bounded original-A transaction")?;
    images.verify(action)?;
    if action == Action::InspectReceiver {
        let installed = rescue.ok_or("The verified first-install readback is required")?;
        require(hash(installed) == "d862c6aab38f33490519c70be2583fc56cc5d2bc9c465543efe6aa41ab8d2258",
                "Wrong provisioned-board reference")?;
        same_board(installed)?;
        permit_entry(installed, images)?;
        let ranges = [0..CHUNK, BOOT, A.start..A.start+PAGE, RX, META.start..META.start+PAGE,
                      META.start+8192..META.start+8192+PAGE];
        let mut read_bytes = 0;
        for range in ranges {
            for (i, part) in installed[range.clone()].chunks(CHUNK).enumerate() {
                let off = range.start + i * CHUNK;
                let live = io.read(&format!("key-{off:08x}"), off, part.len())?;
                require(live == part, "Critical installed range differs; suppress receiver request")?;
                read_bytes += live.len();
            }
        }
        return io.checkpoint(json!({"entry_ranges_verified":true,"read_bytes":read_bytes,"flash_writes":0}));
    }
    let mut expected = io.read("before", 0, SIZE)?;
    same_board(&expected)?;
    let before_hash = hash(&expected);
    match action {
        Action::TraceOriginalA | Action::RestoreTraceA | Action::InspectReceiver => return Err("Unexpected storage action".into()),
        Action::Install | Action::VerifyOriginal => {
            original(&expected)?;
            io.preserve_b(&expected[B])?;
            io.checkpoint(json!({"original_verified":true,"before_sha256":before_hash,"a":A_SHA,"b":B_SHA}))?;
            if action == Action::VerifyOriginal {
                return io.checkpoint(json!({"checked_original_only":true,"flash_writes":0}));
            }
            replace(io, action, &mut expected, "receiver", RX.start, &images.receiver)?;
            replace(io, action, &mut expected, "journal", META.start, &images.journal)?;
            replace(io, action, &mut expected, "boot", BOOT.start, &images.boot)?;
        }
        Action::EnterReceiver => {
            permit_entry(&expected, images)?;
            return io.checkpoint(json!({"stable_a_verified":true,"before_sha256":before_hash,"flash_writes":0}));
        }
        Action::Restore | Action::RestoreWithB => {
            let source = rescue.ok_or("A verified original installation snapshot is required")?;
            original(source)?;
            require(expected[BOOT.end..A.start] == source[BOOT.end..A.start] &&
                    expected[RX.end..0x880000] == source[RX.end..0x880000] &&
                    expected[B_BODY_END..B_TAIL] == source[B_BODY_END..B_TAIL], "Unowned firmware bytes changed")?;
            if action == Action::Restore {
                require(expected[B] == source[B], "B changed; restoration outside initial install not authorized")?;
            }
            // Restore the original boot first. User partitions in expected are
            // always the fresh recovery read, including records since install.
            replace(io, action, &mut expected, "boot", BOOT.start, &source[BOOT])?;
            replace(io, action, &mut expected, "receiver", RX.start, &source[RX])?;
            replace(io, action, &mut expected, "journal", META.start, &source[META])?;
            if action == Action::RestoreWithB {
                replace(io, action, &mut expected, "b-body", B.start, &source[B.start..B_BODY_END])?;
                replace(io, action, &mut expected, "b-tail", B_TAIL, &source[B_TAIL..B.end])?;
            }
        }
    }
    let after = io.read("after", 0, SIZE)?;
    require(after == expected, "Full-chip readback differs from exact expected image; suppress reset")?;
    if action == Action::Install { permit_entry(&after, images)?; } else { original(&after)?; }
    io.checkpoint(json!({"entire_nor_verified":true,"after_sha256":hash(&after),"a":hash(&after[A]),"b":hash(&after[B])}))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn restricted_mutation_surface() {
        for action in [Action::Install, Action::VerifyOriginal, Action::EnterReceiver, Action::Restore, Action::RestoreWithB] {
            for role in ["boot","journal","receiver","b-body","b-tail","a","fs"] {
                assert!(!writable(action,role,A.start,PAGE));
                assert!(!writable(action,role,0x9a0000,PAGE));
                assert!(!writable(action,role,META.end,PAGE));
                assert!(!writable(action,role,usize::MAX,CHUNK));
            }
        }
        assert!(writable(Action::Install,"journal",META.start,PAGE));
        assert!(!writable(Action::Install,"journal",META.start+PAGE,PAGE));
        assert!(!writable(Action::Install,"boot",BOOT.start,CHUNK));
        assert!(!writable(Action::Restore,"b-body",B.start,PAGE));
        for (role,range) in [("receiver", RX), ("boot", BOOT), ("journal", META)] {
            assert!(!writable(Action::VerifyOriginal,role,range.start,PAGE));
        }
        assert!(writable(Action::RestoreWithB,"b-tail",B_TAIL,PAGE));
        assert!(!writable(Action::RestoreWithB,"b-body",B_BODY_END,PAGE));
    }
    struct Fault {calls:Vec<&'static str>, fail:&'static str}
    impl Nor for Fault {
        fn preserve_b(&mut self,_:&[u8])->Result<()> {unreachable!()}
        fn write(&mut self,_:&str,_:usize,_:&[u8])->Result<()> {
            self.calls.push("write");require(self.fail!="write","write failed")
        }
        fn read(&mut self,_:&str,_:usize,count:usize)->Result<Vec<u8>> {
            self.calls.push("read");Ok(vec![if self.fail=="read" {0} else {1};count])
        }
        fn checkpoint(&mut self,_:Value)->Result<()> {
            self.calls.push("intent");require(self.fail!="intent","intent not durable")
        }
    }
    #[test]
    fn no_next_chunk_after_ambiguous_io_or_missing_intent() {
        for (stage, calls) in [("intent",1),("write",2),("read",3)] {
            let mut io=Fault{calls:vec![],fail:stage};
            let mut image=vec![255;RX.end];
            assert!(replace(&mut io,Action::Install,&mut image,"receiver",RX.start,&vec![1;2*CHUNK]).is_err());
            assert_eq!(io.calls.len(),calls);
            assert!(image[RX].iter().all(|&b|b==255));
        }
    }
}
