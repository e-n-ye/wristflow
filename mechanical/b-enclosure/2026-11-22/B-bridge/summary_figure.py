"""Dimensioned views drawn from the installed FreeCAD assemblies, looking in from the battery."""
from os import environ
from pathlib import Path
import sys,json
from PIL import Image, ImageDraw, ImageFont
from compare import App,SOURCE,HERE,shadow
from layout_space import pieces

out=Path(sys.argv[1])
q=json.loads((out/'layout-check.json').read_text('utf8'))
previous=json.loads((HERE/'round-seat/comparison.json').read_text('utf8'))
image=Image.new('RGB',(1560,1080),'white');pen=ImageDraw.Draw(image)
ink='#203940';muted='#4b646c';panel='#edf3f2';teal='#146c65';red='#a43e38'
def txt(x,y,value,size=21,color=ink,bold=False,anchor='lm'):
    font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if bold else 'msyh.ttc'),size)
    pen.text((x,y),value,font=font,fill=color,anchor=anchor)
def line(points,color,width=2):
    pen.line(points,fill=color,width=width,joint='curve')
def render(doc,info,left,heading):
    mid=left+350
    def xy(x,y):return mid-x*9.0,418-y*9.0
    def draw_union(geom,fill):
        for p in pieces(geom):
            if p.geom_type!='Polygon':continue
            pen.polygon([xy(x,y) for x,y in p.exterior.coords],fill=fill)
    pen.rounded_rectangle((left,145,left+718,651),radius=14,fill=panel)
    txt(left+22,179,heading,26,ink,True)
    for i,ring in enumerate(info['loops_xy']):
        pts=[xy(x,y) for x,y in ring]
        pen.polygon(pts,fill='#c7d8d4' if i==0 else 'white');line(pts,'#39545a')
    for k,col in [('ModuleParts','#667e8b'),('Antenna','#af5344'),('Motor','#dca950'),('RearRun','#cc9340'),
                  ('BatHeader','#446f8a'),('KeyBody','#826c8f'),('CN1','#443d39')]:
        draw_union(shadow(doc.getObject(k).Shape),col)
    for i in range(10):
        draw_union(shadow(doc.getObject('LPad'+str(i)).Shape),'#ac7e37')
    for i in range(5):
        draw_union(shadow(doc.getObject('DBG'+str(i)).Shape),'#ac7e37')
    for k,label,color in [('ModuleParts','模组','white'),('Motor','电机',ink)]:
        b=doc.getObject(k).Shape.BoundBox.Center;x,y=xy(b.x,b.y);txt(x,y,label,17,color,True,'mm')
    b=doc.Antenna.Shape.BoundBox.Center;x,y=xy(b.x,b.y);txt(x,y,'天线',15,'white',True,'mm')
    a,b=[xy(x,y) for x,y in info['witness_xy']]
    line([a,b],red,4)
    xm,ym=(a[0]+b[0])/2,(a[1]+b[1])/2
    label_right=(a[0]>mid)
    ox=left+687 if label_right else left+19
    elbow=left+570 if label_right else left+140
    line([(xm,ym),(elbow,ym-37),(ox,ym-37)],red)
    txt(ox,ym-60,f"{info['web_mm']:.3f} mm",24,red,True,'rm' if label_right else 'lm')
    # The fixed aperture and CN1 can be located on both drawings despite opposite motors.
    x,y=xy(-2.455,9.99);txt(x,y,'穿线窗',13,muted,True,'mm')

txt(43,55,'B 版 · 电机、模组与接口一起换边',39,ink,True)
txt(45,108,'从电池侧向屏幕看，与红圈截图同向。两图使用同一比例，表冠在图左。',22,muted)
base=App.openDocument(str(SOURCE));other=App.openDocument(str((out/'WristFlow-B-Coordinated.FCStd').resolve()))
render(base,previous['baseline'],38,'当前 10 × 3 mm 穿线窗版')
render(other,q['coordinated'],803,'联动换边评审版')
for x,fill,label in [(85,'#446f8a','电池插座'),(295,'#826c8f','测流短接帽'),(542,'#ac7e37','触点'),(855,'#cc9340','折返屏尾'),(1097,'#443d39','CN1'),(1280,'#af5344','板载天线')]:
    pen.rounded_rectangle((x,672,x+25,692),radius=4,fill=fill);txt(x+36,682,label,20)
line([(43,718),(1517,718)],'#b8c8c9')
txt(46,754,'天线端有多少主板需要留空',25,ink,True)
round_doc=App.openDocument(str((HERE/'round-seat/WristFlow-B-Motor-Web.FCStd').resolve()))
from compare import Part,V,B,F
slab=Part.makeBox(18.9,60,3,V(-30,-30,B-1))
round_info={'rf_held_host_mm2':round_doc.Board.Shape.common(slab).Volume/(F-B),'motor_to_radiator_mm':round_doc.Antenna.Shape.distToShape(round_doc.Motor.Shape)[0]}
for x,label,m in [(45,'当前布局',q['baseline']),(445,'同侧圆弧改型',round_info),(820,'仅电机及其缺口换边',q['motor_only']),(1210,'电机与模组一同换边',q['mirror'])]:
    txt(x,808,label,21,muted)
    txt(x,856,f"{m['rf_held_host_mm2']:.1f} mm²",34,teal,True)
    txt(x,902,f"天线至电机最近距离  {m['motor_to_radiator_mm']:.1f} mm",19,muted)
line([(43,941),(1517,941)],'#b8c8c9')
txt(45,977,'面积为天线端仍有主板、预留给射频的板材。当前与联动版，电芯对天线的投影覆盖均为 109.8 mm²。',20,ink)
txt(45,1022,'模组换边后，天线至表冠轴从 28.85 mm 缩到 1.73 mm。板厚 0.80 mm，六层板强度与整机蓝牙表现仍待实测。',19,muted)
image.save(out/'B-coordinated-review.png')
