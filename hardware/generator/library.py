from circuit import *
import math
from shapely import affinity
from shapely.geometry import box, Polygon, Point
from shapely.ops import unary_union
import sqlite3
from workspace import LIBRARY

extra_symbols={}
db=sqlite3.connect(LIBRARY.as_uri()+'?mode=ro',uri=True)
for ref,title in [('J2','fh34srj-10s-0.5sh(50)'),('U21','xc6219b182mr'),('R60','0402wgf1132tce'),('R64','0402wgf1623tce')]:
    did,display=db.execute('select uuid,display_title from devices where title=?',(title,)).fetchone()
    attrs=dict(db.execute('select key,value from attributes where device_uuid=?',(did,)))
    catalog['devices'][did]=dict(title=display,attributes=attrs,source='',version='1',description='')
    for role in ['Footprint','Symbol']:
        k=attrs[role]
        name,content=db.execute('select display_title,dataStr from components where uuid=?',(k,)).fetchone()
        meta=dict(title=name,source='',version='1',type=4 if role=='Footprint' else 2,desc='')
        if role=='Footprint':libs[k]=records(content);catalog['footprints'][k]=meta
        else:extra_symbols[k]=records(content);catalog['symbols'][k]=meta
    parts[ref].update(device=did,fp=attrs['Footprint'],part=attrs['Manufacturer Part'],status='立创离线系统库')
parts['J2']['nets'].pop('MP')
parts['J2']['nets'].update({'11':'','12':''})
parts['J1']['nets'].update({'1':'GND','3':'VBAT_S'})
db.close()

MM=.0254
def mil(x):return round(x/MM,5)
def built_land(key,w,h,pads):
    fid=uid('land/'+key)
    fp=[['DOCTYPE','FOOTPRINT','1.8'],['HEAD',{'uuid':fid,'title':key,'editorVersion':'2.2.38.8'}]]
    fp+= [r for r in loadfp(parts['U4']['fp']) if r[0] in ('LAYER','ACTIVE_LAYER','CANVAS')]
    path=[mil(-w/2),mil(-h/2),'L',mil(w/2),mil(-h/2),mil(w/2),mil(h/2),mil(-w/2),mil(h/2),mil(-w/2),mil(-h/2)]
    fp.append(['POLY','e1',0,'',48,2,path,0])
    for i,(num,x,y,width,height,*style) in enumerate(pads,10):
        shp=style[0] if style else 'RECT'
        fp.append(['PAD','e'+str(i),0,'',1,str(num),mil(x),mil(y),0,None,
                   [shp,mil(width),mil(height)]+([0] if shp=='RECT' else []),[],0,0,None,1,0,None,None,None,None,0,None,None,None,None,[]])
        if key in ('pogo','fingers','gold','motor','bridge','flexland'):
            fp[-1][19:21]=[-3937.008,-3937.008]
    libs[fid]=fp
    for p in parts.values():
        if p['fp']==key:p['fp']=fid

built_land('spring',3,1.5,[('1',-1.225,0,.55,1.5),('2',.25,0,1.7,1.5)])
for i in range(1,5):
    p=parts['K'+str(i)]
    p['nets']['2']=p['nets']['1']
built_land('battery',5.4,4.53,[(i+1,x,2.145,.6,.85) for i,x in enumerate((-1.2,0,1.2))]+[('MP',x,-1.63,.7,.8) for x in (-2.35,2.35)])
# R2 project variant of the vendor library. Use manufacturer's mounting pattern dimensions.
hf=loadfp(parts['J2']['fp'])
for r in hf:
    if r[0]=='PAD':
        pin=int(r[5])
        r[6:8]=[mil(2.25-.5*(pin-1)),mil(1.25)] if pin<=10 else [mil(-3.25 if pin==11 else 3.25),mil(-1.25)]
        r[10]=['RECT',mil(.30 if pin<=10 else .40),mil(.8),0]
    if r[0]=='POLY' and r[4]==48:
        r[6]=[mil(-3.5),mil(-1.6),'L',mil(-3.5),mil(2.2),mil(3.5),mil(2.2),mil(3.5),mil(-1.6),mil(-3.5),mil(-1.6)]
parts['J2']['status']='立创 C324723 库，焊盘按 HRS 图第4页修订'
built_land('motor',.76,1,[(1,0,0,.76,1)])
built_land('pogo',1.14,1.14,[(1,0,0,1.14,1.14,'ELLIPSE')])
built_land('bridge',2.82,1.02,[(1,.9,0,1.02,1.02,'ELLIPSE'),(2,-.9,0,1.02,1.02,'ELLIPSE')])
built_land('joint',.75,1,[(1,0,0,.75,1)])
built_land('gold',2.76,1.66,[(1,0,0,2.76,1.66)])
built_land('fingers',5.5,2.5,[(i+1,(i-4.5)*.5,0,.3,1.5) for i in range(10)])
built_land('flexland',1.1,.95,[(1,0,0,1.1,.95)])
built_land('encoder',4.84,3.9,[('A',-1.5,-2.97,.80,.60),('C',0,-2.97,.80,.60),('B',1.5,-2.97,.80,.60),
                                 ('S1',-.75,-1.97,.50,.80),('S2',.75,-1.97,.50,.80),('MP',-2.55,.71,1.3,.95),('MP',2.55,.71,1.3,.95)])
built_land('ppg',3.3,5.6,[(i+1,-1.2,(3-i)*.8,.75,.35) for i in range(7)]+[(14-i,1.2,(3-i)*.8,.75,.35) for i in range(7)])
parts['U301']['status']='SparkFun MAX30102 / OLGA-14 库，ADI 管脚'

def padshape(r):
    x,y=r[6]*MM,r[7]*MM
    if r[10][0]=='POLYGON':
        v=[n*MM for n in r[10][1] if not isinstance(n,str)]
        return Polygon(list(zip(v[::2],v[1::2]))).buffer(0)
    kind,w,h,*other=r[10]
    w,h=w*MM,h*MM
    shape=box(-w/2,-h/2,w/2,h/2)
    if kind=='ELLIPSE':shape=affinity.scale(Point(0,0).buffer(1),w/2,h/2)
    return affinity.translate(affinity.rotate(shape,r[8] or 0,origin=(0,0)),x,y)
def outline(fp):
    shapes=[]
    for r in fp:
        if r[0]=='POLY' and r[4]==48:
            path=r[6]
            if isinstance(path[0],(float,int)) and set(i for i in path if isinstance(i,str))=={'L'}:
                v=[i*MM for i in path if not isinstance(i,str)]
                if len(v)>=6:
                    poly=Polygon(list(zip(v[::2],v[1::2])))
                    if poly.is_valid:shapes.append(poly)
            elif path[0]=='R':
                x,y,w,h=path[1:5];shapes.append(box(x*MM,(y-h)*MM,(x+w)*MM,y*MM))
    pads=[padshape(r) for r in fp if r[0]=='PAD']
    return unary_union(shapes).envelope if shapes else unary_union(pads).envelope
def transform(g,x,y,angle,side):
    if side=='B':g=affinity.scale(g,xfact=1,yfact=-1,origin=(0,0))
    g=affinity.rotate(g,angle,origin=(0,0))
    return affinity.translate(g,x,y)
height_limits={'U1':3.1,'U2':.65,'U3':.65,'U4':1.45,'U6':1.10,'U9':1.05,'U13':1.05,'U14':.65,'U15':1.45,
               'U16':.8,'U17':.85,'U19':.65,'U21':1.45,'J1':1.55,'J2':1.,'J201':.72,'D19':1.35,'Q3':1.2,
               'ENC1':3.2,'U301':1.55,'JF1':.3}
for p in parts.values():
    f=loadfp(p['fp'])
    p['body']=outline(f)
    padrows=[r for r in f if r[0]=='PAD']
    p['padshapes']=[(r[5],padshape(r)) for r in padrows]
    p['env']=unary_union([p['body']]+[g for _,g in p['padshapes']]).envelope
    ref=p['ref']
    if ref.startswith(('MT','TP','SP','CH','JC','P')):h=.08 if not ref.startswith('MT') else .7
    elif ref.startswith('K'):h=1.0
    elif ref in ('R49','R50','JP1'):h=.77
    else:h=.95 if p['env'].bounds[2]-p['env'].bounds[0]>1.7 else .65
    p['height']=height_limits.get(ref,h)

def make_symbol(part,number_names):
    sid=uid('symbol/'+part)
    template=records(archive.read('SYMBOL/'+catalog['devices'][parts['U9']['device']]['attributes']['Symbol']+'.esym'))
    rows=template[:next(i for i,r in enumerate(template) if r[0]=='PART')]
    half=max(15,5*len(number_names)+5)
    rows += [['PART',part+'.1',{'BBOX':[-35,-half,35,half]}],['ATTR','e1','','Symbol',part,False,False,None,None,0,'st3',0],
             ['ATTR','e2','','Designator','U?',False,False,None,None,0,'st3',0],['RECT','e3',-35,-half,35,half,0,0,0,'st1',0]]
    for i,(num,label) in enumerate(number_names.items()):
        n=5+i*4;y=half-10-i*10
        rows.extend([['PIN','e'+str(n),1,None,-45,y,10,0,None,0,0,1],
                     ['ATTR','e'+str(n+1),'e'+str(n),'NAME',label,False,True,-32,y-4,0,'st3',0],
                     ['ATTR','e'+str(n+2),'e'+str(n),'NUMBER',num,False,True,-40,y+1,0,'st4',0],
                     ['ATTR','e'+str(n+3),'e'+str(n),'Pin Type','Passive',False,False,None,None,0,'st2',0]])
    return sid,rows

symbols={}
devices={}
footmeta={}
symmeta={}
for p in parts.values():
    if p['device']:
        original=copy.deepcopy(catalog['devices'][p['device']])
    elif p.get('symbol_template'):
        original=copy.deepcopy(catalog['devices'][p['symbol_template']])
        original['title']=p['part']
        original['attributes']={k:v for k,v in original['attributes'].items() if k in ('Symbol','Designator','Name','Add into BOM','Convert to PCB')}
        original['attributes'].update({'Manufacturer Part':p['part'],'Value':p['value'],'Name':p['part']})
    else:
        names={n:n for n in p['nets']}
        if p['ref']=='U301':names=dict(zip(names,['NC','SCL','SDA','PGND','NC','NC','NC','NC','VLED+','VLED+','VDD','GND','INT#','NC']))
        sid,s=make_symbol(p['part'],names)
        symbols[sid]=s
        symmeta[sid]=dict(title=p['part'],type=2,version='1',source='',desc='')
        original=dict(title=p['part'],attributes={'Symbol':sid,'Name':p['part'],'Manufacturer Part':p['part'],'Add into BOM':'yes','Convert to PCB':'yes','Designator':'U?'},version='1',source='',description='')
    oldfp=p['fp']
    if '_' in oldfp:
        # Round-trippable project library copy of the exact on-board modified footprint.
        fid=uid('reference override/'+oldfp)
        if fid not in libs:
            libs[fid]=copy.deepcopy(loadfp(oldfp))
            libs[fid][1][1]['uuid']=fid
        p['fp']=fid
    sid=original['attributes']['Symbol'];p['sid']=sid
    if sid not in symbols:
        symbols[sid]=extra_symbols[sid] if sid in extra_symbols else records(archive.read('SYMBOL/'+sid+'.esym'));symmeta[sid]=catalog['symbols'][sid]
    original['attributes']['Footprint']=p['fp']
    did=uid(p['part']+p['fp']+sid);p['did']=did;devices[did]=original
    if p['fp'] not in footmeta:
        footmeta[p['fp']]=catalog['footprints'].get(p['fp'],dict(title=next((r[1].get('title') for r in loadfp(p['fp']) if r[0]=='HEAD' and r[1].get('title')),p['part']),type=4,version='1',source='',desc=''))
