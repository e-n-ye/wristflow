"""Two views of the same through-board display connection; explanatory sections are not to scale."""
from os import environ
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json, math
HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument('output',type=Path,help='Local output PNG')
output=parser.parse_args().output
cfg=json.loads((HERE/'config.json').read_text(encoding='utf8'))
im=Image.new('RGB',(1400,690),'white')
d=ImageDraw.Draw(im)
ink='#20353d';muted='#49616b';c1='#a45b27';c2='#257b83';conn='#334f70';board='#c4d1d0'
def label(x,y,s,size=22,col=ink,bold=False):
    font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if bold else 'msyh.ttc'),size)
    d.text((x,y),s,font=font,fill=col,anchor='lt')
def line(path,col,width):d.line(path,fill=col,width=width,joint='curve')
def arrow(x,y,dx,dy,col):
    n=math.hypot(dx,dy);dx/=n;dy/=n
    d.polygon([(x,y),(x-15*dx+7*dy,y-15*dy-7*dx),(x-15*dx-7*dy,y-15*dy+7*dx)],fill=col)
label(48,36,'屏尾如何穿过主板并扣到 CN1',40,ink,True)
label(49,99,'左图沿用你的屏背照片方向。右图把折返处单独剖开，画出窄尾换到主板另一面的过程。',22,muted)
line([(48,147),(1352,147)],'#cdd8da',2)
label(52,174,'从屏背看排线',27,ink,True)
d.rounded_rectangle((89,231,590,545),radius=41,fill='#f1f4f4',outline='#92a4a8',width=3)
d.rounded_rectangle((300,274,506,308),radius=8,fill='white',outline='#71888b',width=3)
# Fixed segment lies under the PCB; the returning narrow tail is on its battery-facing surface.
line([(157,491),(157,389),(418,389),(418,291)],c1,12)
line([(418,291),(418,347),(168,347)],c2,10)
arrow(320,389,1,0,c1);arrow(254,347,-1,0,c2)
d.rounded_rectangle((147,317,170,377),radius=4,fill=conn,outline='#223649',width=2)
label(102,259,'左侧扣座',19,conn);line([(147,284),(156,314)],conn,2)
label(336,240,'长方穿线窗',19,muted);label(191,424,'贴屏背的宽段',20,c1)
label(206,309,'回折窄尾',20,c2);label(113,505,'根部',18,c1)
line([(681,180),(681,560)],'#cdd8da',2)
label(723,174,'在孔边看剖面',27,ink,True)
label(758,237,'屏组件背面',20,muted)
d.rounded_rectangle((756,273,1320,293),4,fill='#5f6c71')
line([(803,318),(1172,318)],c1,10);arrow(1008,318,1,0,c1)
d.rectangle((756,366,1080,394),fill=board,outline='#73888d',width=2)
d.rectangle((1238,366,1320,394),fill=board,outline='#73888d',width=2)
label(774,370,'主板 0.80 mm',16,ink)
arc=[(1172+57*math.sin(i*math.pi/70),375-57*math.cos(i*math.pi/70)) for i in range(71)]
line(arc,'#b97a2b',10)
line([(1172,432),(873,432)],c2,10);arrow(1020,432,-1,0,c2)
d.rectangle((842,395,902,414),fill=conn)
d.rectangle((849,415,895,431),fill='#ba985b')
label(759,483,'电池一侧',20,muted);label(930,482,'窄尾末端向板面扣合',20,conn)
line([(920,489),(877,432)],conn,2)
line([(48,568),(1352,568)],'#cdd8da',2)
w,h=cfg['slot_size']
label(48,592,f'原屏板的穿线窗 {w:g} × {h:g} mm。CN1 为 24 针、0.35 mm 间距的板对板母座。',23,ink,True)
label(48,642,'走向已由实物确认。图中带宽、折弯半径和扣合余量仅作示意，孔与座相对玻璃的定位仍需复核。',20,muted)
im.save(output)
