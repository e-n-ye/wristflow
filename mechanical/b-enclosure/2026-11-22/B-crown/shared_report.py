"""Compare real solderable board material, assembly obstacles and mounting height on both faces."""
from os import environ
from pathlib import Path
import sys,json
from shared_edge import App,Part,V,block,FROM
from layout_space import shadow,footprint,largest_rectangle,GRID_X,GRID_Y,PITCH
from shapely.geometry import Polygon,box as rect
import shapely as sh
import numpy as np
from PIL import Image,ImageDraw,ImageFont

out=Path(sys.argv[1]);doc=App.openDocument(str((out/'WristFlow-B-Shared-Recess.FCStd').resolve()))
prior=App.openDocument(str(FROM))
def polygon(g):
    top=max((f for f in g.Faces if f.BoundBox.ZLength<1e-6 and abs(f.BoundBox.ZMin-12.10)<1e-6),key=lambda f:f.Area)
    def loop(w):return [(p.x,p.y) for p in w.discretize(Deflection=.002)]
    return Polygon(loop(top.OuterWire),[loop(w) for w in top.Wires if not w.isSame(top.OuterWire)])
def calculate(d,fresh):
    groups='Fixed Circuit Fasteners Cell Mated Lid Links Display CrownAssembly Tools Jig Meter FreeHarness'.split()
    shapes={o.Name:o.Shape for g in groups for o in d.getObject(g).Group if o.Name!='Board'}
    shapes['Separator']=d.Separator.Shape
    pcb=polygon(d.Board.Shape)
    RF=rect(-25,-25,25,-14.95) if fresh else rect(11.1,-25,25,25)
    # Photo-estimated parts have no fitted Z. Reserve the same footprints on the display face in both comparisons.
    unmeasured=sh.union_all([rect(*b) for b in [(-18,-14,-8.5,-1.5),(-11.5,-12,5,0),(-18,-4,-8.5,11),(-17,9.5,-7,17),(-8,-21,10,-9)]])
    projections={}
    def xy(k):
        if k not in projections:projections[k]=shadow(shapes[k])
        return projections[k]
    def clip(k,z0,z1):
        s=shapes[k];b=s.BoundBox;l=max(b.ZMin,z0);h=min(b.ZMax,z1)
        if h-l<.00002:return Polygon()
        if l<=b.ZMin+.00001 and h>=b.ZMax-.00001:return xy(k)
        return shadow(s.common(block(-32,-32,l,64,64,h-l)))
    def grouped(keys):return sh.union_all([xy(k) for k in keys],grid_size=.0001)
    banks=[grouped([k for k in shapes if k.startswith(pre)]).envelope for pre in ['LPad','DBG','Sleeve']]
    held_rear=sh.union_all(banks+[grouped(['RearRun','Crossing','Stiffener','FlexPlug','CN1','CN1Access','FeedSpace']),
        grouped([k for k in shapes if k.startswith(('Lead','FreeLead'))]).envelope])
    held_front=sh.union_all([unmeasured,grouped(['FeedSpace','Crossing'])])
    X,Y=np.meshgrid(GRID_X+PITCH*np.arange(148),GRID_Y+PITCH*np.arange(174))
    tiles=sh.box(X,Y,X+PITCH,Y+PITCH)
    data={};maps={};large={}
    for side,heights in [('front',(1.,2.,3.)),('rear',(1.,2.,3.))]:
        data[side]={};maps[side]={};large[side]={}
        for h in heights:
            cut=[held_front if side=='front' else held_rear]
            for key in shapes:
                if side=='front':z0,z1=12.10,12.10+h+.20
                else:z0,z1=11.30-h-.20-(.30 if key in ('Battery','Separator') else 0),11.30
                cut.append(clip(key,z0,z1))
            allowed=pcb.buffer(-.30).difference(sh.union_all(cut,grid_size=.0001).buffer(.22)).difference(RF)
            sh.prepare(allowed);mask=sh.covers(allowed,tiles)
            data[side][h]=round(float(np.sum(mask)*.0625),2);maps[side][h]=footprint(mask);large[side][h]=largest_rectangle(mask)
    air=xy('Antenna')
    return dict(board_mm2=round(pcb.area,2),areas_mm2=data,rect_xywh=large,
                antenna_mm2=round(air.area,2),screen_antenna_xy_mm2=round(air.intersection(xy('Screen')).area,2),
                cell_antenna_xy_mm2=round(air.intersection(xy('Battery')).area,2),
                glass_antenna_gap_mm=round(shapes['Antenna'].distToShape(shapes['Screen'])[0],3)),maps,pcb
old,_,_=calculate(prior,False);new,maps,pcb=calculate(doc,True)
delta={'previous':old,'trial':new,'basis':'0.25 mm full tiles, board edge 0.30, lateral clearance 0.22, vertical 0.20, extra cell/insulator reserve 0.30; keep fixture/service/contact banks, photo-uncertain front zones and antenna-side edge strip out. Geometric screen only, not PCB routing.'}
(out/'hardware-area.json').write_text(json.dumps(delta,indent=2,ensure_ascii=False),encoding='utf8')
facts=json.loads((out/'layout-check.json').read_text('utf8'))
paper='#fbfaf7';ink='#203d3e';mut='#526767';im=Image.new('RGB',(1740,1390),paper);draw=ImageDraw.Draw(im)
def t(x,y,s,size=22,c=ink,b=False,a='lm'):
    font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),size)
    draw.text((x,y),s,font=font,fill=c,anchor=a)
def panel(x,y,X,Y):draw.rounded_rectangle((x,y,X,Y),20,fill='#eff3ef',outline='#d0dfd9',width=2)
def polys(g):
    if g.is_empty:return []
    if g.geom_type=='Polygon':return [g]
    return [p for sub in getattr(g,'geoms',[]) for p in polys(sub)]
def fill(g,T,col,edge=None):
    for p in polys(g):
        ext=[T(x,y) for x,y in p.exterior.coords];hs=[[T(x,y) for x,y in r.coords] for r in p.interiors]
        m=Image.new('1',im.size);d=ImageDraw.Draw(m);d.polygon(ext,fill=1)
        for h in hs:d.polygon(h,fill=0)
        im.paste(col,mask=m)
        if edge:
            draw.line(ext,fill=edge,width=2)
            for h in hs:draw.line(h,fill=edge,width=2)
def legend(x,y,col,label):
    draw.rounded_rectangle((x,y-8,x+24,y+8),4,fill=col);t(x+33,y,label,17)
def boundary(points,T,col):
    p=[np.array(T(x,y)) for x,y in points]
    for a,b in zip(p,p[1:]+p[:1]):
        length=np.linalg.norm(b-a)
        if length<1:continue
        direction=(b-a)/length
        for i in range(0,int(length),11):
            draw.line([tuple(a+i*direction),tuple(a+min(i+6,length)*direction)],fill=col,width=2)
def section_y(y,keys,T):
    p=Part.Face(Part.makePolygon([V(x,y,z) for x,z in [(-21,.5),(24,.5),(24,20),(-21,20),(-21,.5)]]))
    for key,col in keys:
        for f in doc.getObject(key).Shape.common(p).Faces:
            def ring(w):return [(a.x,a.z) for a in w.discretize(Deflection=.012)]
            fill(Polygon(ring(f.OuterWire),[ring(w) for w in f.Wires if not w.isSame(f.OuterWire)]).buffer(0),T,col,'#566762')
t(42,53,'屏侧纵放模组，天线和电机共用下沿缺口',41,b=True)
t(45,112,'同一套表身可翻转佩戴。以下都按屏幕正视坐标绘制，虚线表示结构投影。',22,mut)
panel(32,154,845,852);panel(866,154,1707,852)
t(58,197,'屏幕侧装配',30,b=True);t(891,197,'电池侧可摆器件区域',30,b=True)
t(58,240,'浅绿为可新增 3 mm 器件的位置；已移开屏幕和屏尾。',19,mut);t(891,240,'与左图同向，虚线为成品电池的最大投影。',19,mut)
T=lambda x,y:(420+x*11.65,534-y*11.65)
U=lambda x,y:(1265+x*11.65,534-y*11.65)
fill(pcb,T,'#518676','#2b5551')
fill(maps['front'][3.],T,'#bad7ac')
for key,col in [('ModuleSubstrate','#8caba7'),('ModuleParts','#526f7c'),('Antenna','#c5684e'),('MotorSeat','#a99db3'),('Motor','#7d648f'),
                ('MotorWire0','#cf8b4d'),('MotorWire1','#4c5455'),('Crown','#819495'),('Clamp1','#b9c6c1')]:
    fill(shadow(doc.getObject(key).Shape),T,col,'#4f5c60')
t(*T(-6.5,-4.5),'思澈模组',22,'white',True,'mm');t(*T(-6.5,-18),'天线',19,'white',True,'mm');t(*T(12.1,-16),'电机',19,'white',True,'mm')
t(*T(-2.45,10.05),'穿线窗',15,mut,True,'mm')
fx,fy,fw,fl=new['rect_xywh']['front'][3.]
boundary([(fx,fy),(fx+fw,fy),(fx+fw,fy+fl),(fx,fy+fl)],T,'#638663')
t(*T(fx+fw/2,fy+fl/2),f'{fw:g} × {fl:g}',14,'#375c46',True,'mm')
fill(pcb,U,'#d0d8d4','#506761')
colors={1.:'#b9ce83',2.:'#5ba996',3.:'#357aa2'}
for h in (1.,2.,3.):fill(maps['rear'][h],U,colors[h])
bb=doc.Battery.Shape.BoundBox
for p1,p2 in [((bb.XMin,bb.YMin),(bb.XMax,bb.YMin)),((bb.XMax,bb.YMin),(bb.XMax,bb.YMax)),
            ((bb.XMax,bb.YMax),(bb.XMin,bb.YMax)),((bb.XMin,bb.YMax),(bb.XMin,bb.YMin))]:
    a=np.array(U(*p1));b=np.array(U(*p2));L=np.linalg.norm(b-a);v=(b-a)/L
    for i in range(0,int(L),13):draw.line([tuple(a+i*v),tuple(a+min(i+7,L)*v)],fill='#5e6454',width=2)
legend(59,815,'#c5684e','天线下方掏空');legend(292,815,'#7d648f','电机下沉穿过板厚');legend(559,815,'#b9c6c1','迁移后的固定点')
for x,h in [(891,1.),(1050,2.),(1217,3.)]:legend(x,815,colors[h],f'可摆 {h:g} mm')
t(1410,815,'虚线为电池投影',17,mut)
panel(32,873,845,1355);panel(866,873,1707,1355)
t(58,917,'沿表冠中心剖开',28,b=True)
Tsec=lambda x,z:(79+(x+21)*15.72,1280-z*15.72)
section_y(0,[('Frame','#acc1bb'),('Screen','#495d62'),('Board','#4f8c73'),('Tray','#789294'),('Battery','#c0d1a4'),
             ('ModuleSubstrate','#93aaa5'),('ModuleParts','#526f7c'),('FrontRun','#ce9357'),('Daughter','#528f78'),('EncoderBody','#a26d92'),
             ('CrownShaft','#d39e56'),('GuideSleeve','#50a5aa'),('Crown','#738990')],Tsec)
t(*Tsec(-3,5.25),'450 mAh',18,'#456143',True,'mm');t(56,1322,'表冠居中，中框侧壁平直。屏幕、贴合台阶和排线一起抬高 2.80 mm。',18,mut)
t(891,917,'板面空间比较',28,b=True);t(891,965,'焊后高度',18,mut);t(1107,965,'原屏侧 → 新屏侧',17,mut);t(1392,965,'原电池侧 → 新电池侧',17,mut)
for i,h in enumerate((1.,2.,3.)):
    y=1017+i*60;t(894,y,f'{h:g} mm',22,b=True)
    for x,side in [(1111,'front'),(1395,'rear')]:
        t(x,y,f'{old["areas_mm2"][side][h]:.0f} → {new["areas_mm2"][side][h]:.0f} mm²',19)
r=new['rect_xywh']['rear'][2.]
if r:t(891,1212,f'电池侧 2 mm 档连续矩形可达 {r[2]:g} × {r[3]:g} mm。',19,b=True)
t(891,1256,'按实际板材筛选，已扣除孔、工具通道、接触区和天线端预留。',17,mut)
t(891,1300,'表内是几何试排面积。实际布线、射频和量产公差仍需验证。',17,mut)
im.save(out/'shared-layout.png',optimize=True)
App.closeDocument(doc.Name);App.closeDocument(prior.Name)
