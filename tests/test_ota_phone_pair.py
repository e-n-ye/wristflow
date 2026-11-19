import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import Mock, call

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from ota_phone_board import ADVERTISING, FIRST_DATA_REJECTED, IDLE, PRODUCT, SIGNATURE_FILE, START, phone_evidence
from ota_phone_pair import (BLOCKS, BODY_END, IDLE_END, PairedObservation, android_receipts, body_completion, uart_receipts)

PEER = "DA:83:D8:7D:11:36"
IDENTITY = f"[ota-radio] receiver address={PEER} type=1".encode()
ANDROID_START = ["start with 1.1.38", "connecting to " + PEER,
                 "send offline start req, file size 3033926, file count 1482", "receive offline start response 0, freq 1"]
SIGNATURE = [*ANDROID_START, "offline packet response 1, retrans 0, completed 0", "sendDfuImageEndReq", "end with 0"]


def android(messages):
    return "\n".join("I sifli-DFU: " + msg for msg in messages)


def uart(*lines):
    return b"\r\n".join(lines) + b"\r\n"


class PhonePairTests(unittest.TestCase):
    def test_successive_sessions_and_recoverable_body_retry(self):
        acks = [f"offline packet response 0, retrans 0, completed {i}" for i in range(1, BLOCKS + 1)]
        acks.insert(14, "offline packet response 1, retrans 1, completed 14")
        text = android([*SIGNATURE, *ANDROID_START, *acks, "sendDfuImageEndReq", "end with 0"])
        s = android_receipts(text, PEER)
        self.assertTrue(s["signature_rejected"] and s["body_all_acked"])
        self.assertEqual(s["sessions"][1]["retries"], [[1,14]])
        self.assertFalse(android_receipts(text, "another peer")["body_all_acked"])
        self.assertFalse(android_receipts(text.replace(acks[-1], ""), PEER)["body_all_acked"])
        self.assertTrue(android_receipts(text.replace(acks[1], ""), PEER)["body_all_acked"])
        self.assertFalse(android_receipts(text.replace(acks[4], acks[6]), PEER)["body_all_acked"])
        self.assertFalse(android_receipts(text + "\n" + android(ANDROID_START), PEER)["body_all_acked"])
        self.assertFalse(android_receipts(text.replace("response 1, retrans 0", "response 1, retrans 1"), PEER)["signature_rejected"])

    def test_separate_uart_ends_and_retries(self):
        retry = b"[ota-radio] request=40 result=1 accepted=14 sent=16"
        for optional_end in ([], [IDLE_END.encode()]):
            raw = uart(ADVERTISING, START, FIRST_DATA_REJECTED, *optional_end, START, retry, BODY_END.encode(), IDLE)
            s = uart_receipts(phone_evidence(raw))
            self.assertTrue(s["signature_rejected"] and s["body_end_rejected"])
            self.assertEqual(s["body_retries"], [[1,14]])
            self.assertFalse(uart_receipts(phone_evidence(raw.replace(BODY_END.encode(), IDLE_END.encode())))["body_end_rejected"])
        self.assertFalse(uart_receipts(phone_evidence(uart(ADVERTISING, START, FIRST_DATA_REJECTED, BODY_END.encode())))["body_end_rejected"])

    def test_app_that_omits_success_counters_needs_visible_progress_and_end_receipt(self):
        end = ["sendDfuImageEndReq", "receive offline end response", "end with 0"]
        raw = android([*SIGNATURE, *ANDROID_START, *end])
        s = android_receipts(raw, PEER)
        self.assertFalse(s["body_all_acked"])
        self.assertIsNone(body_completion(s, False))
        self.assertEqual(body_completion(s, True), "operator pro100 and Android END receipt")
        for events in [end[:1], list(reversed(end)), [end[0], end[1]+" 0"],
                       ["offline packet response 1, retrans 0, completed 0", *end],
                       ["offline packet response 0, retrans 0, completed 1481", *end]]:
            state = android_receipts(android([*SIGNATURE, *ANDROID_START, *events]), PEER)
            self.assertIsNone(body_completion(state, True))

    def test_second_selection_only_after_rejection_on_this_receiver(self):
        for address, late in [(PEER,False), ("DA:83:D8:7D:11:37",False), (PEER,True)]:
            with self.subTest(address=address,late=late), TemporaryDirectory() as tmp:
                media, console = Path(tmp), Mock()
                collector = Mock()
                collector.poll.return_value = None
                log = media/"android-dfu.log"
                log.write_text(android(SIGNATURE), encoding="utf-8")
                observer = PairedObservation(media,console,filename=SIGNATURE_FILE,
                                             original_address="52:02:03:04:05:06",expected_address=address,
                                             android_log=log,collector=collector)
                path = media/"startup-uart.bin"
                path.write_bytes(uart(IDENTITY, ADVERTISING, START))
                observer.poll()
                self.assertNotIn(call("body","running"), console.stage.call_args_list)
                with path.open("ab") as out:
                    out.write(uart(FIRST_DATA_REJECTED))
                    if late:
                        out.write(uart(IDLE, b"SFBL", PRODUCT+b"face=ok"))
                observer.poll()
                observer.poll()
                self.assertEqual(console.stage.call_args_list.count(call("body","running")), int(address == PEER and not late))

    def test_soft_deadline_and_lost_collector_close_phone_invitations(self):
        for loss in [True, False]:
            with self.subTest(loss=loss), TemporaryDirectory() as tmp:
                media, console, collector = Path(tmp), Mock(), Mock()
                collector.poll.return_value = None
                log = media/"android-dfu.log"
                log.write_text("", encoding="utf-8")
                o = PairedObservation(media,console,filename=SIGNATURE_FILE,original_address="52:02:03:04:05:06",
                                      expected_address=PEER,android_log=log,collector=collector)
                path = media/"startup-uart.bin"
                path.write_bytes(uart(IDENTITY, ADVERTISING, START, FIRST_DATA_REJECTED))
                o.poll()
                self.assertNotIn(call("body","running"), console.stage.call_args_list)
                log.write_text(android(SIGNATURE), encoding="utf-8")
                if loss:
                    collector.poll.return_value = 1
                else:
                    o.active_until = 0
                o.poll()
                self.assertIn(call("disconnect","running"), console.stage.call_args_list)
                with path.open("ab") as stream:
                    stream.write(uart(IDLE, b"SFBL", PRODUCT+b"face=ok"))
                o.poll()
                self.assertNotIn(call("body","running"), console.stage.call_args_list)
                self.assertTrue((media/"finish-listening").is_file())
