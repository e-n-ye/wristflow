"""Run in a fresh output directory, preserving earlier review models."""
import argparse
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('executable',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
dest=args.output.resolve()
dest.mkdir(exist_ok=False,parents=True)
script=Path(__file__).resolve().with_name('service.py')
macro=dest/'target.FCMacro'
macro.write_text(
    "from pathlib import Path\nimport traceback, FreeCADGui as Gui\nfrom PySide import QtCore\n"
    "Gui.getMainWindow().hide()\n"
    f"src=Path({str(script)!r})\ndst=Path({str(dest)!r})\n"
    "try:\n    ns={'__file__':str(src)}\n    exec(compile(src.read_text(encoding='utf-8'),str(src),'exec'),ns)\n    ns['run'](dst)\n"
    "except Exception:\n    (dst/'error.txt').write_text(traceback.format_exc(),encoding='utf-8')\n"
    "finally:\n    QtCore.QTimer.singleShot(0,QtCore.QCoreApplication.quit)\n",encoding='utf-8')
startup=subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow=0
with (dest/'freecad.log').open('w',encoding='utf-8') as log:
    subprocess.run([str(args.executable),'-u',str(dest/'session.cfg'),str(macro)],stdout=log,stderr=log,startupinfo=startup,timeout=180)
if not (dest/'reopened.ok').exists():
    err=dest/'error.txt'
    raise RuntimeError(err.read_text(encoding='utf-8') if err.exists() else 'Native model verification incomplete')
