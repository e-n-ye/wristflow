import sys
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import Mock, call

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from ota_phone_board import ADVERTISING, FIRST_DATA_REJECTED, IDLE, PRODUCT, START, PhoneObservation, confirmed_identity, phone_evidence


def uart(*lines):
    return b"\r\n".join(lines) + b"\r\n"


class PhoneBoard(unittest.TestCase):
    def test_distinct_static_identity_must_precede_advertising(self):
        public = "52:02:03:04:05:06"
        addr = b"[ota-radio] receiver address=DA:83:D8:7D:11:36 type=1"
        self.assertEqual(confirmed_identity(uart(addr, ADVERTISING), public), "DA:83:D8:7D:11:36")
        for log in [uart(ADVERTISING,addr), uart(addr,addr,ADVERTISING),
                    uart(addr.replace(b"type=1",b"type=0"),ADVERTISING),
                    uart(addr.replace(b"DA:83:D8:7D:11:36",b"C0:00:00:00:00:00"),ADVERTISING)]:
            self.assertIsNone(confirmed_identity(log, public))
        self.assertIsNone(confirmed_identity(uart(addr,ADVERTISING), "DA:83:D8:7D:11:36"))
    def test_first_data_only_after_advertising_and_start(self):
        captured = uart(ADVERTISING, START, FIRST_DATA_REJECTED, IDLE, b"SFBL", PRODUCT + b"brightness=50")
        state = phone_evidence(captured)
        self.assertTrue(state["first_data_rejected"] and state["product_return"])
        self.assertFalse(phone_evidence(uart(ADVERTISING, START, FIRST_DATA_REJECTED, IDLE, PRODUCT))["product_return"])
        for other in [uart(ADVERTISING, IDLE, PRODUCT), uart(START, ADVERTISING, FIRST_DATA_REJECTED),
                      uart(ADVERTISING, FIRST_DATA_REJECTED, START),
                      uart(ADVERTISING, START, FIRST_DATA_REJECTED, START), uart(ADVERTISING, START) + FIRST_DATA_REJECTED]:
            self.assertFalse(phone_evidence(other)["first_data_rejected"])
        self.assertTrue(phone_evidence(captured + uart(b"[ota-ready] trial committed"))["install_started"])

    def test_prompt_on_advertising_and_finish_only_after_product_return(self):
        with TemporaryDirectory() as tmp:
            media = Path(tmp)
            control = Mock()
            observer = PhoneObservation(media, control)
            path = media/"startup-uart.bin"
            observer.poll()
            (media/"before-00e80000.bin").touch()
            observer.poll()
            control.stage.assert_called_once_with("phone_ready", "running")
            path.write_bytes(uart(ADVERTISING, START, FIRST_DATA_REJECTED, IDLE))
            observer.poll()
            observer.poll()
            self.assertEqual(control.stage.call_args_list, [call("phone_ready", "running"),
                             call("phone_ready", "complete"), call("phone", "running")])
            self.assertFalse((media/"finish-listening").exists())
            with path.open("ab") as stream:
                stream.write(uart(b"SFBL", PRODUCT + b"face=diffusion"))
            observer.poll()
            self.assertTrue((media/"finish-listening").is_file())
