"""Registered sections from the saved BRep, plus a battery-side projection."""
from os import environ
import sys, math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from crown_fit import App, Part, V, Y
from shapely.geometry import Polygon
from shapely.ops import unary_union

dst=Path(sys.argv[1]);doc=App.openDocument(str((dst/'WristFlow-B-Crown.FCStd').resolve()))
W,H=1840,1390
img=Image.new('RGB',(W,H),'#faf9f5');d=ImageDraw.Draw(img)
ink='#183739';mut='#506969';bg='#f1f4ef';outline='#364e50';green='#296d58'
colors={'Back':'#becbc9','Board':'#3d876a','Frame':'#a2b9b5','MotorSeat':'#7a9192','Motor':'#687c83',
        'Daughter':'#3d876a','EncoderBody':'#a96390','EncoderRotor':'#d9a8c2','Crown':'#6e8790',
        'CrownShaft':'#e8ab4e','GuideSleeve':'#51aeb9','Retainer':'#80a7ac','CrownFlex':'#e8a43d',
        'FlexStiffener':'#9679ac','CrownZIF':'#4c7a97','GoldFingers':'#e7bd63','ZIFContacts':'#d8a948',
        'Battery':'#d8e0ca','Screen':'#d2d8d7','EncoderPins':'#879496','RetainerScrews':'#566b73',
        'SwitchFollower':'#b96348'}
def t(x,y,s,size=22,c=ink,b=False,anchor='lm'):
    f=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),size)
    d.text((x,y),s,font=f,fill=c,anchor=anchor)
def line(pts,c=outline,w=2):
    d.line(pts,fill=c,width=w,joint='curve')
def panel(rect):
    d.rounded_rectangle(rect,22,fill=bg,outline='#dae3dd',width=2)
def tag(x,y,n,dx,dy):
    line([(x,y),(dx,dy)],'#2f5050',2);d.ellipse((dx-15,dy-15,dx+15,dy+15),fill='#22494b',outline='white',width=2)
    t(dx,dy,str(n),17,'white',True,'mm')
def meshpts(wire):
    return wire.discretize(Deflection=.009)
def native(face,proj):
    f=Polygon([proj(p) for p in meshpts(face.OuterWire)],
              [[proj(p) for p in meshpts(w)] for w in face.Wires if not w.isSame(face.OuterWire)])
    return f if f.is_valid else f.buffer(0)
def polys(geo):
    if geo.is_empty:return []
    return [geo] if geo.geom_type=='Polygon' else [p for a in getattr(geo,'geoms',[]) for p in polys(a)]
def paint(shapes,col,xy,mask_outline=True):
    for p in shapes:
        ring=[xy(a,b) for a,b in p.exterior.coords];holes=[[xy(a,b) for a,b in q.coords] for q in p.interiors]
        m=Image.new('1',img.size);md=ImageDraw.Draw(m);md.polygon(ring,fill=1)
        for hole in holes:md.polygon(hole,fill=0)
        img.paste(col,mask=m)
        if mask_outline:
            line(ring,outline,1)
            for hole in holes:line(hole,outline,1)
def section(y,spec,x0,x1,xleft,zbase,scale):
    def xy(x,z):return xleft+(x-x0)*scale,zbase-z*scale
    plane=Part.Face(Part.makePolygon([V(x,y,z) for x,z in [(x0,0),(x1,0),(x1,13.1),(x0,13.1),(x0,0)]]))
    for name in spec:
        sh=doc.getObject(name).Shape; cut=sh.common(plane)
        for face in cut.Faces:paint(polys(native(face,lambda p:(p.x,p.z))),colors[name],xy)
    return xy
def shadow(key):
    sh=doc.getObject(key).Shape
    vs,ts=sh.tessellate(.035)
    items=[]
    for a,b,c in ts:
        p=Polygon([(vs[i].x,vs[i].y) for i in (a,b,c)])
        if p.area>1e-9:items.append(p)
    return polys(unary_union(items))
def dotted(a,b,col='#73908d',length=10):
    x,y=a;dx,dy=b[0]-x,b[1]-y;n=math.hypot(dx,dy)
    for i in range(0,int(n),length*2):
        j=min(n,i+length);line([(x+dx*i/n,y+dy*i/n),(x+dx*j/n,y+dy*j/n)],col,2)
def key(x,y,n,label):
    d.ellipse((x-12,y-12,x+12,y+12),fill='#22494b');t(x,y,str(n),15,'white',True,'mm');t(x+22,y,label,20)

t(46,56,'表冠轴怎样接到主板',46,ink,True)
t(49,116,'WristFlow B   /   BMQ22-N1A1-2012-A   /   保留 503035T 电池和原有主板',24,mut)
panel((36,163,909,864));panel((933,163,1804,864))
t(62,207,'A—A   沿表冠轴剖开',31,ink,True);t(62,254,'Y = 15.20 mm     电机在上，后盖在下，右侧朝壳外',20,mut)
t(960,207,'B—B   剖开主板插座',31,ink,True);t(960,254,'Y = 18.75 mm     穿过一个实际接点所在的位置',20,mut)
A=section(Y,['Screen','Board','Back','Frame','Retainer','MotorSeat','Motor','Daughter','EncoderBody','EncoderRotor','SwitchFollower','CrownFlex','CrownShaft','GuideSleeve','Crown'],6.8,24.3,163,739,34)
B=section(18.75,['Board','Back','MotorSeat','Motor','Frame','Retainer','Daughter','CrownFlex','FlexStiffener','CrownZIF','GoldFingers','ZIFContacts'],.3,16.,1091,739,34)
for xy,name,x,z in [(A,'电机',12.0,9.65),(B,'电机',13.15,9.68),(B,'主板',3.37,11.72)]:
    t(*xy(x,z),name,17,'white',True,'mm')
for xy,n,x,z,ox,oz in [(A,1,18.43,5.35,18.4,3.55),(A,2,15.6,6.58,16.25,7.45),(A,3,13.82,4.72,12.6,5.4),
                         (B,4,7.35,5.05,6.38,5.6),(B,5,4.4,10.67,4.30,9.23),(B,6,2.84,11.69,2.05,12.57)]:
    tag(*xy(x,z),n,*xy(ox,oz))
key(84,790,1,'圆轴转方榫');key(334,790,2,'段落编码器');key(608,790,3,'补强小板')
key(980,790,4,'表冠专用 FPC');key(1244,790,5,'6P 翻盖座');key(1512,790,6,'0.80 mm 主板')
t(66,831,'蓝绿色为中框导套。表冠向左按入，方榫沿同一轴线传力。',18,mut)
t(960,831,'橙色排线上翻后向左插入蓝色座，尾端补强至 0.30 mm。',18,mut)

panel((36,888,888,1354));panel((912,888,1804,1354))
t(63,929,'从后盖看两处剖切位置',27,ink,True)
t(63,969,'后盖已移开，绿色为原电池投影。',18,mut)
def top(x,y):return 310-x*7.25,1155-y*7.25
for name,col in [('Board','#b9cebf'),('Battery','#cddabc'),('MotorSeat','#9aafad'),('Motor','#7c9094'),
                 ('Daughter',colors['Daughter']),('EncoderBody',colors['EncoderBody']),('Crown',colors['Crown']),
                 ('CrownZIF',colors['CrownZIF']),('CrownFlex',colors['CrownFlex'])]:
    paint(shadow(name),col,top)
t(*top(-5,-5),'原电池',17,'#456549',True,'mm')
for yy,s in [(Y,'A'),(18.75,'B')]:
    xx,z=top(24.5,yy);dotted((xx-4,z),(380,z),'#365d61',7);t(xx-12,z,s,16,ink,True,'rm')
for row,legend in enumerate([('电机下方','#7c9094'),('编码器','#a96390'),('表冠排线','#e8a43d'),('主板插座','#4c7a97')]):
    name,c=legend;y=1040+row*48;d.rounded_rectangle((525,y-8,548,y+8),4,fill=c);t(560,y,name,19)
t(514,1260,'A、B 相距 3.55 mm。',19,ink,True);t(514,1301,'两图的上下方向一致。',18,mut)
t(942,929,'本次试装尺寸',27,ink,True)
def metric(y,s,v):
    t(947,y,s,20,mut);t(1768,y,v,22,ink,True,'rm')
for y,s,v in [(992,'表冠轴心沿同侧向表带移动','15.20 mm'),(1046,'表冠轴心向后盖移动','2.65 mm'),
              (1100,'编码器本体至电芯 / 小板至电芯','2.68 / 1.60 mm'),(1154,'排线至后盖最小 / 小板至电机座','0.28 / 0.35 mm'),
              (1208,'按入 0.25 mm 后，冠帽至中框','0.13 mm')]:metric(y,s,v)
line([(947,1245),(1765,1245)],'#c4d4cd')
t(947,1280,'轴端插深、按键触发和防水密封待实物确认。',19,ink)
t(947,1318,'剖面取自名义装配。编码器内部和插座接点作简化表达。',18,mut)
img.save(dst/'crown-sections.png',optimize=True)
App.closeDocument(doc.Name)
