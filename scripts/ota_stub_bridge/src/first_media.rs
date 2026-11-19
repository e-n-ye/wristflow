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
const IDENTITY_RX: Range<usize> = 0x820000..0x879000;
pub const INSTALLED_SHA: &str = "d862c6aab38f33490519c70be2583fc56cc5d2bc9c465543efe6aa41ab8d2258";
pub const META: Range<usize> = 0xda0000..0xda4000;
const A: Range<usize> = 0x20000..0x420000;
const B: Range<usize> = 0x420000..0x820000;
const B_BODY_END: usize = 0x420000 + 3035136;
const ROLLBACK_BODY_END: usize = B.start + 0x2e4000;
const B_TAIL: usize = 0x820000 - PAGE;
const A_SHA: &str = "6ebed703bad44f8715b75ec6aecf59f00ba13ce2076464b8667300c95cb0345d";
const B_SHA: &str = "4205c2f63f4417b614d34edf9422e9f3adb3d9899cd5d9db379c7fce3ccda0a0";
pub const FTAB_SHA: &str = "1b1bfcd48a69bcef2da5fd314f2aef0a2fd5953934c504c75c3fce83dab8d905";
const BOOT_ORIGINAL_SHA: &str = "f59f948f987362f1270b1da541405e82bd887c1a16c306364c04913c9c07f667";

#[derive(Clone, Copy, Deserialize, Serialize, Debug, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum Action { Install, VerifyOriginal, EnterReceiver, InspectReceiver, InstallIdentity, EnterIdentity, RestoreIdentity,
                  Restore, RestoreWithB, RestoreTrialB, EnterTrialB, InspectTrialB, TraceTrialB, RestoreHandoffB,
                  TraceOriginalA, RestoreTraceA }
impl Action {
    pub fn traced(self) -> bool { matches!(self, Self::TraceOriginalA | Self::RestoreTraceA) }
    pub fn identity(self) -> bool { matches!(self, Self::InstallIdentity | Self::EnterIdentity | Self::RestoreIdentity) }
    pub fn key_regions(self) -> bool { self.identity() || self == Self::InspectReceiver }
    pub fn trial_read(self) -> bool { matches!(self, Self::EnterTrialB | Self::InspectTrialB) }
    pub fn handoff(self) -> bool { matches!(self, Self::TraceTrialB | Self::RestoreHandoffB) }
}

pub struct Inputs {
    pub receiver: Vec<u8>,
    pub journal: Vec<u8>,
    pub boot: Vec<u8>,
    pub trace_boot: Option<Vec<u8>>,
}
impl Inputs {
    pub fn verify(&self, action: Action) -> Result<()> {
        require((action == Action::TraceTrialB) == self.trace_boot.is_some(), "Unexpected or missing trace image")?;
        if let Some(bytes) = &self.trace_boot {
            require(bytes.len() == BOOT.len() && hash(bytes) ==
                "340d5aab6097e892216b0129a7d377cd3ffb748ba5bdc736f175e9c7cdfd8ed8", "Wrong padded handoff boot")?;
        }
        let identity_images = action.identity() || action.trial_read() || action.handoff() || action == Action::RestoreTrialB;
        let span = if identity_images { IDENTITY_RX } else { RX };
        require(self.receiver.len() == span.len() && self.boot.len() == BOOT.len(), "Wrong boot/receiver span")?;
        require(self.journal.len() == PAGE && hash(&self.journal) ==
            "0cec98cc451a579cc237715a1c259637a633b07dc9b9ad44cba901a6a3914c52", "Journal input changed")?;
        let boot = hash(&self.boot);
        let receiver = hash(&self.receiver);
        if identity_images {
            return require(boot == "a7452af386289216c3357d61e329cf83e709cd76c2866c2940a4dcfbecbb520c" &&
                           receiver == "7f52f7482b5e4ae2fe81bf354ede5ad58c71c4a47fca8e94057071f29af755d4",
                           "Unbound independent-address pair");
        }
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
        (Action::InstallIdentity | Action::RestoreIdentity | Action::TraceTrialB | Action::RestoreHandoffB, "boot") => BOOT,
        (Action::InstallIdentity | Action::RestoreIdentity, "receiver") => IDENTITY_RX,
        (Action::Install | Action::Restore | Action::RestoreWithB, "boot") => BOOT,
        (Action::Install | Action::Restore | Action::RestoreWithB, "receiver") => RX,
        (Action::Install, "journal") => META.start..META.start+PAGE,
        (Action::Restore | Action::RestoreWithB, "journal") => META,
        (Action::RestoreWithB, "b-body") => B.start..B_BODY_END,
        (Action::RestoreTrialB | Action::RestoreHandoffB, "b-body") => B.start..ROLLBACK_BODY_END,
        (Action::RestoreTrialB | Action::RestoreHandoffB, "stable-a") => META.start..META.start+8192,
        (Action::RestoreTrialB | Action::RestoreHandoffB, "other-record") => META.start+8192..META.end,
        (Action::RestoreWithB | Action::RestoreTrialB | Action::RestoreHandoffB, "b-tail") => B_TAIL..B.end,
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

fn key_transaction(io: &mut impl Nor, action: Action, images: &Inputs, installed: &[u8]) -> Result<()> {
    require(hash(installed) == INSTALLED_SHA, "Wrong provisioned-board reference")?;
    same_board(installed)?;
    let rx = if action.identity() { IDENTITY_RX } else { RX };
    let mut snapshot = installed.to_vec();
    if action == Action::EnterIdentity {
        snapshot[BOOT].copy_from_slice(&images.boot);
        snapshot[rx.clone()].copy_from_slice(&images.receiver);
    }
    let ranges = [0..CHUNK, BOOT, A.start..A.start+PAGE, rx.clone(), META.start..META.start+PAGE,
                  META.start+8192..META.start+8192+PAGE];
    let mut bytes = 0;
    for range in ranges {
        // Recovery can follow an interrupted program in either of these two extents.
        let recoverable = action == Action::RestoreIdentity && (range == BOOT || range == rx);
        for off in (range.start..range.end).step_by(CHUNK) {
            let end = (off + CHUNK).min(range.end);
            let live = io.read(&format!("key-{off:08x}"), off, end-off)?;
            require(live.len() == end-off && (recoverable || live == snapshot[off..end]),
                    "Critical installed range differs; stop before mutation or reset")?;
            snapshot[off..end].copy_from_slice(&live);
            bytes += live.len();
        }
    }
    if matches!(action, Action::InspectReceiver | Action::EnterIdentity) {
        return io.checkpoint(json!({"entry_ranges_verified":true,"read_bytes":bytes,"flash_writes":0}));
    }
    io.checkpoint(json!({"identity_keys_archived":true,"read_bytes":bytes}))?;
    if action == Action::RestoreIdentity {
        replace(io, action, &mut snapshot, "boot", BOOT.start, &installed[BOOT])?;
        replace(io, action, &mut snapshot, "receiver", rx.start, &installed[rx.clone()])?;
    } else {
        require(action == Action::InstallIdentity, "Unexpected receiver programming action")?;
        replace(io, action, &mut snapshot, "receiver", rx.start, &images.receiver)?;
        replace(io, action, &mut snapshot, "boot", BOOT.start, &images.boot)?;
    }
    for range in [BOOT, rx] {
        for off in (range.start..range.end).step_by(CHUNK) {
            let end = (off+CHUNK).min(range.end);
            require(io.read(&format!("pair-{off:08x}"), off, end-off)? == snapshot[off..end],
                    "Programmed boot/receiver pair differs; suppress reset")?;
        }
    }
    io.checkpoint(json!({"identity_pair_verified":true,"restored":action==Action::RestoreIdentity}))
}

pub fn run(io: &mut impl Nor, action: Action, images: &Inputs, rescue: Option<&[u8]>) -> Result<()> {
    require(!action.traced(), "Use the bounded original-A transaction")?;
    images.verify(action)?;
    if action.key_regions() {
        return key_transaction(io, action, images, rescue.ok_or("Missing installed reference")?);
    }
    let mut expected = io.read("before", 0, SIZE)?;
    same_board(&expected)?;
    let before_hash = hash(&expected);
    match action {
        Action::TraceOriginalA | Action::RestoreTraceA | Action::InspectReceiver |
        Action::InstallIdentity | Action::EnterIdentity | Action::RestoreIdentity => return Err("Unexpected storage action".into()),
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
        Action::EnterTrialB | Action::InspectTrialB | Action::TraceTrialB => {
            let source = rescue.ok_or("Missing provisioned board reference")?;
            require(hash(source) == INSTALLED_SHA, "Wrong 05 reference")?;
            let mut baseline = source.to_vec();
            baseline[BOOT].copy_from_slice(&images.boot);
            baseline[IDENTITY_RX].copy_from_slice(&images.receiver);
            require(expected[..B.start] == baseline[..B.start] &&
                    expected[B.end..0x9a0000] == baseline[B.end..0x9a0000] &&
                    expected[0xdac000..] == baseline[0xdac000..], "Protected 05 firmware region differs")?;
            if action != Action::InspectTrialB {
                require(expected[B] == baseline[B] && expected[META] == baseline[META],
                        "05 requires the original B and first stable A record")?;
                io.preserve_b(&expected[B])?;
                if let Some(boot) = &images.trace_boot {
                    io.checkpoint(json!({"pretrace_archived":true,"before_sha256":before_hash,"b_sha256":hash(&expected[B])}))?;
                    replace(io, action, &mut expected, "boot", BOOT.start, boot)?;
                    return io.checkpoint(json!({"trace_boot_verified":true,"before_sha256":before_hash,
                                                "b_sha256":hash(&expected[B]),"boot_sha256":hash(boot)}));
                }
            } else {
                require(hash(&expected[META]) == "4c9eb1c6e30e14f9b24ae5959641342cb3b44469170c51bc294b2f8585031c73",
                        "05 journal does not prove the return to stable A")?;
                // Stable A is verified, so a bad inactive B must not strand the watch in the RAM stub.
            }
            return io.checkpoint(json!({"trial_read_complete":true,"before_sha256":before_hash,
                                        "b_sha256":hash(&expected[B]),"flash_writes":0}));
        }
        Action::RestoreTrialB | Action::RestoreHandoffB => {
            let source = rescue.ok_or("Missing original installation backup")?;
            if action == Action::RestoreHandoffB {
                same_board(source)?;
                require(hash(&source[B]) == B_SHA && source[BOOT] == images.boot && source[IDENTITY_RX] == images.receiver &&
                    source[META.start..META.start+PAGE] == images.journal && source[META.start+PAGE..META.end].iter().all(|&b| b==255),
                    "Wrong pre-trace board snapshot")?;
                for range in [0..BOOT.start, BOOT.end..B.start, ROLLBACK_BODY_END..B_TAIL,
                              B.end..0x9a0000, 0xdac000..SIZE] {
                    require(expected[range.clone()] == source[range], "Protected trace firmware changed")?;
                }
            } else {
                original(source)?;
                require(expected[BOOT] == images.boot, "Trial rescue expects the ordinary boot")?;
            }
            require(expected[IDENTITY_RX] == images.receiver &&
                    expected[BOOT.end..A.start] == source[BOOT.end..A.start] &&
                    expected[IDENTITY_RX.end..0x880000] == source[IDENTITY_RX.end..0x880000] &&
                    expected[ROLLBACK_BODY_END..B_TAIL] == source[ROLLBACK_BODY_END..B_TAIL],
                    "Recovery requires the installed identity pair and unchanged protected gap")?;
            let mut stable = source[META.start..META.start+8192].to_vec();
            stable[..PAGE].copy_from_slice(&images.journal);
            // Read back a valid stable-A record before retiring the other copy or touching B.
            replace(io, action, &mut expected, "stable-a", META.start, &stable)?;
            replace(io, action, &mut expected, "other-record", META.start+8192, &source[META.start+8192..META.end])?;
            replace(io, action, &mut expected, "b-body", B.start, &source[B.start..ROLLBACK_BODY_END])?;
            replace(io, action, &mut expected, "b-tail", B_TAIL, &source[B_TAIL..B.end])?;
            if action == Action::RestoreHandoffB {
                replace(io, action, &mut expected, "boot", BOOT.start, &images.boot)?;
            }
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
    if matches!(action, Action::RestoreTrialB | Action::RestoreHandoffB) {
        require(hash(&after[B]) == B_SHA && after[META.start..META.start+PAGE] == images.journal &&
                after[META.start+PAGE..META.end].iter().all(|&b| b == 255), "Trial rescue readback incomplete")?;
    } else if action == Action::Install { permit_entry(&after, images)?; } else { original(&after)?; }
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
