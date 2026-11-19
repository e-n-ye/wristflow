from pathlib import Path
import json
import sys
from tempfile import TemporaryDirectory
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
import ota_rollback_evidence as trial


class RollbackTranscript(unittest.TestCase):
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
