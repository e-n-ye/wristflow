"""Run native CAD validation and check its structured result, not just exit 0."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--freecadcmd',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    args=parser.parse_args()
    out=args.out.resolve(strict=True)
    result=out/'native-validation.json'
    if result.exists():
        raise FileExistsError('Preserve previous validation before rerunning')
    env=os.environ.copy(); env['WRISTFLOW_CAD_CHECK_OUT']=str(out)
    with (out/'native-validation.log').open('x',encoding='utf-8') as log:
        completed=subprocess.run([str(args.freecadcmd.resolve(strict=True)),str(Path(__file__).with_name('check_freecad.py'))],
            input='',stdout=log,stderr=subprocess.STDOUT,text=True,encoding='utf-8',
            errors='replace',env=env,timeout=1200)
    data=json.loads(result.read_text(encoding='utf-8')) if result.exists() else {}
    if completed.returncode or data.get('status')!='ok':
        raise RuntimeError('Native validation failed; inspect native-validation.json/log')
    print(json.dumps(data,ensure_ascii=False))


if __name__=='__main__':
    main()
