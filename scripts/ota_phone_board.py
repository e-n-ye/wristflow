"""Collect the Android wrong-board acceptance observations on a provisioned board."""
from pathlib import Path
import re
from zipfile import ZipFile

from ota_first_install import require, sha

FILENAME = "01-bad-board.bin"
FILE_BYTES = 3033926
FILE_SHA = "e50b14000bf830fe959484069fef692770a28dac22ddc4774d6772d7c14add00"
ADVERTISING = b"[ota-radio] advertising=0 name=WristFlow OTA"
START = b"[ota-radio] request=38 result=0 accepted=0 sent=16"
FIRST_DATA_REJECTED = b"[ota-radio] request=40 result=1 accepted=0 sent=16"
IDLE = b"[ota-radio] inactive, returning to boot selection"
PRODUCT = b"[product] storage=ready restore=yes "
READY_PHASE = ("phone_ready", "此行显示进行中时，手机打开 SIFLI BLE 首页准备扫描")
PHONE_PHASE = ("phone", "此行显示进行中时立即扫描 WristFlow OTA，点 NOR DFU → OFFLINE，选 01-bad-board.bin 一次")


def verify_fixture(archive):
    with ZipFile(archive) as package:
        items = [item for item in package.infolist() if item.filename == FILENAME]
        require(len(items) == 1 and items[0].file_size == FILE_BYTES, "Missing or ambiguous Android fixture")
        require(sha(package.read(items[0])) == FILE_SHA, "Wrong Android fixture contents")
    return dict(filename=FILENAME, bytes=FILE_BYTES, sha256=FILE_SHA)


def phone_evidence(uart):
    # Only complete UART lines can advance the live operator display.
    lines = [line.rstrip(b"\r") for line in uart.split(b"\n")[:-1]]
    advertised = ADVERTISING in lines
    session = lines[lines.index(ADVERTISING) + 1:] if advertised else []
    idle = IDLE in session
    requests = session[:session.index(IDLE)] if idle else session
    replies = [line for line in requests if line.startswith(b"[ota-radio] request=")]
    after_idle = session[session.index(IDLE) + 1:] if idle else []
    product = b"SFBL" in after_idle and any(
        line.startswith(PRODUCT) for line in after_idle[after_idle.index(b"SFBL") + 1:])
    install = any(b"starting local install" in line or b"[ota-ready]" in line for line in lines)
    return dict(advertising=advertised, first_data_rejected=replies == [START, FIRST_DATA_REJECTED],
                idle_return=idle, product_return=product, install_started=install,
                replies=[line.decode("ascii", "replace") for line in replies])


def confirmed_identity(uart, original_address):
    address, marker, _ = uart.partition(ADVERTISING)
    if not marker:
        return None
    matches = re.findall(rb"(?m)^\[ota-radio\] receiver address=((?:[A-F0-9]{2}:){5}[A-F0-9]{2}) type=1\r?$", address)
    if len(matches) != 1:
        return None
    value = matches[0].decode("ascii")
    number = int(value.replace(":", ""), 16)
    random_bits = number & ((1 << 46) - 1)
    return value if value != original_address and number >> 46 == 3 and 0 < random_bits < (1 << 46) - 1 else None


class PhoneObservation:
    def __init__(self, media, console, *, original_address=None):
        self.uart = Path(media) / "startup-uart.bin"
        # The last 24 reads take roughly one minute on the original COM5.
        self.last_reads = Path(media) / ("key-00000000.bin" if original_address else "before-00e80000.bin")
        self.original_address = original_address
        self.console = console
        self.size = 0
        self.prepared = False
        self.announced = False
        self.finished = False

    def poll(self):
        if not self.prepared and (self.last_reads.exists() or self.uart.exists()):
            self.console.stage("phone_ready", "running")
            self.prepared = True
            print("Android operator: have SIFLI BLE ready to scan on the phone", flush=True)
        if self.finished or not self.uart.exists():
            return
        size = self.uart.stat().st_size
        if size == self.size:
            return
        self.size = size
        uart = self.uart.read_bytes()
        state = phone_evidence(uart)
        identity = confirmed_identity(uart, self.original_address) if self.original_address else None
        if state["advertising"] and not self.announced and (not self.original_address or identity):
            self.console.stage("phone_ready", "complete")
            self.console.stage("phone", "running")
            self.announced = True
            print("WristFlow OTA ready for Android: NOR DFU > OFFLINE > " + FILENAME, flush=True)
            if identity:
                print("Receiver address suffix " + identity[-5:], flush=True)
        if state["product_return"] and not state["install_started"]:
            # The child remains the sole port owner. End its passive listening
            # after the original Product's full storage-ready line has arrived.
            self.uart.with_name("finish-listening").touch(exist_ok=True)
            self.finished = True
