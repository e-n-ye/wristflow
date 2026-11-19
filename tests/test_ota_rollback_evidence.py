from pathlib import Path
import json
import struct
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import Mock
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
import ota_rollback_evidence as trial


class RollbackTranscript(unittest.TestCase):
    def test_scanner_ready_gate_for_short_window_only(self):
        import ota_journal_window as dma
        for available in (True, False):
            with TemporaryDirectory() as td:
                media = Path(td)
                console = Mock()
                if not available:
                    console.prompt.side_effect = TimeoutError()
                observer = trial.TrialObservation(media, console, check=dma.CHECK, handoff=True)
                observer.poll()
                console.prompt.assert_not_called()
                (media/"await-phone").touch()
                observer.poll()
                observer.poll()
                console.prompt.assert_called_once()
                self.assertEqual((media/"phone-ready").exists(), available)
                self.assertEqual((media/"phone-unavailable").exists(), not available)

    def test_handoff_frames_are_not_product_execution_and_bad_crc_is_retained(self):
        def stage(point, detail):
            words = [trial.MAGIC,0x10001,*trial.TRACE_CHECK["startup_trace_tag"],ord(point),detail,
                     0x20023000,0x20041c00,0x20020000,0,1,0,0,0,4,0,0]
            wire = struct.pack("<19I",*words)
            return wire+struct.pack("<I",zlib.crc32(wire))
        prefix = b"\r\n".join((trial.ADVERTISING,trial.START,trial.END_OK,trial.INSTALL,trial.COMMIT))+b"\r\n"
        b = b"SFBL\n"+stage("T",0x1202ba49)
        a = b"SFBL\n"+stage("T",0x1202baa1)
        self.assertFalse(trial.handoff_report(prefix+b+a)["automatic_sequence_observed"])
        raw = prefix+b+trial.B_TOUCH+b"\r\n"+a+b"\x1b[32m"+trial.A_TOUCH+b" \x1b[0m\r\n"+trial.PRODUCT+b"yes\r\n"
        result = trial.handoff_report(raw)
        self.assertTrue(result["automatic_sequence_observed"] and result["original_storage_seen"])
        partial = trial.handoff_report(raw+stage("T",123)[:-1])
        self.assertTrue(partial["decode_error"])
        self.assertFalse(partial["automatic_sequence_observed"])
        with self.assertRaises(ValueError):
            trial.handoff_report(raw.replace(trial.END_OK,b"other release"))

    def test_two_product_boots_after_bound_commit(self):
        lines = [trial.ADVERTISING,trial.START,trial.END_OK,trial.INSTALL,trial.COMMIT,
                 b"SFBL",trial.B_TOUCH,b"SFBL",trial.A_TOUCH,trial.PRODUCT+b"face=diffusion"]
        def parse(items):
            return trial.trial_boots(b"\r\n".join(items)+b"\r\n")
        self.assertEqual(parse(lines)["trial_probe"],"12027edd")
        for altered in (lines[:6]+lines[7:],lines[:7]+lines[8:],lines[:-1],
                        [trial.A_TOUCH if x==trial.B_TOUCH else x for x in lines],lines+[b"SFBL"]):
            with self.assertRaises(ValueError):
                parse(altered)

    def test_host_reset_between_two_product_markers_is_not_automatic_return(self):
        first = b"\r\n".join((trial.ADVERTISING,trial.START,trial.END_OK,trial.INSTALL,trial.COMMIT,
                               b"SFBL",trial.B_TOUCH))+b"\r\n"
        second = b"\r\n".join((b"SFBL",trial.A_TOUCH,trial.PRODUCT+b"face=diffusion"))+b"\r\n"
        with TemporaryDirectory() as td:
            media = Path(td)
            (media/"startup-uart.bin").write_bytes(first+second)
            (media/"rx.bin").write_bytes(b"ack"+first+second)
            rx = [{"kind":"rx","offset":0,"bytes":3+len(first)},
                  {"kind":"rx","offset":3+len(first),"bytes":len(second)}]
            events = media/"wire-events.jsonl"
            for interference in ([], [{"kind":"rts_intent"}], [{"kind":"tx"}]):
                events.write_text("\n".join(map(json.dumps,[rx[0],*interference,rx[1]])),encoding="utf-8")
                if interference:
                    with self.assertRaises(ValueError):
                        trial.passive_boots(media)
                else:
                    self.assertEqual(trial.passive_boots(media)["return_probe"],"12027f35")
