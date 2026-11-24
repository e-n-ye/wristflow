"""Matched orthographic plans and sections read directly from the two saved FreeCAD files."""
from os import environ
from functools import lru_cache
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from shapely.geometry import Polygon,box as rect,Point
from review_revision import App,Part,V,BASE,FILE,shadow,shifted,PORT_FACE
import sys

out=Path(sys.argv[1]).resolve()
doc=App.openDocument(str(out/FILE));baseline=App.openDocument(str(BASE))
new={o.Name:o.Shape for o in doc.Objects if hasattr(o,'Shape')}
old={o.Name:o.Shape for o in baseline.Objects if hasattr(o,'Shape')}
W=1500;H=940
bg='#fbfaf6';card='#edf2ee';ink='#243f42';sub='#526b6b';rule='#c9d7d1'
frame='#abbdb7';board='#538672';motor='#90759c';dark='#586c76';cell='#e1bb79'
ant='#b96450';flex='#c78b38';steel='#82949c';mark='#d99a22';soft='#ce8f4d';newcol='#128678';bad='#cc4b42'
def start(title,subtitle):
    global im,p
    im=Image.new('RGB',(W,H),bg);p=ImageDraw.Draw(im)
    txt(40,55,title,37,ink,True);txt(42,109,subtitle,20,sub)
def panel(x,y,xx,yy,title):
    p.rounded_rectangle((x,y,xx,yy),18,fill=card,outline=rule,width=2)
    txt(x+24,y+41,title,24,ink,True)
@lru_cache(None)
def font(size,b):
    return ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),size)
def txt(x,y,s,n=19,c=ink,b=False,anchor='lm'):
    p.text((x,y),s,font=font(n,b),fill=c,anchor=anchor)
def key(x,y,c,label):
    p.rounded_rectangle((x,y-6,x+23,y+7),3,fill=c)
    txt(x+32,y,label,16,sub)
def flatten(g):
    if g.is_empty:return
    if g.geom_type=='Polygon':yield g
    else:
        for subgeom in getattr(g,'geoms',[]):yield from flatten(subgeom)
def paint(g,T,color,outline=None):
    for f in flatten(g):
        coords=lambda wire:[T(a,b) for a,b in wire.coords]
        pts=coords(f.exterior);holes=[coords(h) for h in f.interiors]
        if color:
            m=Image.new('1',im.size);br=ImageDraw.Draw(m);br.polygon(pts,fill=1)
            for h in holes:br.polygon(h,fill=0)
            im.paste(color,mask=m)
        for a in [pts]+holes:p.line(a,fill=outline or '#425956',width=2 if not color else 1)
def plan(sh,win,T,col):
    paint(shadow(sh).intersection(rect(*win)),T,col)
def cutgeom(sh,axis,v,win):
    x,z,xx,zz=win
    pt=lambda a,b:V(v,a,b) if axis=='x' else V(a,v,b)
    plane=Part.Face(Part.makePolygon([pt(a,b) for a,b in [(x,z),(xx,z),(xx,zz),(x,zz),(x,z)]]))
    vals=[]
    for f in sh.common(plane).Faces:
        points=lambda w:[(a.y if axis=='x' else a.x,a.z) for a in w.discretize(Deflection=.008)]
        vals.append(Polygon(points(f.OuterWire),[points(q) for q in f.Wires if not q.isSame(f.OuterWire)]).buffer(0))
    from shapely.ops import unary_union
    return unary_union(vals)
def cut(sh,axis,at,win,T,col):paint(cutgeom(sh,axis,at,win),T,col)
def leader(T,point,end,label,align='lm',color=ink):
    pt=T(*point);xx,yy=end
    p.line([pt,(xx-6 if align=='lm' else xx+6,yy)],fill=color,width=2)
    p.ellipse((pt[0]-3,pt[1]-3,pt[0]+3,pt[1]+3),fill=color)
    txt(xx,yy-12,label,17,color,True,align)
def save(name):im.save(out/name,optimize=True)

start('电机右移，固定孔补回板材','左、右采用同一视向和比例。圆环示螺钉头外缘，孔中留白。下图比较电机本体翻到电池侧后的高度。')
for x,ss,title,web in [(34,old,'原版','0.285 mm'),(760,new,'R2 · 电机向右 1.00 mm','1.597 mm')]:
    panel(x,152,x+706,592,title)
    win=(2.2,-21.65,19.65,-7.10)
    T=lambda a,b:(x+48+(a-2.2)*22.5,240+(-7.10-b)*22.5)
    for k,c in [('Board',board),('MotorSeat','#bbaac1'),('Motor',motor),('MotorWire0','#bd704c'),('MotorWire1','#46575a')]:
        plan(ss[k],win,T,c)
    paint(Point(5.15,-9.8).buffer(1.52),T,None,steel)
    if ss is new:paint(shadow(new['Board']).difference(shadow(old['Board'])).intersection(rect(*win)),T,mark)
    leader(T,(6.36,-10.45),(x+460,286),'孔边 '+web)
    txt(x+462,365,'螺钉头边距',18,sub);txt(x+462,397,'0.005 mm' if ss is old else '1.317 mm',23,ink,True)
    txt(x+462,469,'电机到右壁',18,sub);txt(x+462,501,'1.65 mm' if ss is old else '0.65 mm',23,ink,True)
panel(34,614,1466,883,'电机与电池的层叠 · 沿 Y = −16 mm 剖开')
for x,trial,caption in [(58,False,'采用 · 电机在中框托架上'),(773,True,'试放 · 电机翻到板背，上托架另设')]:
    txt(x,695,caption,19,ink,True)
    T=lambda a,b:(x+16+(a-6.3)*32.5,832-(b-7.30)*15.5)
    win=(6.3,7.30,20.50,14.15)
    for k,c in [('Battery',cell),('Separator','#7faba7'),('Board',board)]: cut(new[k],'y',-16,win,T,c)
    for k,c in (([('TrialUpperMotorSeat','#a4b8b0'),('TrialRearMotor',motor)] if trial else
                 [('MotorSeat',frame),('Motor',motor)])):cut(new[k],'y',-16,win,T,c)
    txt(x,853,'绝缘层净距 0.50 mm，退板到上托架仅 0.054 mm。' if trial else '两根线绕过板边，焊在主板朝后盖的一面。',16,sub)
key(40,911,board,'主板');key(145,911,mark,'补回板材');key(289,911,motor,'电机');key(392,911,cell,'电池')
txt(580,911,'两种电机位置均可装入同一壳厚。翻面方案为独立比较姿态。',17,sub)
save('01-motor-board.png')

start('天线端退板与屏座高度','天线在装好和退板途中分别靠近不同的零件。两帧均沿 X = −13.65 mm 剖开，表带端在左。')
for x,dz,title in [(34,0,'合壳 · 天线端与中框'),(760,-5.9,'主板向后盖退出 5.90 mm · 模组与表耳')]:
    panel(x,151,x+706,537,title);T=lambda a,b:(x+56+(a+26.4)*62,496-(b-4.0)*24)
    win=(-26.4,4.,-17.2,14.2)
    for k,c in [('Frame',frame),('LugLongL','#8098a5')]:cut(new[k],'x',-13.65,win,T,c)
    for k,c in [('ModuleSubstrate',board),('Antenna',ant),('ModuleParts',dark)]:cut(shifted(new[k],dz),'x',-13.65,win,T,c)
    if dz:leader(T,(-22.39,6.7),(x+365,263),'退板侧隙 0.38 mm')
    else:leader(T,(-22.38,12.88),(x+358,243),'合壳最近 0.36 mm')
    txt(x+26,507,'原值 0.025 mm。左表耳内缘局部让位。' if dz else '原值 0.16 mm。模组的焊接位置保持原坐标。',17,sub)
panel(34,555,1466,881,'屏座向外抬 0.45 mm，外框再高 0.15 mm')
for x,ss,title in [(57,old,'原版'),(770,new,'R2')]:
    txt(x,636,title,20,ink,True)
    # A cross section intersects the module and screen-contacted part of the original display flex.
    T=lambda a,b:(x+30+(a+20.55)*24.0,843-(b-11.0)*22.8)
    win=(-20.55,11.,-3.8,19.48)
    for k,c in [('Frame',frame),('Board',board),('ModuleSubstrate','#84978c'),('ModuleParts',dark),
                ('GlassTape','#d6bb75'),('Screen','#a9c0ca'),('FrontRun',flex)]:cut(ss[k],'y',0,win,T,c)
    xx=x+444
    txt(xx,689,'模组到屏背排线',16,sub);txt(xx,722,'0.36 mm' if ss is old else '0.81 mm',21,ink,True)
    txt(xx,781,'屏玻璃低于框边',16,sub);txt(xx,815,'0.015 mm' if ss is old else '0.165 mm',21,ink,True)
txt(40,913,'含后盖光学凸台的总厚从 20.30 增至 20.90 mm。屏胶厚 0.15 mm，原屏排线还需在折返处多释放 0.45 mm。',18,sub)
save('02-antenna-screen.png')

start('表冠的承力面与电池安装耳','表冠位置及两颗压片螺钉的轴线保留。新增耳位包住沉孔，中框承台伸至四个弹片的正上方。')
panel(34,151,733,534,'压片正视 · 固定孔局部')
for x,ss,lab in [(80,old,'原版'),(405,new,'R2')]:
    # Look along +X through the counterbore, drawing the real remaining plate material.
    T=lambda y,z:(x+(y-2.68)*57.5,415-(z-7.35)*48)
    cut(ss['Retainer'],'x',15.55,(2.68,7.35,5.72,10.10),T,'#83ab9e')
    leader(T,(5.08,8.75),(x+62,244),lab)
    txt(x,467,'沉孔破边 0.16 mm' if ss is old else '沉孔外留边 0.38 mm',19,sub)
panel(752,151,1466,534,'四弹片与中框承台 · 穿过 B 触点')
T=lambda x,z:(782+(x-11.4)*60,458-(z-11.15)*61)
win=(11.4,11.15,18.5,14.50)
for k,c in [('Frame',frame),('Board',board),('Daughter',board),('CrownBridge',board),
            ('CrownLeaf2','#c08732'),('CrownPad2','#d4ac52')]:cut(new[k],'y',.95,win,T,c)
leader(T,(14.50,13.93),(1221,260),'中框承台');leader(T,(14.42,13.12),(1243,420),'弹片对接金面')
txt(777,495,'四处接触点的投影均落在承台内，水平小板把弹力传给框。',17,sub)
panel(34,553,1466,881,'托板安装耳向表带端移 0.20 mm')
for x,ss,lab in [(58,old,'原版'),(775,new,'R2')]:
    txt(x,634,lab,20,ink,True)
    T=lambda y,z:(x+12+(y+22.52)*84,826-(z-1.52)*62)
    win=(-22.52,1.52,-18.2,3.94)
    for k,c in [('Frame',frame),('Tray','#6c9896'),('Battery',cell),('TrayScrew1',steel)]:
        cut(ss[k],'x',12.50,win,T,c)
    txt(x+391,684,'下耳至包体',16,sub);txt(x+391,716,'0.030 mm' if ss is old else '0.230 mm',21,ink,True)
    txt(x+391,777,'托板底面留胶',16,sub);txt(x+391,809,'0.05 mm' if ss is old else '0.10 mm',21,ink,True)
txt(40,912,'另一端安装耳余量为 0.175 mm。电池保持 LP503030 成品上限 32 × 30.5 × 5.3 mm，上面有绝缘层及外贴 NTC。',17,sub)
save('03-crown-cell.png')

start('开后盖时，导音垫怎么离开侧孔','四帧使用相同的视向与比例，剖面均取 Y = 14.74 mm，靠近盖螺钉柱。灰绿中框固定，橙色软垫和蓝色喇叭随后盖向下。')
for x,y,ss,dz,title in [(34,150,old,0,'原版 · 合盖'),(760,150,old,-1,'原版 · 开盖 1 mm'),
                       (34,493,new,0,'R2 · 合盖'),(760,493,new,-1,'R2 · 开盖 1 mm')]:
    panel(x,y,x+706,y+322,title)
    T=lambda a,b:(x+42+(a+20.65)*60,y+284-(b-1.65)*60)
    win=(-20.65,1.65,-14.7,5.05)
    cut(ss['Frame'],'y',14.74,win,T,frame)
    for k,c in [('Speaker','#7f929d'),('Outlet',soft)]:cut(shifted(ss[k],dz),'y',14.74,win,T,c)
    if dz and ss is old:
        overlap=shifted(ss['Outlet'],-1).common(ss['Frame'])
        cut(overlap,'y',14.74,win,T,bad)
    lines=(['原软垫伸入侧孔。','最近间隙 0.020 mm。'] if ss is old and dz==0 else
           ['红色为相交部分。','整件重叠 0.491 mm³。'] if ss is old else
           ['短软垫贴在框的平面。','整圈接触 1.753 mm²。'] if dz==0 else
           ['软垫沿平面下滑。','整件刚体重叠为 0。'])
    for i,s in enumerate(lines):txt(x+426,y+155+37*i,s,17,ink if i==0 else sub,i==0)
    if dz:
        p.line([(x+517,y+216),(x+517,y+265)],fill=newcol,width=4)
        p.polygon([(x+510,y+255),(x+524,y+255),(x+517,y+271)],fill=newcol)
key(42,863,frame,'固定中框与声道');key(247,863,soft,'随盖软垫');key(407,863,bad,'原版干涉')
txt(628,863,'本剖面经过垫的侧唇；完整出声通道位于剖面后方。',17,sub)
txt(42,909,'R2 的侧孔位置沿用原版。0–6 mm 刚体扫掠通过，软垫的预压、复位和漏音仍需实物验证。',18,sub)
save('04-speaker-opening.png')
App.closeDocument(doc.Name);App.closeDocument(baseline.Name)
