"""Caption the two native FreeCAD views and their measured area/height results."""
from os import environ
import json
from pathlib import Path
import sys
from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageOps

folder=Path(sys.argv[1])
data=json.loads((folder/'pass.json').read_text(encoding='utf8'))
sheet=Image.new('RGB',(1520,1000),'#ffffff')
d=ImageDraw.Draw(sheet)
ink='#223841';secondary='#475f67'
def text(x,y,s,size=23,col=ink,bold=False):
    f=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if bold else 'msyh.ttc'),size)
    d.text((x,y),s,fill=col,font=f,anchor='lt')
def line(x1,y1,x2,y2):d.line((x1,y1,x2,y2),fill='#cfdbdd',width=2)
text(56,38,'旧 B 板空间预算 · 屏侧直扣假设已撤回',40,ink,True)
text(58,104,'图中板框尚未开屏尾长窗。原机穿窗折返已确认，以下两面面积须在屏座、模组重新布局后复算。',23,'#8c4b28')
line(55,157,1465,157)
for j,(key,title,caption) in enumerate([('front','屏幕侧','绿色包络高 0.7 mm'),('rear','电池侧','蓝色包络按当地可用高度分档')]):
    x0=58+j*730
    text(x0,183,title,30,ink,True);text(x0+155,190,caption,21,secondary)
    im=Image.open(folder/(key+'.png')).convert('RGB')
    bb=ImageChops.difference(im,Image.new('RGB',im.size,'white')).getbbox()
    im=ImageOps.contain(im.crop(bb),(600,395),method=Image.Resampling.LANCZOS)
    sheet.paste(im,(x0+(660-im.width)//2,245+(395-im.height)//2))
line(754,183,754,863)
def swatch(x,y,col,s):
    d.rounded_rectangle((x,y+3,x+22,y+24),radius=3,fill=col,outline='#74858b')
    text(x+34,y,s,20,secondary)
swatch(82,665,'#368375','可布件');swatch(233,665,'#c78167','天线端待复核');swatch(437,665,'#ad9274','既有器件')
for i,(s,c) in enumerate([('1 mm','#d6e7f2'),('2 mm','#a6cfe0'),('3 mm','#6badc1'),('4 mm','#3e819e'),('6 mm','#375c80')]):
    swatch(796+i*132,665,c,s)
a=data['height_bins']['Front'][2];b=data['height_bins']['Rear'][2];c=data['height_bins']['Rear'][3]
text(81,727,f"0.7 mm 高  ·  {a['area']:.1f} mm²",31,ink,True)
text(81,785,'其中一块连续矩形为 14.5 × 33.5 mm。',22,secondary)
text(81,827,'升到 0.8 mm 后，面向屏幕的一侧没有合格区域。',21,secondary)
text(797,727,f"3.0 mm 高  ·  {b['area']:.1f} mm²",31,ink,True)
text(797,785,'其中一块连续矩形为 16.25 × 13.75 mm。',22,secondary)
text(797,827,f"升到 4.0 mm 后仅余 {c['area']:.1f} mm²，多为板边窄条。",21,secondary)
line(55,890,1465,890)
text(58,920,'按 0.25 mm 整格统计，已扣除固定、维修和板端预留；器件与障碍保留 0.20 mm 高度间隙。',22,secondary)
text(58,957,'各高度面积彼此包含。色块用于摆放封装，焊盘、走线、电池成品尺寸和射频表现仍需硬件复核。',21,secondary)
sheet.save(Path(sys.argv[2]) if len(sys.argv)>2 else folder/'B-board-space.png')
