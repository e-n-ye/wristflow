"""Export review images with FreeCAD's own GUI API in an isolated process."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--freecad',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--variant',choices=['A','B','both'],default='both')
    args=parser.parse_args()
    executable=args.freecad.resolve(strict=True)
    out=args.out.resolve(strict=True)
    script=Path(__file__).with_name('review_freecad.py').resolve(strict=True)
    for variant in (['A','B'] if args.variant=='both' else [args.variant]):
        macro=out/('BatchReview_'+variant+'.FCMacro')
        result=out/('visual-review_'+variant+'.json')
        if macro.exists() or result.exists():
            raise FileExistsError('Preserve existing review; choose a fresh build output')
        request={'out':str(out),'variant':variant}
        macro.write_text(
            'from pathlib import Path\nimport traceback\nimport FreeCADGui as Gui\n'
            'try: from PySide import QtCore\nexcept ImportError: from PySide6 import QtCore\n'
            'Gui.getMainWindow().hide()\nREQUEST='+repr(request)+'\n'
            'try:\n    script=Path('+repr(str(script))+')\n'
            "    exec(compile(script.read_text(encoding='utf-8'),str(script),'exec'))\n"
            'except Exception:\n    Path('+repr(str(out/('batch-review-error_'+variant+'.txt')))+
            ").write_text(traceback.format_exc(),encoding='utf-8')\n"
            'finally:\n    QtCore.QTimer.singleShot(0,QtCore.QCoreApplication.quit)\n',encoding='utf-8')
        options={}
        if hasattr(subprocess,'STARTUPINFO'):
            startup=subprocess.STARTUPINFO()
            startup.dwFlags=subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow=0
            options['startupinfo']=startup
        with (out/('gui-render_'+variant+'.log')).open('w',encoding='utf-8') as log:
            completed=subprocess.run([str(executable),'-u',str(out/('renderer-'+variant+'-user.cfg')),str(macro)],
                stdout=log,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace',timeout=180,**options)
        review=json.loads(result.read_text(encoding='utf-8')) if result.exists() else {}
        if completed.returncode or review.get('status')!='ok':
            raise RuntimeError('Native GUI review failed: '+variant)
        if not all((out/name).stat().st_size>1000 for name in review['files']):
            raise RuntimeError('Missing native viewport image')
        print(json.dumps(review,ensure_ascii=False))


if __name__=='__main__':
    main()
