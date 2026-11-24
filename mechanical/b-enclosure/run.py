"""Run a dated B enclosure builder in a new local output directory."""

import argparse
from pathlib import Path
import subprocess

from workspace import SOURCE, WORKSPACE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freecad", type=Path, required=True, help="Path to FreeCAD.exe")
    parser.add_argument("--script", type=Path, required=True, help="Builder path relative to mechanical/b-enclosure")
    parser.add_argument("--out", type=Path, required=True, help="New output directory inside WRISTFLOW_B_WORKSPACE")
    args = parser.parse_args()
    executable = args.freecad.resolve(strict=True)
    source = (SOURCE / args.script).resolve(strict=True)
    output = args.out.resolve()
    if not source.is_relative_to(SOURCE) or not source.is_file() or source.suffix != ".py":
        parser.error("--script must point to a Python builder in mechanical/b-enclosure")
    if not output.is_relative_to(WORKSPACE):
        parser.error(f"--out must be in {WORKSPACE}")
    output.mkdir(parents=True, exist_ok=False)
    macro = output / "build.FCMacro"
    macro.write_text(
        "from pathlib import Path\n"
        "import runpy, sys, traceback\n"
        "import FreeCADGui as Gui\n"
        "from PySide import QtCore\n"
        f"src = Path({str(source)!r})\n"
        f"out = Path({str(output)!r})\n"
        "sys.path[:0] = [str(src.parent), " + repr(str(SOURCE)) + "]\n"
        "try:\n"
        "    Gui.getMainWindow().hide()\n"
        "    runpy.run_path(str(src))['run'](out)\n"
        "    (out / 'builder.ok').write_text('Completed', encoding='utf-8')\n"
        "except Exception:\n"
        "    (out / 'error.txt').write_text(traceback.format_exc(), encoding='utf-8')\n"
        "finally:\n"
        "    QtCore.QTimer.singleShot(0, QtCore.QCoreApplication.quit)\n",
        encoding="utf-8",
    )
    with (output / "freecad.log").open("w", encoding="utf-8") as log:
        subprocess.run([str(executable), "-u", str(output / "session.cfg"), str(macro)],
                       stdout=log, stderr=log, timeout=300, check=True)
    if not (output / "builder.ok").is_file():
        raise RuntimeError(f"FreeCAD did not complete. See logs in {output}")


if __name__ == "__main__":
    main()
