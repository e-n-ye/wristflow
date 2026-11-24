"""Front comparison and centreline cut from reopened design files."""
from os import environ
from pathlib import Path
import sys,json
from PIL import Image, ImageDraw, ImageFont
from shapely.geometry import Polygon
from shapely.ops import unary_union
from center_fit import App,Part,V,PREVIOUS

dst=Path(sys.argv[1]);old=App.openDocument(str(PREVIOUS))
doc=App.openDocument(str((dst/'WristFlow-B-Centered-Crown.FCStd').resolve()))
W,H=1560,1080;im=Image.new('RGB',(W,H),'#fbfaf7');d=ImageDraw.Draw(im)
ink='#213e40';mut='#506966';line='#425b60'
def txt(x,y,s,n=21,b=False,c=ink,a='lm'):
    font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),n)
    d.text((x,y),s,font=font,fill=c,anchor=a)
def panel(x,y,X,Y):
    d.rounded_rectangle((x,y,X,Y),19,fill='#f0f3ef',outline='#d3dfda',width=2)
def dashed(x,X,y,col):
    for a in range(int(x),int(X),16):d.line([(a,y),(min(a+8,X),y)],fill=col,width=2)
def polys(s):
    if s.is_empty:return []
    if s.geom_type=='Polygon':return [s]
    return [p for g in getattr(s,'geoms',[]) for p in polys(g)]
def fill(g,conv,col,edge=line):
    for p in polys(g):
        outer=[conv(x,y) for x,y in p.exterior.coords]
        holes=[[conv(x,y) for x,y in r.coords] for r in p.interiors]
        m=Image.new('1',im.size);pen=ImageDraw.Draw(m);pen.polygon(outer,fill=1)
        for h in holes:pen.polygon(h,fill=0)
        im.paste(col,mask=m);d.line(outer,fill=edge,width=2)
        for h in holes:d.line(h,fill=edge)
def plan(shape):
    vertices,tris=shape.tessellate(.04)
    faces=[Polygon([(vertices[i].x,vertices[i].y) for i in t]) for t in tris]
    return unary_union([p for p in faces if p.area>.000001])
def rotview(source,ox):
    def xy(x,y):return ox+x*7.60,422-y*7.60
    for key,col in [('Frame','#9db4ae'),('CrownShaft','#799094'),('Crown','#6f8b90'),
                    ('Screen','#303c40'),('ActiveArea','#182327')]:
        fill(plan(source.getObject(key).Shape),xy,col)
    for x,X in [(ox-196,ox-162),(ox+175,ox+226)]:dashed(x,X,422,'#527f78')
    return xy
txt(41,49,'表冠移回右侧正中',39,True);txt(43,100,'同一块 0.80 mm 主板、同一颗 BMQ22 编码器、原位 500 mAh 电池。两张正视图比例相同。',22,c=mut)
panel(33,146,750,651);panel(778,146,1527,651)
txt(60,184,'上一版',27,True);txt(804,184,'居中试装',27,True)
t1=rotview(old,363);t2=rotview(doc,1116)
d.line([(550,306),(584,306),(584,422)],fill='#976044',width=2)
txt(596,361,'15.2 mm',18,c='#8a573a');txt(1350,421,'中心线',18,c='#3c726a')
txt(64,619,'冠帽直径 6.6 mm',19,c=mut);txt(804,619,'外侧冠帽比上一版多伸出 1.68 mm',19,c=mut)
panel(33,682,1527,1049);txt(61,721,'沿新表冠轴线剖开',26,True)
plane=Part.Face(Part.makePolygon([V(x,0,z) for x,z in [(1.0,0),(25.4,0),(25.4,12.4),(1.0,12.4),(1.0,0)]]))
def xy(x,z):return 61+(x-1.0)*23.4,1035-z*23.4
def native(w):return [(p.x,p.z) for p in w.discretize(Deflection=.01)]
for key,col in [('Back','#c8d1cd'),('Tray','#68878b'),('Board','#397866'),('Antenna','#ae6f62'),('ModuleSubstrate','#808c98'),
                ('Battery','#b9cfa5'),('Frame','#a4bab4'),('CrownFlex','#d4a150'),('Retainer','#71949b'),('Daughter','#429279'),
                ('EncoderBody','#a4658b'),('EncoderRotor','#caa0bd'),('SwitchFollower','#985949'),
                ('CrownShaft','#dca34d'),('GuideSleeve','#56a1ac'),('Crown','#738b91')]:
    for f in doc.getObject(key).Shape.common(plane).Faces:
        fill(Polygon(native(f.OuterWire),[native(w) for w in f.Wires if not w.isSame(f.OuterWire)]).buffer(0),xy,col)
txt(160,915,'电芯',18,True,c='#355440',a='mm')
def swatch(x,y,c,s):
    d.rounded_rectangle((x,y-8,x+23,y+8),4,fill=c);txt(x+34,y,s,19)
swatch(669,798,'#429279','编码器小板');swatch(940,798,'#a4658b','BMQ22 本体');swatch(1212,798,'#dca34d','表冠轴')
swatch(669,845,'#71949b','内侧固定压片');swatch(940,845,'#a4bab4','局部加宽的中框');swatch(1212,845,'#738b91','冠帽')
txt(669,909,'电芯到固定压片 0.37 mm，小板到电芯 0.81 mm。',21,True)
txt(669,954,'按入 0.25 mm 后，冠帽距中框 0.13 mm，距导套 0.08 mm。',19,c=mut)
txt(669,998,'剖面与正视图均取自保存后的 FreeCAD 实体。',17,c=mut)
im.save(dst/'centered-crown.png',optimize=True)
App.closeDocument(doc.Name);App.closeDocument(old.Name)
