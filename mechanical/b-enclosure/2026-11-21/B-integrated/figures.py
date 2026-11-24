"""Assemble labelled native FreeCAD views and computed size results for the hardware review."""
from os import environ
import sys,json
from pathlib import Path
from PIL import Image, ImageChops, ImageFont, ImageDraw, ImageOps
root=Path(sys.argv[1])
r=json.loads((root/'geometry.json').read_text(encoding='utf8'))
ink='#213a41';text2='#4e656a';rule='#c8d7d8';teal='#167477';red='#b5483d';gold='#986014';navy='#35657f'
def make(w,h):return Image.new('RGB',(w,h),'white')
def t(x,y,word,size=22,col=ink,bold=False):
    d.text((x,y),word,font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if bold else 'msyh.ttc'),size),fill=col,anchor='lt')
def li(x1,y1,x2,y2,col=rule,w=2):d.line((x1,y1,x2,y2),fill=col,width=w)
def trim(name):
    im=Image.open(root/(name+'.png')).convert('RGB')
    return im.crop(ImageChops.difference(im,Image.new('RGB',im.size,'white')).getbbox())
def image(name,x,y,w,h):
    m=ImageOps.contain(trim(name),(w,h),Image.Resampling.LANCZOS)
    x=x+(w-m.width)//2;y=y+(h-m.height)//2;sheet.paste(m,(x,y))
    return x,y,m.width,m.height
def title(head,sub):
    t(54,32,head,43,ink,True);t(56,100,sub,23,text2);li(54,146,1466,146)

sheet=make(1520,1020);d=ImageDraw.Draw(sheet)
title('B 版整机 · 按原屏尾重新布局','屏幕按原来的方向安装。主板保留 14 × 3 mm 的穿线窗，CN1 在电池侧扣合。')
t(65,167,'合盖整表',22,text2);image('full',103,174,696,201)
li(866,175,866,370);t(900,170,'后盖内侧',22,text2);image('lid',1050,166,320,212)
li(54,394,1466,394);t(74,421,'移开后盖和电池，从背面看',26,ink,True)
ix,iy,iw,ih=image('inside',65,476,630,437)
def point(x,y):
    return ix+(23.40-x)*iw/43.95, iy+(23.75-y)*ih/47.5
def pin(x,y,letter,col,dx,dy):
    u,v=point(x,y);xm,ym=u+dx,v+dy;li(u,v,xm,ym,col,3)
    d.ellipse((xm-16,ym-16,xm+16,ym+16),fill=col,outline='white',width=2)
    d.text((xm,ym-1),letter,font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/msyhbd.ttc'),17),anchor='mm',fill='white')
pin(10.545,5.135,'A',gold,-29,19);pin(0,9.99,'B',gold,22,21)
pin(-3.15,14.18,'C',teal,-32,-21);pin(-13,15.5,'D',teal,24,24)
pin(-3.3,-8.3,'E',navy,-33,17);pin(-14.2,-8.3,'F',red,25,-23)
for i,(k,head,body,col) in enumerate([
    ('A/B','原屏尾和扣座','窄尾穿窗折回，扣在 CN1。装配时先穿补强头，再把主板坐实。',gold),
    ('C','后盖十触点','两排触点移到窗外。开盖时弹触与主板分开。',teal),
    ('D','电机与托座','电机移到图右上，靠中框传振，让出模组退板路径。',teal),
    ('E','思澈模组','横转 90°，模组与折返屏尾名义最小间隙 0.52 mm。',navy),
    ('F','板载天线','天线下方主板已开缺口；109.8 mm² 天线投影全被电池覆盖。',red)]):
    yy=434+i*100;t(728,yy,k,24,col,True);t(800,yy,head,24,ink,True)
    t(800,yy+41,body,18,text2)
li(54,951,1466,951);t(55,969,'表体 41.1 × 47.5 × 16.0 mm，含光学凸台总厚 17.5 mm。红色项需用装壳及佩戴状态验证 BLE。',21,text2)
sheet.save(root/'B-integrated-review.png')

import FreeCAD as App, Part
import shapely as sh
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'B-board-space'))
from layout_space import shadow,pieces
model=App.openDocument(str((root/'WristFlow-B-Placement.FCStd').resolve()))
watch=App.openDocument(str((root/'WristFlow-B-Integrated.FCStd').resolve()))
from height_figure import render_map
render_map(root,model,watch,r)

def xy(x,y,back):
    return (1130 if back else 380)+(-x if back else x)*9.2,458-y*9.2
def paint(geo,color,back):
    mask=Image.new('1',sheet.size,0);md=ImageDraw.Draw(mask)
    for p in pieces(geo):
        if p.geom_type!='Polygon':continue
        for ring,value in [(p.exterior,1)]+[(h,0) for h in p.interiors]:
            md.polygon([xy(x,y,back) for x,y in ring.coords],fill=value)
    sheet.paste(color,(0,0,sheet.width,sheet.height),mask)
def color_node(k,color,back):
    paint(shadow(model.getObject(k).Shape),color,back)
def key(x,y,col,words):
    d.rounded_rectangle((x,y+4,x+23,y+23),radius=3,fill=col);t(x+36,y,words,21)

sheet=make(1520,1000);d=ImageDraw.Draw(sheet)
title('B 版装配 · 屏、主板和电池怎么固定','左图从屏侧看中框与板孔，右图沿厚度方向穿过思澈模组。剖面层厚放大示意，标注值来自 B 版模型。')
t(62,176,'中框定位',31,ink,True);t(760,176,'从屏幕到后盖',31,ink,True)
paint(shadow(watch.GlassTape.Shape),'#af657d',False)
color_node('Board','#e0e6e4',False)
# Ring on the overlay shows the projected adhesive seat, situated above the PCB.
paint(shadow(watch.GlassTape.Shape),'#af657d',False)
color_node('FrontFilm','#dac19b',False)
color_node('Crossing','#cb812b',False)
for i,(xx,yy) in enumerate(r['support_holes'],1):
    u,v=xy(xx,yy,False);d.ellipse((u-12,v-12,u+12,v+12),fill='#236c65',outline='white',width=2)
    d.text((u,v-1),str(i),font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/msyhbd.ttc'),16),anchor='mm',fill='white')
key(64,685,'#af657d','屏幕由周边模切胶粘在中框台阶上')
key(64,728,'#236c65','主板落在 3 个凸肩上，螺钉从后盖侧锁紧')
key(64,771,'#dac19b','宽片随屏；入窗前的窄尾保留活动空间')
li(708,170,708,842)
def layer(y1,y2,w,col):
    x=865;d.rounded_rectangle((x-w//2,y1,x+w//2,y2),radius=3,fill=col,outline='#405456',width=1)
def row(y,txt,col=ink):
    t(1030,y,txt,21,col)
layer(250,309,245,'#42545b');row(267,'屏幕总成')
layer(309,318,130,'#d9a961');row(309,'贴屏宽片 · 名义 0.12 mm')
li(842,322,842,377,'#66847f',2);row(348,'屏背基准至板面 0.93 mm',text2)
layer(380,409,240,'#44816c');row(383,'主板 · 0.80 mm')
layer(410,505,146,'#748792');row(444,'思澈模组 · 高 3.10 mm')
li(842,508,842,548,'#66847f',2);row(513,'模组至电芯 0.50 mm',text2)
layer(551,557,202,'#ad73a5');row(552,'主板侧电气绝缘 · 试算含胶 0.10 mm','#835479')
layer(559,672,202,'#dfbb72');row(610,'电芯 · 5.00 mm 成品占位')
layer(684,696,221,'#a0a9aa');row(683,'后盖侧可拆托板 · 0.40 mm')
layer(735,753,245,'#50616a');row(733,'后盖')
li(55,851,1465,851)
t(56,873,'原屏可贴合至沿 Z 分开约 15 mm；B 版锁紧后的板面位置由支承决定。',23,ink,True)
t(56,920,'若另加 0.10 mm 绝缘片，模组处剩 0.40 mm。按现有 0.30 mm 电芯厚度余量及 0.20 mm 装配间隙，还差 0.10 mm。',20,text2)
sheet.save(root/'B-mounting-stack.png')
