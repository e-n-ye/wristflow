"""Generate the four B-R2 placement-review projects from local library inputs."""
import argparse
from pathlib import Path
import workspace

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out', type=Path, default=workspace.OUTPUT, help='New output directory (default: workspace/eda)')
parser.add_argument('--reference', type=Path, default=workspace.REFERENCE, help='Local Huangshan V1.2 EPRO')
parser.add_argument('--library', type=Path, default=workspace.LIBRARY, help='Local LCEDA Pro system library')
parser.add_argument('--reviewed-placement', action='store_true', help='Use the saved R2 board outlines and placements')
args = parser.parse_args()
workspace.REFERENCE = args.reference.resolve()
workspace.LIBRARY = args.library.resolve()
out = args.out
out.mkdir(parents=True, exist_ok=False)

if args.reviewed_placement:
    from library import *
else:
    from place import *
from itertools import count
from collections import OrderedDict
import csv
from shapely.geometry import LineString

names={'main':'WristFlow-B-R2','rear':'WristFlow-B-Rear-FPC','crown':'WristFlow-B-Crown','bridge':'WristFlow-B-Crown-Contacts'}
board_dirs={'main':'b-r2-main','rear':'b-rear-fpc','crown':'b-crown','bridge':'b-crown-contacts'}
titles={'Module':'01  模组、供电循环与 UART','Charger':'02  触点输入、充电与电芯温度',
        'Rails':'03  受控电源','AMOLED':'04  原屏直连与触摸','Audio':'05  差分功放与输出滤波',
        'Mic':'06  上进音麦克风','Motion light':'07  姿态、地磁与前向环境光',
        'Crown motor':'08  表冠与电机','Service':'09  探针、测流桥与默认通路','Rear interface':'10  后盖接口与光学电源',
        'Rear flex':'01  后盖柔板端子与光学器件','Crown':'01  表冠立板与直角焊端','Contact bridge':'01  表冠水平四触点'}
notes={
 'Module':'N16R8 module includes Flash, PSRAM, both crystals and PCB antenna. RTS_N_OD is driven open-drain by the fixture.',
 'Charger':'LP503030 450 mAh PCM pack. External 103JT on pack surface to PACK-. R60 / R64 are provisional. Keep charge disabled until TS validation.',
 'Rails':'PA38 enables VSYS_1. PA26 controls the display 3V3 regulator. PA30 controls GS_3V3 and PPG.',
 'AMOLED':'J201 = LCD adapter CN1, 24 signals plus shell lands. Keep 22R series terminations. R206 selects OLED_VBAT. No parallel 3V3 link.',
 'Audio':'BTL outputs go through the 10P tail. SPK_OUTN is a driven output. C22/C23/C25 are optional filter positions (DNP).',
 'Mic':'Top port must face its own acoustic passage. U9 to ADCIN; MIC_BIAS through R30. Confirm gasket and enclosure before enabling audio.',
 'Motion light':'U16 faces the front light guide. U14 placement/heading needs motor, speaker and charging-fixture characterization.',
 'Crown motor':'KEY_N pulls Q3 gate low to reproduce original positive KEY1 pulse. A/B use PA24/PA25. Motor wire lands on rear side.',
 'Service':'TP11 GND, TP12 sense only, TP13 MCU RX, TP14 MCU TX, TP15 open-drain RTS. Close all three removable bridges to run.',
 'Rear interface':'0.5 A/contact. Single 5V and single return. Check load current plus charging current, blocked/reverse polarity, insertion side and flex bend.',
 'Rear flex':'Optical island and terminal placement study only. Lid contour and tail fold must be developed from the bending neutral axis.',
 'Crown':'Standing PCB in local Y/Z. Check encoder pinout and locator holes against approved A suffix drawing. Four joints continue to horizontal gold contacts.',
 'Contact bridge':'Horizontal board, four bottom gold lands to the spring contacts. Four edge joints to the upright encoder PCB.'}

def units(p):
    rows=symbols[p['sid']]
    starts=[i for i,r in enumerate(rows) if r[0]=='PART']+[len(rows)]
    for a,b in zip(starts,starts[1:]):
        attrs=defaultdict(dict)
        for r in rows[a:b]:
            if r[0]=='ATTR':attrs[r[2]][r[3]]=r[4]
        yield rows[a][1],[(r,str(attrs[r[1]]['NUMBER'])) for r in rows[a:b] if r[0]=='PIN']

def schematic(group,group_parts,sheetnum):
    rows=[['DOCTYPE','SCH','1.1'],['HEAD',dict(originX=0,originY=0,version='2',maxId=100000)],
          ['FONTSTYLE','st1',None,None,None,7,None,None,None,None,None,None],
          ['FONTSTYLE','st2',None,'#24545C',None,10,None,1,None,None,None,None],
          ['FONTSTYLE','st3',None,'#27585A',None,18,None,1,None,None,None,None],
          ['LINESTYLE','st4',None,None,'none',None,None]]
    ids=count(100)
    eid=lambda:'e'+str(next(ids))
    rows.append(['TEXT',eid(),60,90,0,titles[group],'st3',0])
    rows.append(['TEXT',eid(),60,60,0,notes[group],'st2',0])
    drawings={
        'Charger':'D19 U17 U3 J1 D17 C2 R4 C3 C6 C7 C8 D18 R60 R64 R5 Q1 R14 R15 R42 R43 R2',
        'Rails':'U2 U4 U15 R52 C4 C5 C9 C10 C33 C34 R3 R7 R54 R41',
        'Crown motor':'K4 R61 Q3 R34 R35 D15 K1 R62 K3 R63 K2 R36 MT1 D16 MT2 Q2 R37 R38 C28',
        'Audio':'C20 R27 U6 L1 C21 R28 L2 D1 R26 C18 C19 C24 R29 L3 D2 C22 C23 C25',
        'Mic':'R30 U9 C26 C27 L4',
        'Rear interface':'U21 C39 C40 J2 R68',
        'Rear flex':'JF1 U301 C301 C302 C303 C304 CH1 CH2 SP1 SP2'}
    ranking=drawings.get(group,'').split()
    group_parts=sorted(group_parts,key=lambda p:ranking.index(p['ref']) if p['ref'] in ranking else 999)
    x=200;y=-30;rowheight=0
    blocks=[];stubs=defaultdict(list);segments=[]
    for p in group_parts:
        for unit,pins in units(p):
            lx=[r[4] for r,n in pins];ly=[r[5] for r,n in pins]
            width=max(lx)-min(lx)+230;height=max(ly)-min(ly)+110
            if x+width>1480:
                x=200;y-=rowheight;rowheight=0
            cx=x-min(lx);cy=y-max(ly)-40
            if p['ref']=='U1':cy-=80;height+=220
            blocks.append(box(cx+min(lx)-2,cy+min(ly)-4,cx+max(lx)+2,cy+max(ly)+4))
            blocks.append(box(cx+min(lx)-3,y-6,cx+min(lx)+max(60,len(p['part'])*4),y+24))
            comp=eid()
            rows.append(['COMPONENT',comp,unit,cx,cy,0,0,{},0])
            p.setdefault('channel',f'${sheetnum}{comp}')
            def attr(parent,k,v,px=None,py=None,angle=0,show=0,style='st1'):
                rows.append(['ATTR',eid(),parent,k,v,0,show,px,py,angle,style,0])
            attr(comp,'Symbol',p['sid'])
            attr(comp,'Device',p['did'])
            attr(comp,'Footprint',p['fp'])
            attr(comp,'Unique ID','WF_'+p['ref'])
            attr(comp,'Designator',p['ref'],cx+min(lx),y+12,0,1,'st2')
            attr(comp,'Name',(p['value'] or p['part'])+('  DNP' if p['dnp'] else ''),cx+min(lx),y,0,1)
            if p['dnp']:attr(comp,'Add into BOM','no')
            for pin,num in pins:
                n=p['nets'].get(num,'')
                if not n:continue
                px,py=cx+pin[4],cy+pin[5]
                dx,dy={0:(-1,0),90:(0,-1),180:(1,0),270:(0,1)}[pin[7]%360]
                length=5*math.ceil(max(35,len(n)*4.6+8)/5)
                ex,ey=round(px+length*dx,2),round(py+length*dy,2)
                wid=eid()
                rows.append(['WIRE',wid,[[px,py,ex,ey]],'st4',0])
                attr(wid,'NET',n,min(px,ex)+1,min(py,ey)+1,90 if dy else 0,1)
                stubs[n].append((ex,ey))
                segments.append((n,LineString([(px,py),(ex,ey)])))
            x+=width;rowheight=max(rowheight,height)
    # Join neighbouring labelled stubs in the sheet. Each branch stays clear of unrelated
    # symbols and wires; other occurrences retain their ordinary global net labels.
    candidates=[]
    for n,points in stubs.items():
        for i,a in enumerate(points):
            for b in points[i+1:]:
                length=abs(a[0]-b[0])+abs(a[1]-b[1])
                if 10<length<=320:candidates.append((length,n,a,b))
    links=defaultdict(set)
    for _,n,a,b in sorted(candidates):
        if len(links[(n,a)])>=2 or len(links[(n,b)])>=2:continue
        middle=5*round((a[0]+b[0])/10)
        for pts in ([a,(b[0],a[1]),b],[a,(a[0],b[1]),b],[a,(middle,a[1]),(middle,b[1]),b]):
            segs=[(v,w) for v,w in zip(pts,pts[1:]) if v!=w]
            path=LineString([segs[0][0]]+[s[1] for s in segs])
            if any(path.intersects(g) for g in blocks) or any(path.distance(g)<4 for netname,g in segments if netname!=n):continue
            if any(path.intersection(g).length>1e-5 for netname,g in segments if netname==n):continue
            wid=eid();rows.append(['WIRE',wid,[[*v,*w] for v,w in segs],'st4',0])
            rows.append(['ATTR',eid(),wid,'NET',n,0,0,a[0],a[1],0,'st1',0])
            segments.append((n,path));links[n,a].add(b);links[n,b].add(a)
            break
    return rows

def pcb(which,subset,pcb_id):
    allowed_header={'CANVAS','LAYER','LAYER_PHYS','ACTIVE_LAYER','RULE_TEMPLATE','RULE','RULE_SELECTOR','PRIMITIVE','SILK_OPTS','PREFERENCE'}
    rows=[['DOCTYPE','PCB','1.8'],['HEAD',{'editorVersion':'2.2.38.8','importFlag':0}]]
    rows+=copy.deepcopy([r for r in pcb_template if r[0] in allowed_header and r[0]!='LAYER_PHYS'])
    if which=='main':
        spec=[(3,0),(7,0),(5,.01),(1,.035),(361,.09),(15,.0175),(362,.14),(16,.0175),(363,.20),(17,.0175),(364,.14),(18,.0175),(365,.09),(2,.035),(6,.01),(8,0),(4,0)]
    else:
        cu=.018 if which=='rear' else .035
        core={'rear':.050,'crown':.47,'bridge':.53}[which]
        mask=.017 if which=='rear' else .01
        spec=[(3,0),(7,0),(5,mask),(1,cu),(361,core),(2,cu),(6,mask),(8,0),(4,0)]
    rows += [['LAYER_PHYS',k,'Nominal study',mil(h),0,0,1] for k,h in spec]
    for r in rows:
        if r[0]=='LAYER' and r[2]=='SIGNAL':r[4]=3 if which=='main' and 15<=r[1]<=18 else 0
    if args.reviewed_placement:
        placement=workspace.SOURCE.parent/'boards'/board_dirs[which]/'placement.ndjson'
        return rows+records(placement.read_bytes())
    allnets=sorted({n for p in subset for n in p['nets'].values()})
    rows.extend([['NET',n,None,None,1,None,0,None] for n in allnets])
    ids=count(200)
    eid=lambda:'e'+str(next(ids))
    poly=deck[which]
    for wire in [poly.exterior,*poly.interiors]:
        pts=list(wire.coords)
        # A complete circular hole is stored as a single outline object.
        b=Polygon(pts).bounds
        if wire!=poly.exterior and abs((b[2]-b[0])-(b[3]-b[1]))<.02 and abs(Polygon(pts).area-math.pi*((b[2]-b[0])/2)**2)<.05:
            path=['CIRCLE',mil((b[0]+b[2])/2),mil((b[1]+b[3])/2),mil((b[2]-b[0])/2)]
        else:
            path=[mil(pts[0][0]),mil(pts[0][1]),'L']+[mil(c) for pt in pts[1:] for c in pt]
        rows.append(['POLY',eid(),0,'',11,1,path,0])
    if which=='main':
        # Keep-out for the module's overhanging antenna. This follows the structural antenna zone.
        path=[mil(-15.5),mil(-22.2),'L',mil(2.5),mil(-22.2),mil(2.5),mil(-16.1),mil(-15.5),mil(-16.1),mil(-15.5),mil(-22.2)]
        rows.append(['POLY',eid(),0,'',13,3,path,0])
    if which=='crown':
        for peg in (-1.5,1.5):rows.append(['POLY',eid(),0,'',11,1,['CIRCLE',mil(peg),mil(-1.37),mil(.30)],0])
    for p in subset:
        cid=eid();L=1 if p['side']=='F' else 2;x=mil(p['x']);y=mil(p['y'])
        rows.append(['COMPONENT',cid,0,L,x,y,p['rot'],{'Unique ID':'WF_'+p['ref'],'Reuse Block':'','Group ID':'','Channel ID':p['channel']},0])
        for k,v in [('Footprint',p['fp']),('Designator',p['ref']),('Device',p['did'])]:
            rows.append(['ATTR',eid(),0,cid,(9 if L==1 else 10) if k=='Designator' else (3 if L==1 else 4),
                         x,y,k,v,0,1 if k=='Designator' else 0,'default',20,3,0,0,3,0,0,0,0,0])
        for pad in loadfp(p['fp']):
            if pad[0]=='PAD':rows.append(['PAD_NET',cid,pad[5],p['nets'].get(pad[5],''),pad[1]])
    return rows

def harness():
    rows=[['DOCTYPE','SCH','1.1'],['HEAD',dict(originX=0,originY=0,version='2',maxId=5000)],
          ['FONTSTYLE','st1',None,None,None,9,None,None,None,None,None,None],
          ['FONTSTYLE','st2',None,'#24545c',None,16,None,1,None,None,None,None],
          ['LINESTYLE','st3',None,None,'none',None,None],
          ['TEXT','e1',70,270,0,'11  电池线束 / 外贴测温元件','st2',0],
          ['TEXT','e2',70,237,0,'J1.3 BAT+  |  J1.2 CELL_TS  |  J1.1 PACK-     Mate 78172-0003; confirm continuity and polarity before insertion.','st1',0]]
    for i,(ref,part,nets_) in enumerate([('BT1','LP503030 450mAh PCM',{'P+':'VBAT_S','P-':'GND'}),('TH1','SEMITEC 103JT',{'T':'CELL_TS','RETURN':'GND'})]):
        sid,s=make_symbol(part,{n:n for n in nets_});symbols[sid]=s;symmeta[sid]=dict(title=part,type=2,version='1',source='',desc='')
        did=uid('harness/'+ref);devices[did]=dict(title=part,version='1',source='',attributes={'Symbol':sid,'Footprint':'','Name':part,'Designator':ref,'Convert to PCB':'no','Add into BOM':'no'})
        proj['symbols'][sid]=symmeta[sid];proj['devices'][did]=devices[did]
        cx=320+i*400;cy=135;cid='e'+str(100+i*50);rows.append(['COMPONENT',cid,part+'.1',cx,cy,0,0,{},0])
        for j,(key,val) in enumerate([('Symbol',sid),('Device',did),('Designator',ref),('Name',part),('Convert to PCB','no'),('Unique ID','WF_'+ref)]):
            rows.append(['ATTR',cid+'a'+str(j),cid,key,val,0,1 if key in ['Designator','Name'] else 0,cx-30,cy+60-j*9,0,'st1',0])
        for j,n in enumerate(nets_.values()):
            py=cy+5-j*10;wid=cid+'w'+str(j)
            rows.extend([['WIRE',wid,[[cx-45,py,cx-170,py]],'st3',0],['ATTR',wid+'a',wid,'NET',n,0,1,cx-165,py+1,0,'st1',0]])
    return rows

for which,name in names.items():
    subset=[p for p in parts.values() if p['board']==which]
    sid,bid=uid('SCH/'+which),uid('PCB/'+which)
    grouped=OrderedDict((k,[p for p in subset if p['group']==k]) for k in titles if any(p['group']==k for p in subset))
    sh=[dict(name=titles[g],id=i,uuid=uid(which+'/'+g)) for i,g in enumerate(grouped,1)]
    proj=dict(config=dict(title=name,editorVersion='2.2.38.8',cbbProject=False,defaultSheet=''),boards={name:dict(schematic=sid,pcb=bid)},
              schematics={sid:dict(name=name,sheets=sh)},pcbs={bid:name},panels={},
              devices={p['did']:devices[p['did']] for p in subset},symbols={p['sid']:symmeta[p['sid']] for p in subset},
              footprints={p['fp']:footmeta[p['fp']] for p in subset})
    with zipfile.ZipFile(out/(name+'.epro'),'w',zipfile.ZIP_DEFLATED) as z:
        for g,page in zip(grouped,sh):
            z.writestr(f"SHEET/{sid}/{page['id']}.esch",encode(schematic(g,grouped[g],page['id'])))
        if which=='main':
            sh.append(dict(name='11  电池线束',id=11,uuid=uid('harness/sheet')))
            z.writestr(f"SHEET/{sid}/11.esch",encode(harness()))
        z.writestr('project.json',json.dumps(proj,ensure_ascii=False,indent=2).encode())
        z.writestr('PCB/'+bid+'.epcb',encode(pcb(which,subset,bid)))
        for key in proj['symbols']:z.writestr('SYMBOL/'+key+'.esym',encode(symbols[key]))
        for key in proj['footprints']:z.writestr('FOOTPRINT/'+key+'.efoo',encode(loadfp(key)))
    print(which,'=>',out/(name+'.epro'))
