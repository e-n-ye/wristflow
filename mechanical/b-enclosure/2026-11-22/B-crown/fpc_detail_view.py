"""Assembly callouts for the crown cross section and single rear flex."""
from os import environ
from pathlib import Path
import sys
from PIL import Image,ImageDraw,ImageFont
from shapely.geometry import Polygon
from one_rear_flex import App,Part,V,HERE
from layout_space import shadow

dst=Path(sys.argv[1]);doc=App.openDocument(str((dst/'WristFlow-B-Rear-FPC.FCStd').resolve()))
im=Image.new('RGB',(1540,940),'#fbfaf7');p=ImageDraw.Draw(im);ink='#213e40';mut='#526863'
def text(x,y,s,n=20,b=False,c=ink,anchor='lm'):
    f=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),n)
    p.text((x,y),s,font=f,fill=c,anchor=anchor)
def drawpoly(geom,conv,col):
    parts=[geom] if geom.geom_type=='Polygon' else [x for x in geom.geoms if x.geom_type=='Polygon']
    for g in parts:
        if g.is_empty:continue
        outer=[conv(a,b) for a,b in g.exterior.coords];holes=[[conv(a,b) for a,b in q.coords] for q in g.interiors]
        m=Image.new('1',im.size);pen=ImageDraw.Draw(m);pen.polygon(outer,fill=1)
        for h in holes:pen.polygon(h,fill=0)
        im.paste(col,mask=m);p.line(outer,fill='#425754',width=2)
def note(pt,x,y,s):
    p.line([pt,(x-6 if x>pt[0] else x+6,y)],fill=mut,width=2);p.ellipse((pt[0]-3,pt[1]-3,pt[0]+3,pt[1]+3),fill=ink)
    text(x,y-13,s,18,True)
def cut(k,col,at,limits,T):
    x,X,z,Z=limits
    pts=[V(a,at,b) for a,b in [(x,z),(X,z),(X,Z),(x,Z),(x,z)]]
    for f in doc.getObject(k).Shape.common(Part.Face(Part.makePolygon(pts))).Faces:
        w=lambda a:[(v.x,v.z) for v in a.discretize(Deflection=.009)]
        drawpoly(Polygon(w(f.OuterWire),[w(j) for j in f.Wires if not j.isSame(f.OuterWire)]).buffer(0),T,col)
text(40,53,'表冠怎么固定，后盖柔板哪里补强',40,True)
for rect in [(30,119,777,902),(800,119,1509,902)]:
    p.rounded_rectangle(rect,18,fill='#eff3ef',outline='#d3ddd5',width=2)
text(56,163,'表冠固定件与两块直角小板',28,True)
text(56,211,'从电池仓看压片',19,False,mut)
def retview(k):
    s=doc.getObject(k).Shape.copy()
    s.rotate(V(),V(0,0,1),-90);s.rotate(V(),V(1,0,0),-90)
    return shadow(s)
T=lambda y,z:(277+y*28,242+(z-4.49)*28)
drawpoly(retview('Retainer'),T,'#a7bfc0');drawpoly(retview('RetainerScrews'),T,'#75878c')
pt=T(4.42,8.72);note(pt,502,317,'两颗压片螺钉');text(499,353,'孔中心距 8.84 mm',18,False,mut)
text(55,459,'沿表冠轴线剖开',19,False,mut)
A=lambda x,z:(85+(x-10)*31,796-(z-3.8)*31)
for k,c in [('Board','#548471'),('Retainer','#94aaad'),('Daughter','#368574'),('EncoderBody','#a16b90'),
            ('EncoderRotor','#c5a0ba'),('CrownShaft','#d4a055'),('GuideSleeve','#59a4a4'),('Crown','#7c8f94'),
            ('CrownBridge','#368574'),('CrownLeaf2','#cf903a'),('CrownPad2','#e0bb65')]:
    cut(k,c,0,(10,23,3.8,14.2),A)
note(A(14.52,13.11),542,524,'水平板底面镀金')
note(A(13.35,12.77),542,588,'弹片焊在主板上')
note(A(16.15,9.4),542,654,'立板上焊编码器')
text(53,846,'剖面穿过中间一路；另外四路沿板边并排。',18,False,mut)
text(824,163,'后盖整张 FPC · 从电池侧看',28,True)
text(825,210,'橙色为同一片柔板，绿色为血氧贴装区背面的局部补强。',18,False,mut)
R=lambda x,y:(1050+x*19.0,579-y*16.25)
for k,c in [('LidFlexNew','#d99947'),('PpgStiffener','#77a48c'),('RearFingers','#e9bc6b')]:
    drawpoly(shadow(doc.getObject(k).Shape),R,c)
for k in ('ChargeTab0','ChargeTab1'):drawpoly(shadow(doc.getObject(k).Shape),R,'#e9bc6b')
note(R(4,16.5),1270,302,'10P 金手指尾端')
note(R(-.5,14.4),837,392,'喇叭支路')
note(R(2,1),1265,552,'血氧区 FR-4 补强')
text(1264,587,'器件贴在朝皮肤一面',17,False,mut)
note(R(1,-14.8),1245,797,'两个充电触点')
text(824,864,'试放叠厚：FPC 0.12 + 胶 0.05 + FR-4 0.30 = 0.47 mm。',18,False,mut)
im.save(dst/'crown-and-rear-fpc.png',optimize=True);App.closeDocument(doc.Name)
