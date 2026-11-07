"""Launch the local FreeCAD draft build without modifying an existing output."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--freecadcmd',required=True,type=Path)
    parser.add_argument('--source-zip',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--parameters',type=Path,default=Path(__file__).with_name('parameters.json'))
    parser.add_argument('--variant',choices=['A','B','both'],default='both')
    args=parser.parse_args()
    executable=args.freecadcmd.resolve(strict=True)
    source=args.source_zip.resolve(strict=True)
    parameters=args.parameters.resolve(strict=True)
    cfg=json.loads(parameters.read_text(encoding='utf-8'))
    if hashlib.sha256(source.read_bytes()).hexdigest()!=cfg['source_zip_sha256']:
        raise ValueError('Source archive differs from inspected SHA256')
    out=args.out.resolve()
    if out.exists():
        raise FileExistsError('Use a new output directory; existing CAD and evidence are preserved: '+str(out))
    out.mkdir(parents=True)
    request={'source_zip':str(source),'parameters':str(parameters),'out':str(out),
             'variants':['A','B'] if args.variant=='both' else [args.variant]}
    request_path=out/'request-local.json'
    request_path.write_text(json.dumps(request,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    env=os.environ.copy(); env['WRISTFLOW_CAD_REQUEST']=str(request_path)
    build=Path(__file__).with_name('build_freecad.py').resolve()
    with (out/'freecad-build.log').open('w',encoding='utf-8') as log:
        completed=subprocess.run([str(executable),str(build)],input='',stdout=log,
                                 stderr=subprocess.STDOUT,text=True,encoding='utf-8',
                                 errors='replace',env=env,timeout=1200)
    result_path=out/'build-result.json'
    if not result_path.exists():
        print('FreeCAD produced no completion report; inspect freecad-build.log',file=sys.stderr)
        return 1
    result=json.loads(result_path.read_text(encoding='utf-8'))
    if completed.returncode!=0 or result['status']!='ok':
        print(json.dumps(result,ensure_ascii=False,indent=2))
        return 1
    for report in result['reports']:
        print(json.dumps({k:report[k] for k in ['variant','modelled_checks_pass','rear_body_bbox_mm','glass_datum_to_outer_lid_mm','fcstd']},ensure_ascii=False))
    if not all(r['modelled_checks_pass'] for r in result['reports']):
        print('Draft has modelled collisions; not released. Inspect report JSON.',file=sys.stderr)
        return 2
    print('Scoped CAD checks passed; physical fit and printing remain blocked.')
    return 0


if __name__=='__main__':
    raise SystemExit(main())
