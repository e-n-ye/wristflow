"""Pack a private restore kit. PCB templates and library-derived records stay local."""
from pathlib import Path
import sqlite3,json,hashlib,zipfile,shutil
from workspace import OUTPUT, REFERENCE, LIBRARY, WORKSPACE
from restore_projects import records,embedded_footprints
def digest(b):return hashlib.sha256(b).hexdigest()
out=OUTPUT
src=REFERENCE
dest=WORKSPACE/'restore-kit'
dest.mkdir(parents=True,exist_ok=False)
reference=zipfile.ZipFile(src)
embedded=embedded_footprints(reference)
lib=sqlite3.connect(LIBRARY.as_uri()+'?mode=ro',uri=True)
manifest=dict(reference_sha256=digest(src.read_bytes()),projects={})
stored=set()
with zipfile.ZipFile(dest/'design.zip','w',zipfile.ZIP_DEFLATED) as target:
    for f in sorted(out.glob('*.epro')):
        entries=[]
        with zipfile.ZipFile(f) as z:
            for info in z.infolist():
                n=info.filename;raw=z.read(n);base=None
                item=dict(name=n,date=info.date_time,sha256=digest(raw))
                if n.endswith(('.efoo','.esym')):
                    k=Path(n).stem
                    if n in reference.namelist():
                        item['reference']=n;base=records(reference.read(n))
                    else:
                        r=lib.execute('select dataStr from components where uuid=?',(k,)).fetchone()
                        if r:item['library']=k;base=records(r[0])
                        elif n.endswith('.efoo'):
                            rows=records(raw)
                            for original,v in embedded.items():
                                if v[0]==rows[0] and len(v)==len(rows) and v[2:]==rows[2:]:
                                    item['embedded']=original;base=v;break
                if base is not None:
                    rows=records(raw);assert len(base)==len(rows)
                    edits=[(i,new) for i,(old,new) in enumerate(zip(base,rows)) if old!=new]
                    if edits:item['edits']=edits
                else:
                    path=f.stem+'/'+n if not n.endswith(('.esym','.efoo')) else n
                    item['drawing']=path
                    if path not in stored:target.writestr(path,raw);stored.add(path)
                entries.append(item)
        manifest['projects'][f.name]=entries
    target.writestr('manifest.json',json.dumps(manifest,ensure_ascii=False,separators=(',',':')).encode())
for n in ['both-sides.png','placement-BOM.csv','reference-delta.csv','free-space.csv','electrical-readback.json','mechanical-readback.json','layout-1to1.pdf']:
    shutil.copyfile(out/n,dest/n)
print('Private restore kit',len(stored),'unique members;',sum(len(x) for x in manifest['projects'].values()),'project members')
