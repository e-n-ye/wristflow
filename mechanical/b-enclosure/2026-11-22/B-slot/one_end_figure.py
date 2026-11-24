"""Draw the as-photographed slot orientation beside the B board edge reliefs."""
from os import environ
import json
import sys
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

root=Path(sys.argv[1])
q=json.loads((root/'slot-check.json').read_text(encoding='utf8'))
cx,cy=q['slot_xy'];w,h,_=q['width_height_radius']
assert (w,h)==(10.,3.) and abs(abs(cx+.455)-2)<.00001
kept_fold=cx<-.455
im=Image.new('RGB',(1500,920),'white');d=ImageDraw.Draw(im)
ink='#193640';muted='#4c686f';green='#1b716a';gold='#d89947';panel='#edf4f2'
def title(x,y,txt,size=20,fill=ink,bold=False,anchor='lm'):
    f=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if bold else 'msyh.ttc'),size)
    d.text((x,y),txt,font=f,fill=fill,anchor=anchor)
def line(pts,col=ink,w=2):
    d.line(pts,fill=col,width=w,joint='curve')
def xy(x,y):return 344+x*10.5,437-y*10.5
def flag(x,y,n,px,py,txt):
    pt=xy(x,y);line([pt,(px,py)],green,3)
    d.ellipse((px-13,py-13,px+13,py+13),fill=green)
    title(px,py,n,15,'white',True,'mm');title(px+19,py,txt,18)
title(42,54,'B 版主板 · 10 × 3 mm 单侧缩窗',39,bold=True)
title(45,108,'左图与原限高图同向，右图把长窗转到你放尺子的方向。两图中的 CN1 都是板上的扣座。',21,muted)
d.rounded_rectangle((39,147,722,784),16,fill=panel)
title(60,180,'板框与安装避让',26,bold=True)
for i,ring in enumerate(q['outline_mm']):
    pts=[xy(x,y) for x,y in ring]
    d.polygon(pts,fill='#cfddda' if i==0 else 'white');line(pts,ink,3)
x,y=xy(cx,cy);title(x,y,'10 × 3',14,ink,True,'mm')
x,y=xy(*q['cn1_xy']);d.rounded_rectangle((x-10,y-36,x+10,y+36),4,fill='#839994')
title(x-16,y,'背面 CN1',15,muted,anchor='rm')
flag(17.34,14,'1',586,247,'后盖螺柱')
flag(16.55,11.06,'5',586,320,'托板支座')
flag(17.2,0,'2',586,440,'表冠轴')
flag(17.34,-14,'3',586,582,'后盖螺柱')
flag(-12.5,-20.05,'4',170,706,'电池托板支座')
title(62,749,'右上小方口⑤与底边方口④避让同一托板的两处支座。',18,muted)
title(765,182,'按照片方向对照',26,bold=True)
title(767,229,'黄山派 V1.1 屏板原始开窗为 14 × 3，圆角 R0.5。',20,muted)
top,scale=324,21
bottom=top+14*scale
def slot(x,ya,yb,fill,outline):
    d.rounded_rectangle((x-1.5*scale,ya,x+1.5*scale,yb),radius=.5*scale,fill=fill,outline=outline,width=3)
for x,lab in [(890,'原板'),(1115,'10 mm 试样')]:
    title(x,283,lab,20,ink,True,'mm')
    slot(x,top,bottom,'#dce4e5','#71888d')
new_top=top+4*scale if kept_fold else top
new_bottom=bottom if kept_fold else bottom-4*scale
slot(1115,new_top,new_bottom,'#d7f2e6',green)
d.rounded_rectangle((875,top+12,905,top+36),6,fill='#46535a')
title(1187,top+15,'黑色端 · 靠 CN1',19,muted)
line([(1182,top+15),(1156,top+15)],muted)
title(1187,bottom-10,'银色折返端 · 红圈',19,'#b4493f',True)
line([(1182,bottom-10),(1156,bottom-10)],'#b4493f')
tip_y=top+2*scale if kept_fold else bottom-2*scale
title(1115,tip_y,'补板',15,ink,True,'mm')
title(1115,(new_top+new_bottom)/2,'10',18,green,True,'mm')
title(890,(top+bottom)/2,'14',18,muted,True,'mm')
title(768,675,'银色端固定，4 mm 全从黑色端收回。' if kept_fold else '黑色端固定，4 mm 全从银色端收回。',24,ink,True)
title(768,721,'两窗短边均为 3 mm，圆角 R0.5。CN1 沿用原位。',19,muted)
line([(44,813),(1454,813)],'#b5c8c7')
title(45,851,'四处红箭头所指缺口逐一补平后，主板沿后盖方向拆出时均会遇到固定件。',21,ink,True)
title(45,893,'插头、补强及黑色部分的实际装拆包络仍需用原屏试穿，CAD 结果只覆盖图示方案。',18,muted)
im.save(root/'B-slot-notches.png')
