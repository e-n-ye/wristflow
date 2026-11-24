"""Annotate orthographic FreeCAD exports; model features retain their actual scale inside each view."""
from os import environ
from pathlib import Path
import json
import sys
from PIL import Image, ImageDraw, ImageFont, ImageChops

folder=Path(sys.argv[1])
paper='white'
ink='#1b303a'
dim='#526570'
rule='#c6d0d4'
edge='#0a6774'
purple='#795d96'
ochre='#966318'
blue='#356491'
red='#994451'
fontroot=Path(environ['SystemRoot'])/'Fonts'
def font(size,bold=False):
    return ImageFont.truetype(str(fontroot/('msyhbd.ttc' if bold else 'msyh.ttc')),size)
def label(x,y,word,size=23,color=ink,bold=False):
    draw.text((x,y),word,font=font(size,bold),fill=color,anchor='lt')
def line(pts,color,width=3):
    draw.line(pts,fill=color,width=width,joint='curve')
def pointer(pts,color):
    line(pts,color)
    x,y=pts[-1]
    draw.ellipse((x-5,y-5,x+5,y+5),fill='white',outline=color,width=3)
def source(stem):
    return Image.open(folder/(stem+'.png')).convert('RGB')
def trim(im):
    bounds=ImageChops.difference(im,Image.new('RGB',im.size,'white')).getbbox()
    return im.crop(bounds)
def place(im,x,y,w,h):
    global sheet
    im=im.resize((w,h),Image.Resampling.LANCZOS)
    sheet.paste(im,(x,y))
def header(title,sub):
    label(64,38,title,44,ink,True)
    label(66,106,sub,23,dim)
    line([(64,151),(1376,151)],rule,2)

data=json.loads((folder/'geometry.json').read_text(encoding='utf8'))
sheet=Image.new('RGB',(1440,900),paper)
draw=ImageDraw.Draw(sheet)
header('B 版维修纵剖面','距表体中线 9 mm，沿表带方向切开。上方朝向屏幕，下方朝向手腕。')
sec=trim(source('section'))
y0,y1,z0,z1=data['section_yz_bounds']
factor=24.0
x0,top=134,299
def pos(y,z):
    return (round(x0+(y-y0)*factor),round(top+(z1-z)*factor))
place(sec,x0,top,round((y1-y0)*factor),round((z1-z0)*factor))
label(65,180,'屏边落在中框台阶',26,red,True)
label(65,221,'0.15 mm 周边胶层',21,dim)
pointer([(174,257),(174,283),pos(-20.21,12.95)],red)
label(380,180,'螺钉从后侧锁主板',26,edge,True)
label(380,221,'板前侧由中框凸肩承托',21,dim)
pointer([(402,257),(402,278),pos(-18.5,11.05)],edge)
label(933,180,'三线电池插头',26,ochre,True)
label(933,221,'开盖后沿板面法向拔出',21,dim)
pointer([(1055,257),(1055,279),pos(15.45,10.10)],ochre)
label(61,719,'电芯与托板一起取出',25,ochre,True)
pointer([(283,710),(283,688),pos(-3.4,3.05)],ochre)
label(745,719,'喇叭随后盖移开，露出电池座',25,blue,True)
pointer([(1052,707),(1052,688),pos(16.0,3.45)],blue)
line([(1309,top),(1309,top+384)],dim,2)
for yy in (top,top+384):line([(1297,yy),(1321,yy)],dim,2)
label(1319,top+169,'16.0',20,dim)
label(1321,top+199,'mm',17,dim)
line([(64,797),(1376,797)],rule,2)
label(66,822,'平背外表面至前框顶 16.0 mm。中央光学凸台在切面以外，含凸台名义总厚 17.5 mm。',22,dim)
sheet.save(folder/'B-service-review.png')

sheet=Image.new('RGB',(1440,915),'white');draw=ImageDraw.Draw(sheet)
header('打开后盖，电池可以留在表内','左侧是从手腕一侧看进去的实际模型。右侧放大同一块主板端部，位置与左图一致。')
raw=source('open');w,h=raw.size
def span(mask):
    return min(mask),max(mask)
xs=span([x for x in range(w) if raw.getpixel((x,round(h*.40)))!=(255,255,255)])
xmid=sum(xs)/2
ys=span([y for y in range(h) if raw.getpixel((round(xmid),y))!=(255,255,255)])
ymid=sum(ys)/2;scale=(xs[1]-xs[0])/41.1
def pix(x,y):
    return (round(xmid-x*scale),round(ymid-y*scale))
whole=trim(raw)
sw=round(608*whole.width/whole.height)
place(whole,77,194,sw,608)
realbounds=ImageChops.difference(raw,Image.new('RGB',raw.size,'white')).getbbox()
def left(x,y):
    a,b=pix(x,y)
    return (77+(a-realbounds[0])*sw/whole.width,194+(b-realbounds[1])*608/whole.height)
p,q=left(17.8,21.65),left(1.3,10.10)
draw.rounded_rectangle((p[0],p[1],q[0],q[1]),8,outline=edge,width=4)
zoom=(pix(18.35,21.8),pix(.10,9.85))
(xa,ya),(xb,yb)=zoom
crop=raw.crop((xa,ya,xb,yb))
zx,zy,zw=716,200,510
zh=round(zw*crop.height/crop.width)
place(crop,zx,zy,zw,zh)
draw.rectangle((zx-1,zy-1,zx+zw,zy+zh),outline=rule,width=2)
def zoompt(x,y):
    a,b=pix(x,y)
    return (zx+(a-xa)*zw/crop.width,zy+(b-ya)*zh/crop.height)
def mark(x,y,s,col,dx,dy):
    u,v=zoompt(x,y)
    px,py=u+dx,v+dy
    line([(px,py),(u,v)],col,3)
    draw.ellipse((px-17,py-17,px+17,py+17),fill=col,outline='white',width=2)
    draw.text((px,py-1),s,font=font(19,True),fill='white',anchor='mm')
mark(7,20,'A',edge,47,-29)
mark(9,15.45,'B',ochre,51,-18)
mark(15,16.16,'C',purple,-37,-17)
mark(7,11.63,'D',blue,45,14)
for i,(letter,title,desc,col) in enumerate([('A','串口探针','5 点，用于首次烧录和日志',edge),('B','电池线壳','夹具中间给插拔工具留窗',ochre),('C','三路短接帽','断电换测流插块，选一路串表',purple),('D','后盖触点','连接后盖上的 PPG、喇叭和充电触点',blue)]):
    yy=570+i*60
    label(707,yy,letter,24,col,True)
    label(754,yy,title,23,ink,True)
    label(922,yy+2,desc,21,dim)
line([(64,833),(1376,833)],rule,2)
label(66,854,'开盖图中的米色大面是电池托板。夹具借用两个后盖螺孔；测整机负载时，后盖电路需通过延长转接连回。',21,dim)
sheet.save(folder/'B-open-review.png')
