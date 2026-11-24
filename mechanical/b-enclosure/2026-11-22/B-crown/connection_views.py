"""Scaled assembly drawings for direct contacts, two separate light paths, and the service loop."""
from os import environ
from pathlib import Path
import sys,json
from PIL import Image,ImageDraw,ImageFont
from shapely.geometry import Polygon
import numpy as np
from routing_review import App,Part,V,shadow,BASE,routed_flex

dst=Path(sys.argv[1]);doc=App.openDocument(str((dst/'WristFlow-B-Connections.FCStd').resolve()))
old=App.openDocument(str(BASE));q=json.loads((dst/'connection-check.json').read_text('utf8'))
ink='#203b3f';mut='#4d696c';bg='#fbfaf6';canvas='#eff3ef';border='#cddcd7'
cols={'Frame':'#aec0bb','Back':'#c8d4d1','Board':'#548471','ModuleSubstrate':'#9eafa9','ModuleParts':'#57717b','Antenna':'#bd6b52',
      'Motor':'#836c90','Crown':'#70868d','Daughter':'#378372','CrownBridge':'#378372','CrownLeaf2':'#bd8336','CrownPad2':'#ddb45c',
      'EncoderBody':'#a36891','EncoderRotor':'#c5a2bb','CrownShaft':'#d7a052','GuideSleeve':'#5b9ba2','Retainer':'#75969b',
      'AmbientALS':'#265b5e','ALSPipe':'#63b9be','ALSLens':'#7fc7c9','ALSBaffle':'#526b6d','Screen':'#86999a','ActiveArea':'#25363b',
      'LidFlexNew':'#d09a4a','ChargeBranch':'#bd8263','SpeakerBranch':'#9175a1','Speaker':'#7c768e','PpgBoard':'#598d79','OpticalDice':'#2c6e73',
      'ChargeTab0':'#c3a04d','ChargeTab1':'#c3a04d','RearSocket':'#4e798b','RearFingers':'#d4ac58','LidEnd':'#a78baa','Battery':'#c2d0a8','Tray':'#779199',
      'Baffle':'#475b5e','Optic0':'#5fabb4','Optic1':'#5fabb4','Optic2':'#5fabb4','Contact0':'#c9a852','Contact1':'#c9a852',
      'FrontRun':'#c18a4d','MotorSeat':'#aa9cb4'}
def init():
    global im,d
    im=Image.new('RGB',(1680,1150),bg);d=ImageDraw.Draw(im)
def t(x,y,s,n=21,c=ink,b=False,a='lm'):
    f=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),n)
    d.text((x,y),s,font=f,fill=c,anchor=a)
def panel(x,y,X,Y):d.rounded_rectangle((x,y,X,Y),18,fill=canvas,outline=border,width=2)
def polys(g):
    if g.is_empty:return []
    if g.geom_type=='Polygon':return [g]
    return [p for a in getattr(g,'geoms',[]) for p in polys(a)]
def paint(g,T,c,only=False,w=2):
    for p in polys(g):
        outer=[T(a,b) for a,b in p.exterior.coords];holes=[[T(a,b) for a,b in r.coords] for r in p.interiors]
        if not only:
            m=Image.new('1',im.size);md=ImageDraw.Draw(m);md.polygon(outer,fill=1)
            for h in holes:md.polygon(h,fill=0)
            im.paste(c,mask=m)
        d.line(outer,fill=c if only else '#506464',width=w)
        for h in holes:d.line(h,fill=c if only else '#506464',width=1)
def plan(keys,T,style=None):
    for k in keys:paint(shadow(doc.getObject(k).Shape),T,(style or {}).get(k,cols[k]))
def drawcut(keys,axis,at,limits,T):
    lo,hi,z0,z1=limits
    pt=lambda a,z:V(at,a,z) if axis=='x' else V(a,at,z)
    plane=Part.Face(Part.makePolygon([pt(a,z) for a,z in [(lo,z0),(hi,z0),(hi,z1),(lo,z1),(lo,z0)]]))
    for k,shape,c in keys:
        for f in shape.common(plane).Faces:
            def wire(w):return [(p.y if axis=='x' else p.x,p.z) for p in w.discretize(Deflection=.011)]
            paint(Polygon(wire(f.OuterWire),[wire(w) for w in f.Wires if not w.isSame(f.OuterWire)]).buffer(0),T,c,w=1)
def section(keys,ax,at,limits,T):
    drawcut([(k,doc.getObject(k).Shape,cols[k]) for k in keys],ax,at,limits,T)
def dash(a,b,c):
    a=np.array(a);b=np.array(b);v=b-a;L=np.linalg.norm(v)
    if L<1:return
    for i in range(0,int(L),13):d.line([tuple(a+v*i/L),tuple(a+v*min(i+7,L)/L)],fill=c,width=2)
def call(point,dest,s,col=ink,right=False):
    xx,yy=dest
    d.line([point,(xx+7 if right else xx-7,yy)],fill=col,width=2)
    d.ellipse((point[0]-3,point[1]-3,point[0]+3,point[1]+3),fill=col)
    t(xx,yy-14,s,18,col,True,'rm' if right else 'lm')
def xyz(x,y):return (x,y)
init();t(43,53,'表冠取消排线，天线向表带端伸出',40,b=True)
t(44,106,'主板保持原高度。沿表冠轴与环境光导光柱分别剖切，两处都取自新装配。',21,mut)
panel(32,154,780,1109);panel(807,154,1647,641);panel(807,665,1647,1109)
t(60,195,'屏幕侧俯视',28,b=True);t(60,237,'细线为屏组件外缘；天线端在图下方。',18,mut)
T=lambda x,y:(391+x*13.0,612-y*13.)
plan(['Board','ModuleSubstrate','ModuleParts','Antenna','MotorSeat','Motor','Crown','CrownBridge','ALSLens'],T)
screen=shadow(doc.Screen.Shape);ant=shadow(doc.Antenna.Shape)
paint(ant.difference(screen),T,'#ea9b46',w=1)
paint(screen,T,'#667b80',True)
t(*T(-6.5,-5.7),'思澈模组',20,'white',True,'mm');t(*T(12.1,-16),'电机',18,'white',True,'mm')
call(T(8.25,22.13),(605,291),'环境光前窗')
call(T(14.6,1.8),(629,544),'表冠接触板')
t(62,957,'整段天线 109.8 mm²，橙色是屏幕外的部分。',20,b=True)
t(62,1002,'向表带端前移 1.00 mm，屏外从 9.31 增到 27.31 mm²。',18,mut)
t(62,1054,'剩余端壁最薄约 1.12 mm，天线处壳材与射频待验证。',18,mut)
t(833,196,'表冠 A—A',27,b=True);t(834,236,'主板弹片向屏幕侧压合，向后盖拆板时自然分离。',18,mut)
A=lambda x,z:(850+(x-10.3)*55.5,595-(z-4.2)*29)
# Use isotropic geometry for this detail rather than exaggerating the spring gap.
A=lambda x,z:(856+(x-8.8)*34.0,617-(z-3.8)*34.0)
section(['Board','Frame','Daughter','Retainer','EncoderBody','EncoderRotor','CrownShaft','GuideSleeve','Crown',
         'CrownBridge','CrownPad2','CrownLeaf2'],'y',0,(8.8,23.7,3.8,14.5),A)
call(A(14.45,13.08),(1460,316),'五路金面')
call(A(13.45,12.54),(1460,372),'主板弹片')
call(A(16.10,11.1),(1460,431),'编码器立板')
t(1385,502,'工作高 1.00 mm',17,mut);t(1385,538,'自由高 1.30 mm',17,mut)
t(1385,578,'5 路合力约 1.95 N',16,mut)
t(834,704,'环境光 B—B',27,b=True);t(834,748,'独立前窗在远离天线的一端，光导穿过前框，传感器焊在主板上。',18,mut)
B=lambda y,z:(874+(y-18.0)*39.,1072-(z-11.1)*39)
section(['Frame','Screen','Board','AmbientALS','ALSBaffle','ALSPipe','ALSLens'],'x',8.25,(18.,23.75,11.1,18.9),B)
call(B(22.13,18.72),(1185,808),'前窗透光面 Ø1.88 mm')
call(B(21.65,15.45),(1185,879),'独立导光柱与遮光套')
call(B(20.25,12.53),(1185,987),'LTR-303ALS-01')
t(1178,1036,'需验证侧向入光、盖窗透射与屏幕漏光。',17,mut)
im.save(dst/'crown-antenna-als.png',optimize=True)

init();t(43,53,'后盖三条支路，合成一个可拆接头',40,b=True);t(44,106,'中央光学刚板对准后盖原有三窗；扬声器与充电触点分别在后盖上就近接入。',21,mut)
panel(32,154,799,1109);panel(825,154,1647,733);panel(825,758,1647,1109)
t(58,196,'从主板位置看后盖内面',27,b=True);t(58,239,'电池、托板与主板移开显示。',18,mut)
L=lambda x,y:(400+x*12.0,573-y*12.0)
paint(shadow(doc.Back.Shape),L,'#dce5df')
plan(['Baffle','Contact0','Contact1','ChargeBranch','PpgBoard','Speaker','SpeakerBranch','LidFlexNew','LidEnd','RearFingers','RearSocket'],L)
for i in range(3):paint(shadow(doc.getObject('Optic'+str(i)).Shape),L,'#7cc5c6',w=1)
t(*L(-9,16),'扬声器',17,'white',True,'mm');t(*L(0,-3.2),'光学刚板',15,ink,True,'mm')
call(L(3.2,16.4),(624,334),'插入主板 10P 座')
call(L(-.25,14.4),(130,443),'喇叭短支路',right=True)
call(L(0,-14.9),(593,814),'外部充电触点')
t(61,907,'血氧发射与接收各走独立光窗。',21,b=True)
t(61,951,'浅蓝是朝向皮肤的三窗投影，黑色隔墙挡住内部直射光。',18,mut)
t(61,996,'充电走线贴后盖，经光学刚区边缘回到同一条尾线。',18,mut)
t(61,1041,'充电与喇叭各有专用往返线路，光学模拟前端留在中央。',18,mut)
t(852,196,'同一条排线在开盖时如何展开',26,b=True)
t(852,240,'沿尾线中段剖切；两种状态的自由路径等长。',18,mut)
C=lambda y,z:(864+(y-2.)*34.4,553-z*24.0)
# The x-axis length and height are shown at the same scale.
C=lambda y,z:(887+(y-2.)*23.8,544-z*23.8)
shape=doc.LidFlexNew.Shape
drawcut([('closed',shape,'#c58d3d')],'x',3.18,(2,21,-6.5,11.2),C)
section(['Board','RearSocket','PpgBoard'],'x',3.18,(2,21,-6.5,11.31),C)
opened=routed_flex(6)[0]
drawcut([('open',opened,'#248a89')],'x',3.18,(2,21,-6.5,11.2),C)
call(C(18.6,2.5),(1444,430),'合盖折返','#9d6c2a')
call(C(15.65,-3.85),(1444,608),'后盖移开 6 mm','#217672')
t(855,697,'打开后先翻锁扣，再平行抽出尾端；工具操作空间与弯折寿命待打样。',17,mut)
t(852,801,'连接器十路分配',26,b=True)
for row,col,label,description in [(0,'#598d79','光学 6 路','传感器供电、LED 供电、信号地、SCL、SDA、INT'),
      (1,'#9175a1','扬声器 2 路','SPK+ / SPK−，不与光学信号共用回线'),
      (2,'#bd8263','充电 2 路','充电输入 / 充电地，电流和压降待样件确认')]:
    y=866+row*74;d.rounded_rectangle((852,y-6,874,y+8),4,fill=col);t(883,y,label,19,b=True);t(1070,y,description,17,mut)
im.save(dst/'rear-cover-routing.png',optimize=True)
App.closeDocument(doc.Name);App.closeDocument(old.Name)
