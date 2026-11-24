"""Draw the lower board support and its load path from the saved FreeCAD solids."""
from os import environ
from pathlib import Path
import json
import sys
from PIL import Image, ImageDraw, ImageFont
from shapely.geometry import Polygon, box
from lower_support import App, Part, V, BASE, FILE, NETS, shadow, slice_xy

out=Path(sys.argv[1]).resolve()
doc=App.openDocument(str(out/FILE))
old=App.openDocument(str(BASE))
delta=doc.Frame.Shape.cut(old.Frame.Shape).removeSplitter()
data=json.loads((out/'support-check.json').read_text('utf-8'))
im=Image.new('RGB',(1510,838),'#fafaf7')
p=ImageDraw.Draw(im)
ink='#203637';muted='#4f6463';edge='#465854'
frame='#a9b9b3';rib='#efa82f';board='#4c8470';parts='#637983';ant='#ba6856';motor='#8f799f'
def txt(x,y,s,n=18,col=ink,b=False,anchor='lm'):
    ft=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),n)
    p.text((x,y),s,fill=col,font=ft,anchor=anchor)
def flatten(g):
    if g.is_empty: return
    if g.geom_type=='Polygon': yield g
    else:
        for sub in getattr(g,'geoms',[]):yield from flatten(sub)
def fill(g,conv,color):
    for poly in flatten(g):
        outer=[conv(a,b) for a,b in poly.exterior.coords]
        holes=[[conv(a,b) for a,b in r.coords] for r in poly.interiors]
        m=Image.new('1',im.size);brush=ImageDraw.Draw(m);brush.polygon(outer,fill=1)
        for h in holes:brush.polygon(h,fill=0)
        im.paste(color,mask=m)
        for w in [outer]+holes:p.line(w,fill=edge,width=1)
def key(x,y,col,name):
    p.rounded_rectangle((x,y-7,x+23,y+7),3,fill=col)
    txt(x+32,y,name,17)
def call(pt,end,label,anchor='lm'):
    x,y=end
    p.line([pt,(x-5 if anchor=='lm' else x+5,y)],fill=ink,width=2)
    p.ellipse((pt[0]-3,pt[1]-3,pt[0]+3,pt[1]+3),fill=ink)
    txt(x,y-9,label,16,b=True,anchor=anchor)

txt(38,52,'主板下缘的支撑与固定',40,b=True)
txt(40,104,'黑圈对应天线与电机之间的空隙。新增塑料筋连接表带端内壁和原螺钉座，板边落在筋的平面上。',21,muted)
for r in [(32,148,815,746),(837,148,1477,746)]:
    p.rounded_rectangle(r,16,fill='#eff3ef',outline='#d5dfd7',width=2)
txt(57,190,'从屏幕侧看下半部',26,b=True)
txt(57,229,'屏幕及排线隐藏，表带端朝下。',17,muted)
win=box(-20.4,-23.75,19.15,-6.8)
T=lambda x,y:(433+x*18.8,274+(-6.8-y)*18.8)
def plan(sh,col): fill(sh.intersection(win),T,col)
plan(slice_xy(old.Frame.Shape,12.44),frame)
for k,c in [('Board',board),('ModuleSubstrate','#90a29a'),('ModuleParts',parts),('Antenna',ant),
            ('MotorSeat','#b7a8c2'),('Motor',motor)]:
    plan(shadow(doc.getObject(k).Shape),c)
plan(shadow(delta),rib)
plan(shadow(doc.Clamp1.Shape),'#819395')
txt(*T(-6.6,-11.7),'思澈模组',21,'white',True,'mm')
txt(*T(-6.4,-19.3),'天线端',20,'white',True,'mm')
txt(*T(12.2,-16.3),'电机',19,'white',True,'mm')
call(T(5.15,-9.8),(654,261),'原固定螺钉')
call(T(5.15,-20.5),(614,604),'新托筋')
key(57,649,board,'主板');key(171,649,frame,'中框');key(284,649,rib,'新增塑料筋')
txt(57,704,'天线到筋最近 1.65 mm，电机本体到筋最近 1.07 mm。',18,muted)

txt(861,190,'沿托筋和螺钉中心剖开',26,b=True)
txt(861,229,'左侧为表带端，上方朝屏幕。',17,muted)
T2=lambda y,z:(882+(y+24)*33.2,513-(z-9.9)*33.2)
limits=[(-24,10.2),(-7.35,10.2),(-7.35,16.09),(-24,16.09),(-24,10.2)]
plane=Part.Face(Part.makePolygon([V(5.15,y,z) for y,z in limits]))
def cross(shape,col):
    for f in shape.common(plane).Faces:
        def points(w):return [(v.y,v.z) for v in w.discretize(Deflection=.008)]
        fill(Polygon(points(f.OuterWire),[points(w) for w in f.Wires if not w.isSame(f.OuterWire)]).buffer(0),T2,col)
cross(old.Frame.Shape,frame);cross(doc.Screen.Shape,'#c3cdcb')
cross(doc.Board.Shape,board);cross(delta,rib);cross(doc.Clamp1.Shape,'#819395')
call(T2(-14.15,12.10),(1125,532),'主板与托筋接触')
call(T2(-10.72,12.60),(1294,277),'原螺钉座')
txt(861,577,'主板厚 0.80 mm，原悬出的 3.19 mm 板边得到支撑。',18,b=True)
txt(861,620,'表冠小板对主板的四个触点',18,muted)
for i,net in enumerate(NETS):
    x=862+i*145
    p.rounded_rectangle((x,646,x+121,699),8,fill='#e7c881',outline='#ad8a4c')
    txt(x+60,672,net,22,b=True,anchor='mm')
txt(40,792,'名义装配尺寸。板材挠度、塑料成型公差和整机蓝牙性能仍需用样件验证。',18,muted)
im.save(out/'lower-support-section.png',optimize=True)
App.closeDocument(doc.Name);App.closeDocument(old.Name)
