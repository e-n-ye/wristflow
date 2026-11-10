"""Serial transport for the owned ROM worker. Never opens during import.

No unbounded flush; the parent process deadline also covers driver calls that
ignore pySerial timeouts (open, control lines and close included).
"""
import time


class WallClock:
    kind = "monotonic"
    def now(self): return time.monotonic()
    def sleep(self, seconds): time.sleep(seconds)


class BoundedSerial:
    def __init__(self, port, role, journal, check, identity, *, factory=None):
        if role not in ("before", "rom", "after"):
            raise ValueError("Unknown serial role")
        self.role, self.journal, self.check, self.identity = role, journal, check, identity
        self.closed = False
        if factory is None:
            import serial
            factory = serial.Serial
            journal.event("serial_runtime", version=serial.VERSION, module=serial.__file__)
        self.serial = factory(port=None, baudrate=1000000, bytesize=8, parity="N", stopbits=1,
                              timeout=.01, write_timeout=.05, rtscts=False, dsrdtr=False, xonxoff=False)
        # port=None above cannot open the UART. Set controls before assigning it.
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.port = port

    def open(self):
        self.check()
        self.identity()
        self.check()
        self.journal.event("serial_open_intent", role=self.role)
        self.serial.open()
        self.check()

    def read(self, count):
        self.check()
        return self.serial.read(count)

    def write(self, wire):
        self.check()
        # Positive write_timeout uses the completed-byte result; flush() may
        # wait forever on serialwin32 and is intentionally not called.
        return self.serial.write(wire)

    def set_rts(self, high):
        if self.role != "rom" or type(high) is not bool:
            raise ValueError("RTS control belongs only to the ROM phase")
        if high:
            self.check()
        # Releasing our asserted RTS must remain possible after cancellation.
        self.journal.event("rts_level_intent", high=high)
        self.serial.rts = high
        self.journal.event("rts_level_complete", high=high)

    def close(self):
        self.journal.event("serial_close_intent", role=self.role)
        self.serial.close()
        self.closed = True
        self.journal.event("serial_close_complete", role=self.role)
