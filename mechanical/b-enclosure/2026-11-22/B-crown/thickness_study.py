"""Read-only thickness budget for the saved R2 assembly. Candidates are envelope trials."""
from os import environ
from pathlib import Path
import json
from PIL import Image,ImageDraw,ImageFont
from shapely.geometry import Polygon
from review_revision import App,Part,V,HERE,FILE,block,shifted,gap,hit

folder=HERE/'B-R2-thickness'
folder.mkdir(exist_ok=True)
doc=App.openDocument(str(HERE/'B-review-R2'/FILE))
S={o.Name:o.Shape for o in doc.Objects if hasattr(o,'Shape')}
Z=lambda v:V(0,0,v)

def cut_straight(shape,at,amount):
    a=shape.common(block(-48,-48,-20,96,96,at+20))
    b=shifted(shape.common(block(-48,-48,at+amount,96,96,60)),-amount)
    c=a.fuse(b).removeSplitter()
    assert c.isValid() and len(c.Solids)==1
    return c

def trial(drop,front,optic):
    M=lambda k:shifted(S[k],-drop)
    display=shifted(S['FrontRun'],-drop-front)
    crossing=shifted(cut_straight(S['Crossing'],13.10,front),-drop)
    main_flex=cut_straight(S['LidFlexNew'],5.15,drop)
    open_flex=cut_straight(S['Open6LidFlexNew'],5.15,drop)
    daughter=S['Daughter'].common(block(-40,-40,-5,80,80,18.125-drop))
    new_bridge=M('CrownBridge')
    assert hit(new_bridge,daughter)<1e-6 and gap(new_bridge,daughter)<1e-5
    for i in range(4):
        assert gap(M('CrownLeaf'+str(i)),M('CrownPad'+str(i)))<1e-5
    for terminal,other in [(crossing,M('RearRun')),(crossing,display),(main_flex,M('RearFingers')),
                           (open_flex,M('RearFingers'))]:
        assert gap(terminal,other)<1e-5
    for k in ('Board','ModuleParts','ModuleSubstrate'):
        assert hit(M(k),S['Separator'])<1e-5 and hit(M(k),display)<1e-5
    for k in ('Speaker','Battery','Tray','Separator'):
        assert hit(main_flex,S[k])<1e-5
    for k in ('ModuleParts','ModuleSubstrate','Board','CrownBridge','Motor','MotorWire0','MotorWire1','RearSocket'):
        assert hit(main_flex,M(k))<1e-5 and hit(open_flex,M(k))<1e-5
    a=shifted(S['Optic0'],optic)
    n=gap(a,S['OpticalDice'])
    assert n>0
    return dict(board_shift=drop,display_extra_shift=front,optical_extension_reduction=optic,
        total=20.90-drop-front-optic,flat_back_to_rim=19.40-drop-front,
        optical_bump=1.5-optic,cell_insulator_to_board=11.30-drop-7.75,
        module_to_display_FPC=gap(M('ModuleParts'),display),window_to_optical_chip=n,
        display_stiffener_to_NTC=gap(M('Stiffener'),S['SurfaceNTC']),
        motor_lead_to_cell_insulator=min(gap(M('MotorWire'+str(i)),S['Separator']) for i in range(2)),
        parked_wire_insulator_overlap_mm3=hit(M('ParkedMotorWire0'),S['Separator']),
        standing_contact_board_top=13.125-drop)

data={'R2_body':19.40,'R2_maximum':20.90,'trial_1':trial(1.40,.30,.30),'trial_2':trial(1.80,.40,.50)}
ray=Part.makeLine(V(0,0,-2),V(0,0,20))
sample={}
for name in ('Baffle','PpgMount','LidFlexNew','PpgStiffAdhesive','PpgStiffener','Tray','Battery','Separator',
             'Board','ModuleSubstrate','ModuleParts','Screen','ActiveArea'):
    edges=S[name].common(ray).Edges
    sample[name]=[[round(e.BoundBox.ZMin,3),round(e.BoundBox.ZMax,3)] for e in edges]
data['centre_sections_z_mm']=sample
for name in ('trial_1','trial_2'):data[name]={k:round(v,4) for k,v in data[name].items()}
(folder/'section-measurements.json').write_text(json.dumps(data,indent=2),encoding='utf8')

im=Image.new('RGB',(1550,1050),'#fbfaf6')
d=ImageDraw.Draw(im);ink='#203c41';grey='#52686a';light='#edf2ef'
def txt(x,y,s,n=19,col=ink,b=False,anchor='lm'):
    f=ImageFont.truetype((str(Path(environ['SystemRoot']) / 'Fonts') + '/')+('msyhbd.ttc' if b else 'msyh.ttc'),n)
    d.text((x,y),s,fill=col,font=f,anchor=anchor)
def panel(x,y,X,Y):d.rounded_rectangle((x,y,X,Y),18,fill=light,outline='#cdd9d4',width=2)
def section(sh,y):
    plane=Part.Face(Part.makePolygon([V(x,y,z) for x,z in [(-21,-2),(21,-2),(21,20),(-21,20),(-21,-2)]]))
    for f in sh.common(plane).Faces:
        wire=lambda w:[(v.x,v.z) for v in w.discretize(Deflection=.01)]
        g=Polygon(wire(f.OuterWire),[wire(w) for w in f.Wires if not w.isSame(f.OuterWire)]).buffer(0)
        if g.geom_type=='Polygon' and not g.is_empty:yield g
def paint(sh,y,T,col):
    for q in section(sh,y):
        pts=[T(x,z) for x,z in q.exterior.coords];m=Image.new('1',im.size);br=ImageDraw.Draw(m);br.polygon(pts,fill=1)
        for h in q.interiors:br.polygon([T(x,z) for x,z in h.coords],fill=0)
        im.paste(col,mask=m);d.line(pts,fill='#435856',width=1)
def line(x,y,X,Y,c,w=2):d.line((x,y,X,Y),fill=c,width=w)
txt(42,53,'20.90 mm 分布在哪里',41,b=True)
txt(43,109,'两处均取自 R2 的闭合装配，视线沿表带方向。左右两图使用相同比例，竖直方向为厚度。',21,grey)
colors={'Frame':'#b6c4bf','Screen':'#a5bec9','ActiveArea':'#a5bec9','GlassTape':'#d7bc89','Board':'#49836e',
    'ModuleSubstrate':'#87988f','ModuleParts':'#576f79','FrontRun':'#d19142','Battery':'#e4ba75','CellTape':'#c8ad7e',
    'Separator':'#93abad','Tray':'#6f9698','Back':'#b6c4bf','Baffle':'#a5b8b4','PpgMount':'#899d98',
    'PpgStiffener':'#6f9698','PpgStiffAdhesive':'#b79a6a','LidFlexNew':'#d19142','Speaker':'#9783a2','SpeakerBed':'#b5a4b7',
    'CrownBridge':'#49836e','Daughter':'#49836e','RearSocket':'#526b6e','BatPlug':'#8d8171','BatHeader':'#cab39b',
    'Optic0':'#80bec2','Optic1':'#80bec2','Optic2':'#80bec2','OpticalDice':'#527b7c',
    'GuideSleeve':'#70989d','CrownShaft':'#c1a165','EncoderBody':'#977f98','Retainer':'#8ca2a4'}
order=['Frame','Back','Baffle','PpgMount','PpgStiffAdhesive','PpgStiffener','LidFlexNew','SpeakerBed','Speaker','Tray',
       'CellTape','Battery','Separator','BatPlug','BatHeader','RearSocket','Board','ModuleSubstrate','ModuleParts',
       'CrownBridge','Daughter','GuideSleeve','CrownShaft','EncoderBody','Retainer','Optic0','Optic1','Optic2',
       'OpticalDice','GlassTape','Screen','ActiveArea','FrontRun']
for x,y,title in [(32,0,'中央剖面 · 电池与思澈模组重叠'),(787,14,'靠表带端剖面 · 喇叭与电池错开')]:
    panel(x,153,x+731,671);txt(x+24,194,title,24,b=True)
    T=lambda a,z:(x+365+a*16.1,569-z*16.1)
    for k in order:paint(S[k],y,T,colors[k])
    if y==0:
        for a,z,l,c in [(-1,4.9,'电池',ink),(-5.4,14.0,'模组','white'),(-1.0,9.50,'3.55 mm',ink),(0,17.80,'屏组件',ink)]:
            txt(*T(a,z),l,16,c,True,'mm')
        txt(x+24,626,'3.55 mm 是绝缘层到裸板的高度，局部还要容纳排线头和器件。',16,grey)
    else:
        txt(*T(-8.8,3.45),'喇叭',17,'white',True,'mm')
        txt(*T(0,17.80),'屏组件',16,ink,True,'mm')
        txt(x+24,626,'喇叭本体厚 4.30 mm，位于电池旁边，和电池厚度并列。',17,grey)
panel(32,698,1518,990)
txt(57,737,'保留原屏与电池的尺寸预算',26,b=True)
headers=[('方案',58),('主板向后盖移',331),('屏座再降',584),('光学凸台收回',817),('壳体厚',1074),('含凸台最厚',1301)]
for label,x in headers:txt(x,793,label,18,grey)
line(57,817,1490,817,'#c9d6cf')
for y,row in [(852,['R2 现状','—','—','—','19.40','20.90 mm']),
              (903,['建议先试','1.40','0.30','0.30','17.70','18.90 mm']),
              (954,['更激进的试放','1.80','0.40','0.50','17.20','18.20 mm'])]:
    for i,(value,(_,x)) in enumerate(zip(row,headers)):txt(x,y,value,19,'#126859' if i==5 else ink,b=i==5)
txt(42,1021,'尺寸单位为 mm。两档候选均需联动主板支承、表冠接触板和排线；电池托板及电池位置均保持 R2 高度。',18,grey)
im.save(folder/'R2-section-budget.png',optimize=True)
App.closeDocument(doc.Name)
