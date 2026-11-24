"""Read the saved EPRO independently. Transfer the read-back pin positions and actual
wire geometry to a temporary KiCad schematic to obtain a second connectivity solve."""
from pathlib import Path
from collections import Counter,defaultdict
import argparse
import csv,hashlib,json,math,re,subprocess,uuid,zipfile
from workspace import OUTPUT
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--projects',type=Path,default=OUTPUT,help='Directory containing the four locally restored projects')
parser.add_argument('--kicad-cli',default='kicad-cli',help='KiCad CLI executable or path')
args=parser.parse_args()
out=args.projects
exe=args.kicad_cli
def records(b):return [r for l in b.splitlines() if l.strip() and (r:=json.loads(l))]
def attrs(rows,ix):
    return {r[ix]:r[ix+1] for r in rows}
def native_bool(n):return n not in (False,0,'no')
def close(a,b):return abs(a-b)<.00001
def on(x,y,s):
    x1,y1,x2,y2=s
    return close((x-x1)*(y2-y1),(y-y1)*(x2-x1)) and min(x1,x2)-1e-5<=x<=max(x1,x2)+1e-5 and min(y1,y2)-1e-5<=y<=max(y1,y2)+1e-5
def datahash(f):return hashlib.sha256(f.read_bytes()).hexdigest()

def verify(f):
    with zipfile.ZipFile(f) as z:
        assert not z.testzip()
        proj=json.loads(z.read('project.json'));b=next(iter(proj['boards'].values()))
        sheets=proj['schematics'][b['schematic']]['sheets']
        pcb=records(z.read('PCB/'+b['pcb']+'.epcb'))
        libs={s:records(z.read('SYMBOL/'+s+'.esym')) for s in proj['symbols']}
        footprint={s:records(z.read('FOOTPRINT/'+s+'.efoo')) for s in proj['footprints']}
        boardnets={};schpins={};external=set();libunits={};schlayout=[];compiled_wires=[];declared_nc=[];actual={}
        for c in [r for r in pcb if r[0]=='COMPONENT']:
            a=attrs([r for r in pcb if r[0]=='ATTR' and r[3]==c[1]],7);ref=a['Designator'];assert ref not in actual
            actual[ref]=a
            padnets={p[4]:(p[2],p[3]) for p in pcb if p[0]=='PAD_NET' and p[1]==c[1]}
            pads=[p for p in footprint[a['Footprint']] if p[0]=='PAD']
            assert {p[1] for p in pads}==set(padnets)
            for p in pads:
                no,n=padnets[p[1]];assert no==p[5]
                key=(ref,no)
                assert boardnets.get(key,n)==n
                boardnets[key]=n
        for sheetno,sheet in enumerate(sheets):
            rs=records(z.read(f"SHEET/{b['schematic']}/{sheet['id']}.esch"))
            at=defaultdict(dict)
            for r in rs:
                if r[0]=='ATTR':at[r[2]][r[3]]=r[4]
            wires=[(at[r[1]]['NET'],seg) for r in rs if r[0]=='WIRE' for seg in r[2]]
            # Offsetting pages keeps distinct sheet coordinates separated in the independent solver.
            offx=(sheetno%3)*2600;offy=(sheetno//3)*2000
            for name,(x,y,X,Y) in wires:compiled_wires.append((name,(x+offx,y-offy,X+offx,Y-offy)))
            for c in [r for r in rs if r[0]=='COMPONENT']:
                a=at[c[1]];ref=a['Designator'];sid=a['Symbol'];sym=libs[sid];did=a['Device']
                if proj['devices'][did]['attributes'].get('Convert to PCB')=='no':external.add(ref)
                else:assert actual[ref]['Device']==did and actual[ref]['Footprint']==a['Footprint']
                boundaries=[i for i,r in enumerate(sym) if r[0]=='PART']+[len(sym)]
                blocks={}
                for idx,(i,end) in enumerate(zip(boundaries,boundaries[1:]),1):
                    block=sym[i:end];pinattrs=defaultdict(dict)
                    for r in block:
                        if r[0]=='ATTR':pinattrs[r[2]][r[3]]=r[4]
                    blocks[block[0][1]]=(idx,[(p,pinattrs[p[1]]) for p in block if p[0]=='PIN'])
                unit,pins=blocks[c[2]]
                libunits[sid]=blocks
                assert c[5:7]==[0,0]
                schlayout.append((sid,ref,unit,c[3]+offx,c[4]-offy))
                for pin,pa in pins:
                    num=str(pa['NUMBER']);px=c[3]+pin[4];py=c[4]+pin[5]
                    touching={n for n,s in wires if on(px,py,s)}
                    assert len(touching)<=1,(ref,num,touching)
                    n=next(iter(touching),'')
                    key=(ref,num)
                    assert schpins.get(key,n)==n
                    schpins[key]=n
                    if not n:declared_nc.append((px+offx,py-offy))
            for i,(n,s) in enumerate(wires):
                for m,t in wires[i+1:]:
                    if n!=m:
                        assert not any(on(x,y,t) for x,y in [s[:2],s[2:]]) and not any(on(x,y,s) for x,y in [t[:2],t[2:]]),(sheet['name'],n,m)
        assert {k:v for k,v in schpins.items() if v and k[0] not in external}=={k:v for k,v in boardnets.items() if v}
        counts=Counter(v for v in schpins.values() if v)
        assert not [n for n,count in counts.items() if count==1]
    qa=out/'native-qa'/f.stem;qa.mkdir(parents=True,exist_ok=False)
    # One symbol and unit definition for each actual library symbol in the EPRO.
    lib=['EESchema-LIBRARY Version 2.4','#encoding utf-8']
    def coord(x):return round(x*10)
    def point(x,y):return (coord(x)+1000,1500-coord(y))
    for sid,blocks in libunits.items():
        symname='S'+sid
        lib+=['DEF '+symname+' U 0 20 Y Y '+str(len(blocks))+' L N','F0 "U" 0 150 40 H V L CNN','F1 "'+symname+'" 0 100 40 H V L CNN','DRAW']
        for unit,pins in blocks.values():
            for pin,a in pins:
                num=str(a['NUMBER']);name=str(a.get('NAME') or num).replace(' ','_')
                typ={'IN':'I','Input':'I','OUT':'O','Output':'O','Power':'W','Passive':'P','BiDi':'B'}.get(a.get('Pin Type'),'U')
                dr={0:'R',90:'U',180:'L',270:'D'}[pin[7]%360]
                lib.append(f'X {name} {num} {coord(pin[4])} {coord(pin[5])} {coord(pin[6])} {dr} 35 35 {unit} 1 {typ}')
        lib+=['ENDDRAW','ENDDEF']
    (qa/'local.lib').write_text('\n'.join(lib+['#End Library'])+'\n',encoding='utf8')
    (qa/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "EPRO")(type "Legacy")(uri "${KIPRJMOD}/local.lib")(options "")(descr "")))',encoding='utf8')
    legacy=['EESchema Schematic File Version 4','EELAYER 29 0','EELAYER END','$Descr A0 46811 33110','encoding utf-8','Sheet 1 1','$EndDescr']
    for i,(sid,ref,unit,cx,cy) in enumerate(schlayout,1):
        x,y=point(cx,cy)
        legacy+=['$Comp',f'L EPRO:S{sid} {ref}',f'U {unit} 1 {i+0x20000000:X}',f'P {x} {y}',
                 f'F 0 "{ref}" H {x} {y-70} 40  0000 C CNN',f'F 1 "S{sid}" H {x} {y} 40  0000 C CNN',
                 f'\t{unit} {x} {y}','\t1 0 0 -1','$EndComp']
    for n,s in compiled_wires:
        x,y=point(*s[:2]);X,Y=point(*s[2:])
        legacy+=['Wire Wire Line',f'\t{x} {y} {X} {Y}',f'Text GLabel {X} {Y} 2    40   BiDi ~ 0',n]
    for x,y in declared_nc:legacy.append('NoConn ~ %d %d'%point(x,y))
    sroot=str(uuid.uuid4())
    def q(s):return json.dumps(str(s),ensure_ascii=False)
    def u():return str(uuid.uuid4())
    def xy(x,y):return f'{(101.6+x*.254):.6f} {(101.6-y*.254):.6f}'
    mk=[f'(kicad_sch (version 20250114) (generator "wristflow_ep_ro") (uuid {sroot}) (paper "A0") (lib_symbols']
    for sid,blocks in libunits.items():
        sn='S'+sid
        mk.append(f'(symbol "EPRO:{sn}" (in_bom yes)(on_board yes) (property "Reference" "U" (at 0 2.54 0)(effects(font(size 1 1)))) (property "Value" "{sn}" (at 0 0 0)(effects(font(size 1 1)))))'[:-1])
        for unit,pins in blocks.values():
            mk.append(f'(symbol "{sn}_{unit}_1"')
            for pin,a in pins:
                name=str(a.get('NAME') or a['NUMBER'])
                ty={'IN':'input','OUT':'output','Input':'input','Output':'output','Passive':'passive','BiDi':'bidirectional'}.get(a.get('Pin Type'),'unspecified')
                mk.append(f'(pin {ty} line (at {pin[4]*.254:.6f} {pin[5]*.254:.6f} {pin[7]%360})(length {pin[6]*.254:.6f})(name {q(name)} (effects(font(size .9 .9))))(number {q(a["NUMBER"])} (effects(font(size .9 .9)))))')
            mk.append(')')
        mk.append(')')
    mk.append(')')
    for sid,ref,unit,cx,cy in schlayout:
        mk.append(f'(symbol (lib_id "EPRO:S{sid}")(at {xy(cx,cy)} 0)(unit {unit})(in_bom yes)(on_board yes)(uuid {u()}) (property "Reference" {q(ref)} (at {xy(cx,cy+10)} 0)(effects(font(size 1 1))))(property "Value" "QA" (at {xy(cx,cy)} 0)(effects(font(size 1 1)))) (instances(project "roundtrip"(path "/{sroot}"(reference {q(ref)})(unit {unit})))))')
    for n,s in compiled_wires:
        start=xy(*s[:2]);end=xy(*s[2:])
        mk.append(f'(wire(pts(xy {start})(xy {end}))(stroke(width 0)(type default))(uuid {u()}))')
        mk.append(f'(global_label {q(n)}(shape bidirectional)(at {end} 0)(effects(font(size 1 1))(justify left))(uuid {u()}))')
    for x,y in declared_nc:mk.append(f'(no_connect(at {xy(x,y)})(uuid {u()}))')
    sch=qa/'roundtrip.kicad_sch';sch.write_text('\n'.join(mk+[')']),encoding='utf8')
    import xml.etree.ElementTree as ET
    target=qa/'compiled.xml'
    subprocess.run([exe,'sch','export','netlist','--format','kicadxml','-o',target,sch],check=True,stdout=subprocess.DEVNULL)
    parsed={}
    for n in ET.parse(target).findall('.//nets/net'):
        for v in n.findall('node'):parsed[v.get('ref'),v.get('pin')]=n.get('name')
    bad=[(k,n,parsed.get(k)) for k,n in schpins.items() if n and n!=parsed.get(k)]
    assert not bad,bad[:6]
    erc=qa/'erc.json'
    subprocess.run([exe,'sch','erc','--format','json','-o',erc,sch],check=True,stdout=subprocess.DEVNULL)
    erc_types=Counter(v['type'] for s in json.loads(erc.read_text(encoding='utf8')).get('sheets',[]) for v in s['violations'])
    native=qa/'imported.kicad_pcb';im=qa/'import.json';dr=qa/'drc.json'
    subprocess.run([exe,'pcb','import','--report-format','json','--report-file',im,'-o',native,f],check=True,stdout=subprocess.DEVNULL)
    raw=native.read_text(encoding='utf8');seen=set();duplicate=0
    def distinct(m):
        nonlocal duplicate
        if m[1] in seen:
            duplicate+=1
            return '(uuid "'+str(uuid.uuid5(uuid.NAMESPACE_URL,m[1]+str(duplicate)))+'")'
        seen.add(m[1]);return m[0]
    normalized=re.sub(r'\(uuid "([a-f0-9-]+)"\)',distinct,raw)
    native.write_text(normalized,encoding='utf8')
    subprocess.run([exe,'pcb','drc','--format','json','-o',dr,native],check=True,stdout=subprocess.DEVNULL)
    d=json.loads(dr.read_text(encoding='utf8'));di=Counter(i['type'] for i in d['violations'])
    ic=json.loads(im.read_text(encoding='utf8'))
    return dict(file=f.name,sha256=datahash(f),pages=len(sheets),footprints=len(actual),external_parts=sorted(external),nets=len(counts),
                physical_pin_net_pairs=sum(bool(v) for v in boardnets.values()),net_count_single_pin=0,schematic_pad_mismatches=0,
                native_solver_mismatches=bad[:20],native_erc_types=erc_types,kicad='10.0.8',pcb_import_errors=ic['errors'],pcb_import_warnings=len(ic['warnings']),
                normalized_duplicate_kicad_uuids=duplicate,unrouted_items=len(d['unconnected_items']),drc_types=di)

projects=sorted(out.glob('*.epro'))
if len(projects)!=4:
    raise ValueError(f'Expected four B-R2 projects in {out}')
r=[verify(f) for f in projects]
(out/'electrical-readback.json').write_text(json.dumps(r,indent=2,ensure_ascii=False),encoding='utf8')
for b in r:print(b['file'],b['pages'],b['physical_pin_net_pairs'],b['native_solver_mismatches'][:2],b['native_erc_types'],b['drc_types'])
