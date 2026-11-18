//! Capture the original NOR in independently verifiable 64 KiB blocks.
use crate::{media::{CHUNK, SIZE}, require, Result};

pub fn snapshot(name: &str, mut read: impl FnMut(&str, usize) -> Result<Vec<u8>>) -> Result<Vec<u8>> {
    let mut bytes = Vec::with_capacity(SIZE);
    for offset in (0..SIZE).step_by(CHUNK) {
        let block = read(&format!("{name}-{offset:08x}"), offset)?;
        require(block.len() == CHUNK, "Incomplete NOR snapshot block")?;
        bytes.extend_from_slice(&block);
    }
    Ok(bytes)
}

pub fn ended_at_shell(raw: &[u8]) -> bool {
    let Some(prior) = raw.strip_suffix(b"\r\nOK\r\nmsh >") else { return false; };
    let Some(index) = prior.len().checked_sub(14) else { return false; };
    let trailer = &prior[index..];
    trailer.starts_with(b"CRC:0x") && trailer[6..].iter().all(u8::is_ascii_hexdigit)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn blocks_remain_ordered_and_short_reads_stop() {
        let data = snapshot("before", |_, off| Ok(vec![(off/CHUNK) as u8; CHUNK])).unwrap();
        for (i, block) in data.chunks_exact(CHUNK).enumerate() {
            assert!(block.iter().all(|&b| b == i as u8));
        }
        let mut offsets = Vec::new();
        let bad = snapshot("before", |_, off| {
            offsets.push(off);
            Ok(vec![0; if off == CHUNK { CHUNK - 1 } else { CHUNK }])
        });
        assert!(bad.is_err());
        assert_eq!(offsets, [0, CHUNK]);
    }

    #[test]
    fn only_an_intact_terminal_trailer_permits_another_read() {
        let original = b"\xff\xffCRC:0xc1921911\r\nOK\r\nmsh >";
        assert!(ended_at_shell(original));
        for i in 0..original.len() {
            assert!(!ended_at_shell(&original[..i]));
        }
        assert!(!ended_at_shell(b"CRC:0xg1921911\r\nOK\r\nmsh >"));
        assert!(!ended_at_shell(b"CRC:0xc1921911\r\nFail\r\nmsh >"));
    }
}
