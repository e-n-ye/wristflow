"""Dimensioned screen-side view linked to the exported board and insertion study."""
from os import environ
import json
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root=Path(sys.argv[1])
r=json.loads((root/'slot-check.json').read_text(encoding='utf8'))
w,h,rad=r['width_height_radius']
keep_width=h==3 and w==13.6
cx,cy=r['slot_xy']
width,height=1500,915
image=Image.new('RGB',(width,height),'white')
draw=ImageDraw.Draw(image)
ink='#18343d';muted='#496369';board='#d1dedb';teal='#216e68';gold='#c98b35'
panel='#f0f5f4';rule='#b2c6c4'
def text(x,y,word,size=21,color=ink,bold=False,anchor='lm'):
    font=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if bold else 'msyh.ttc'),size)
    draw.text((x,y),word,font=font,fill=color,anchor=anchor)
def stroke(points,color,width=2):
    draw.line(points,fill=color,width=width,joint='curve')
def xy(x,y):
    return 356+x*10.8,435-y*10.8
def callout(x,y,n,line_end,tx,ty,label):
    pt=xy(x,y)
    stroke([pt,line_end],teal,3)
    draw.ellipse((pt[0]-3,pt[1]-3,pt[0]+3,pt[1]+3),fill=teal)
    a,b=line_end
    draw.ellipse((a-13,b-13,a+13,b+13),fill=teal)
    text(a,b,n,15,'white',True,'mm')
    text(tx,ty,label,17,ink)

text(44,49,'B 版主板 · 原屏穿线窗与板边缺口',40,ink,True)
text(46,105,'屏幕侧正视，表冠在右。右图放大同一个孔，尺寸单位为 mm。',22,muted)
draw.rounded_rectangle((40,146,738,781),16,fill=panel)
text(65,176,'板边缺口',25,ink,True)
loops=r['outline_mm']
for i,ring in enumerate(loops):
    pts=[xy(x,y) for x,y in ring]
    draw.polygon(pts,fill=board if i==0 else 'white')
    stroke(pts,ink,3)
x,y=xy(cx,cy)
text(x,y,'穿线窗',14,ink,True,'mm')
# The board-mounted socket stays on the other side. Draw only its projection.
cnx,cny=r['cn1_xy']
u,v=xy(cnx,cny)
draw.rounded_rectangle((u-10,v-37,u+10,v+37),4,fill='#809995')
stroke([(u-10,v),(u-25,v),(u-40,v+16)],muted,2)
text(u-46,v+18,'背面 CN1',16,muted,False,'rm')
callout(17.36,14,'1',(603,244),623,243,'后盖螺柱')
callout(16.55,11.1,'5',(599,318),620,318,'托板支座')
callout(17.25,0,'2',(603,441),623,441,'表冠轴')
callout(17.36,-14,'3',(603,594),623,594,'后盖螺柱')
callout(-12.50,-20.05,'4',(174,703),193,702,'电池托板支座')
text(65,751,'①–④ 对应红箭头，⑤ 是紧邻上圆弧的小方口。',18,muted)
text(781,176,'原屏板的开窗是 14 × 3，R0.5' if keep_width else '从 14 × 3 缩到 9 × 2.5',27,ink,True)
text(782,223,'短边沿用 3 mm。对照试样只在远离 CN1 的一端收 0.40 mm。' if keep_width else '保留原窗左端，右端回收 5 mm，上下各收 0.25 mm。',19,muted)
scale=38;ax,ay=835,384
def slot_box(left,centre_y,width_mm,height_mm):
    return (left,centre_y-height_mm*scale/2,left+width_mm*scale,centre_y+height_mm*scale/2)
draw.rounded_rectangle(slot_box(ax,ay,14,3),radius=rad*scale,fill='#e1e7e8',outline='#758a8d',width=3)
new_left=ax+((14-w)*scale if keep_width else 0)
draw.rounded_rectangle(slot_box(new_left,ay,w,h),radius=rad*scale,fill='#e5f4ee',outline=teal,width=4)
head=r['head_nominal_xyz_mm'][0]
if keep_width:
    text(new_left+26,ay,'远离 CN1 端',17,muted)
    text(ax+14*scale-20,ay,'靠 CN1 端',17,teal,True,'rm')
else:
    projection=r['short_side_study'][str(h)]['angles_deg']['75']['projected_short_mm']
    sx=ax+(w-head)*scale/2
    draw.rectangle(slot_box(sx,ay,head,projection),fill='#f3d59c',outline=gold,width=2)
    text(ax+w*scale/2,ay,'插头连补强 · 75° 投影',16,ink,True,'mm')
for start,ydim,ww,col,label in [(new_left,ay-91,w,teal,f'{w:g}'),(ax,ay+98,14,muted,'14.0')]:
    end=start+ww*scale
    stroke([(start,ydim),(end,ydim)],col,2)
    for xp in (start,end):stroke([(xp,ydim-7),(xp,ydim+7)],col,2)
    text((start+end)/2,ydim-17,label,18,col,True,'mm')
for x0,y0,col,msg in [(783,540,teal,f'试样 {w:g} × {h:g} · R0.5'),(1120,540,'#758a8d','原窗 14 × 3 · R0.5')]:
    draw.rounded_rectangle((x0,y0-8,x0+19,y0+9),3,fill=col)
    text(x0+28,y0,msg,19)
if keep_width:
    text(782,601,'孔要给整条屏尾的装拆让位',23,ink,True)
    text(782,645,'穿过的是屏尾插头及补强，之后才扣上板载 CN1。照片中',19,muted)
    text(782,681,'靠 CN1 端能看到黑色部分，需按实物保留它的活动空间。',19,muted)
    text(782,733,'远端小改试样：折返段距板 0.57 mm，穿头通道距板 0.29 mm。'.replace('：',' · '),18,teal,True)
else:
    text(782,601,'穿孔件取屏尾插头及补强最大外廓',23,ink,True)
    text(782,645,'本次取 7.80 × 3.65 × 0.75 的估计包络。长边对准窗长，',19,muted)
    text(782,681,'从屏侧送入，再在电池侧与 CN1 扣合。',19,muted)
    value=r['short_side_study'][str(h)]['fold_gap_mm']
    text(782,733,f'合盖后，折返排线距孔边的模型最小间隙为 {value:.2f} mm。',19,teal,True)
stroke([(45,813),(1455,813)],rule)
text(46,846,f'试样补回板材 {14*3-w*h:.2f} mm²。四个红箭头处逐一补平试算，沿后盖方向取板时均与固定结构相交。',21,ink,True)
text(46,886,'这组尺寸用于试装。原屏插头外廓、公差和带线连续翻转仍需用实物确认。',18,muted)
image.save(root/'B-slot-notches.png')
