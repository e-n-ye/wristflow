"""Wake Product and hold its display temporarily through the board's USB UART."""

import argparse
import json
from pathlib import Path
import re
import secrets
import threading

from battery_device import port_identity
from battery_lab import Journal
from lab_evidence import write_json
from lab_serial import ProtocolError, RecordedClient, RecordedSession, SerialTransport


MAX_SECONDS = 1200
TOKEN = r"(?!00000000)[0-9a-f]{8}"


def hold_seconds(text):
    seconds = int(text)
    if not 1 <= seconds <= MAX_SECONDS:
        raise argparse.ArgumentTypeError(f"Expected 1..{MAX_SECONDS} seconds")
    return seconds


class AwakeClient(RecordedClient):
    COMMAND_PATTERN = rf"wf_awake (?:start {TOKEN} [1-9][0-9]{{0,3}}|stop {TOKEN})"

    def _action(self, command, token, accepted):
        response = re.compile(rf"\[wf-awake\] token={re.escape(token)} result=([a-z_]+)")

        def match(line):
            return response.fullmatch(line.rsplit("msh />", 1)[-1].strip())

        lines = self.request(command, match)
        results = [reply[1] for line in lines if (reply := match(line))]
        if len(results) != 1:
            raise ProtocolError("Expected one fresh display response for this token")
        if results[0] not in accepted:
            raise ProtocolError(f"Product rejected the display request: {results[0]}")
        return results[0]

    def start(self, token, seconds):
        if type(seconds) is not int or not 1 <= seconds <= MAX_SECONDS:
            raise ValueError(f"Temporary screen hold must be 1–{MAX_SECONDS} seconds")
        return self._action(f"wf_awake start {token} {seconds}", token, {"active"})

    def stop(self, token):
        return self._action(f"wf_awake stop {token}", token, {"stopped", "inactive"})


def operate(args):
    if args.action == "start":
        port, location = args.port, args.location
        token = f"{secrets.randbelow(0xffffffff) + 1:08x}"
        endpoint = port_identity(port, location)
    else:
        start = json.loads(args.start_result.read_text(encoding="utf-8"))
        if start.get("kind") != "serial-awake" or start.get("action") != "start":
            raise ValueError("Start receipt required")
        token, endpoint = start["token"], start["endpoint"]
        if not re.fullmatch(TOKEN, token):
            raise ValueError("Invalid display hold token")
        port, location = endpoint["device"], endpoint["location"]

    def same_endpoint():
        actual = port_identity(port, location)
        if actual != endpoint:
            raise ValueError("Board endpoint changed")
        return actual

    output = args.out.resolve()
    report = {"kind": "serial-awake", "action": args.action, "token": token,
              "endpoint": endpoint, "passed": False}
    if args.action == "start":
        report["seconds"] = args.seconds
    journal = Journal(output, binding=report.copy(), mode="serial-awake")
    session = RecordedSession(journal, SerialTransport(port), threading.Event(), AwakeClient,
                              identity=same_endpoint, timeout=4)
    try:
        with session as client:
            if args.action == "start":
                report["result"] = client.start(token, args.seconds)
            else:
                report["result"] = client.stop(token)
        report["passed"] = True
    except (Exception, KeyboardInterrupt) as exc:
        report["error"] = repr(exc)
    report["port_closed"] = session.port_closed
    write_json(output / "awake.json", report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="action", required=True)
    start = modes.add_parser("start", help="Wake and hold the display for a fixed duration")
    start.add_argument("--port", required=True)
    start.add_argument("--location", required=True, help="USB endpoint location")
    start.add_argument("--seconds", type=hold_seconds, default=300, metavar="1..1200")
    stop = modes.add_parser("stop", help="Release only the hold in the saved start receipt")
    stop.add_argument("--start-result", required=True, type=Path)
    for mode in (start, stop):
        mode.add_argument("--out", required=True, type=Path, help="New evidence directory")
    result = operate(parser.parse_args())
    print(result["result"] if result["passed"] else result["error"], flush=True)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
