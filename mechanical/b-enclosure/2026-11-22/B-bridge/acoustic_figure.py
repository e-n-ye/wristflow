"""Compare the two lid layouts of the coordinated PCB in the battery-side view."""
from os import environ
from pathlib import Path
import sys,json
from PIL import Image,ImageDraw,ImageFont
from compare import App,shadow,HERE
from layout_space import pieces

out=Path(sys.argv[1])
im=Image.new('RGB',(1450,850),'white');d=ImageDraw.Draw(im)
ink='#213942';muted='#526970';teal='#19726c'
def write(x,y,text,size=21,color=ink,bold=False,align='lm'):
    font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if bold else 'msyh.ttc'),size)
    d.text((x,y),text,font=font,fill=color,anchor=align)
write(45,50,'同一块换边主板 · 两种出声孔位置',39,bold=True)
write(46,105,'从后盖看，表冠在图左。喇叭和天线不在同一高度，标注距离取两者三维外形上的最近点。',21,muted)
for x0,folder,label in [(35,'fixed-port-3','保留原出声孔'),(745,'mirror-port-3','喇叭、密封垫和孔一起换边')]:
    p=HERE/folder;r=json.loads((p/'layout-check.json').read_text('utf8'))
    doc=App.openDocument(str((p/'WristFlow-B-Coordinated.FCStd').resolve()))
    d.rounded_rectangle((x0,144,x0+675,690),14,fill='#eef4f3')
    write(x0+20,184,label,26,bold=True)
    cx=x0+322;cy=429;scale=8.6
    def xy(x,y):return cx-x*scale,cy-y*scale
    def geo(s,color,border):
        for poly in pieces(shadow(s)):
            if poly.geom_type!='Polygon':continue
            pts=[xy(*z) for z in poly.exterior.coords]
            d.polygon(pts,fill=color);d.line(pts,fill=border,width=2)
    geo(doc.Back.Shape,'#d6e2df','#55716c')
    geo(doc.Crown.Shape,'#9daaae','#566970')
    geo(doc.Antenna.Shape,'#ad5547','#944b43')
    geo(doc.Speaker.Shape,'#5888a2','#44758e')
    geo(doc.Outlet.Shape,'#e6b863','#b38338')
    for k,name in [('Speaker','喇叭'),('Antenna','天线')]:
        b=doc.getObject(k).Shape.BoundBox.Center;x,y=xy(b.x,b.y)
        write(x,y,name,17,'white',True,'mm')
    b=doc.Outlet.Shape.BoundBox.Center;x,y=xy(b.x,b.y);sign=1 if x>cx else -1
    d.line([(x,y),(x+sign*48,y-26)],fill='#8d641e',width=3)
    write(x+sign*50,y-35,'出声口',17,'#805b22',True,'lm' if sign>0 else 'rm')
    x,y=xy(23.4,0);write(x-8,y,'表冠',17,muted,align='rm')
    write(x0+22,658,f"天线至喇叭外形   {r['speaker_to_antenna_mm']:.2f} mm",23,teal,True)
    App.closeDocument(doc.Name)
d.line([(42,728),(1409,728)],fill='#b0c3c6',width=2)
write(45,762,'两版都保留表冠、10 × 3 mm 屏窗和电池位置。两条短声道均已核对几何连通。',22,ink,True)
write(45,809,'声压、频响、佩戴遮挡及装配密封仍需实测。天线距表冠轴均为 1.73 mm。',19,muted)
im.save(out/'B-sound-port-review.png')
