"""Build a mechanical review from the verified B assembly. Millimetres, rear opening towards -Z."""
import hashlib
import json
import sys
from pathlib import Path
import FreeCAD as App
import FreeCADGui as Gui
import Part

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT.parent))
from workspace import WORKSPACE
sys.path.insert(0, str(ROOT / "source"))
from target_model import V, box, rounded, cylinder, aim

BOARD = 11.30
CUT_X = 9.0
GREY = (.32,.38,.41)
STEEL = (.65,.71,.73)
PEACH = (.92,.52,.28)
TEAL = (.08,.55,.52)
BLUE = (.22,.48,.75)
GOLD = (.88,.67,.18)
CREAM = (.90,.84,.64)
RED = (.80,.21,.20)

def ring(w,l,h,r,wi,li,ri,z):
    return rounded(w,l,h,r,z=z).cut(rounded(wi,li,h+.2,ri,z=z-.1))

def wire(points,r):
    segments=[]
    for a,b in zip(points,points[1:]):
        v=V(*b)-V(*a)
        segments.append(Part.makeCylinder(r,v.Length,V(*a),v))
    segments.extend(Part.makeSphere(r,V(*pt)) for pt in points[1:-1])
    return segments[0].multiFuse(segments[1:]).removeSplitter()

def down_prisms(shape, travel):
    """Continuous axial sweep of downward-facing caps, for the prismatic parts checked here."""
    swept=[shape]
    for face in shape.Faces:
        if isinstance(face.Surface, Part.Plane) and face.normalAt(0,0).z < -.999:
            swept.append(face.extrude(V(0,0,-travel)))
    return Part.makeCompound(swept)

def run(dest):
    dest=Path(dest)
    model_dir=WORKSPACE/ROOT.name
    original=model_dir/'WristFlow-B.FCStd'
    digest=hashlib.sha256(original.read_bytes()).hexdigest()
    assert digest==json.loads((model_dir/'validation-B.json').read_text(encoding='utf8'))['sha256']
    Gui.activateWorkbench('PartWorkbench')
    src=App.openDocument(str(original))
    doc=App.newDocument('Bservice')
    doc.Label='WristFlow B · 开盖维修剖面'
    groups={}
    for key,label in [('Fixed','中框、屏幕与主板承力面'),('Circuit','主板 / 硬件接口候选位置'),('Fasteners','主板固定螺钉'),('Cell','可拆电池与托板'),('Mated','三线活动插头与线束'),('Lid','可拆后盖、喇叭和 PPG'),('Jig','开盖定位夹具 · 串口'),('Meter','选装六针测流插块'),('Tools','插拔工具包络与行程'),('Slice','纵剖面 X = +9 mm · 默认'),('LidAside','移开的后盖 · 开盖视图')]:
        groups[key]=doc.addObject('App::DocumentObjectGroup',key)
        groups[key].Label=label
    for key,label in [('Links','日常模式 · 三路独立短接帽'),('FreeHarness','拔出状态 · 同一线束')]:
        groups[key]=doc.addObject('App::DocumentObjectGroup',key)
        groups[key].Label=label
    parts={}; owner={}
    def add(k,label,s,g,color,note='本次设计占位，尺寸单位 mm'):
        assert not s.isNull() and s.Volume>1e-7 and s.isValid(),k
        o=doc.addObject('Part::Feature',k);o.Label=label;o.Shape=s
        o.addProperty('App::PropertyString','DesignBasis','Review').DesignBasis=note
        o.ViewObject.ShapeColor=color;o.ViewObject.LineWidth=1
        o.ViewObject.LineColor=(.12,.15,.17)
        groups[g].addObject(o);parts[k]=o;owner[k]=g
        return o
    def inherit(k,g,delta=(0,0,0)):
        o=src.getObject(k)
        return add(k,o.Label,o.Shape.translated(V(*delta)),g,o.ViewObject.ShapeColor[:3],
                   getattr(o,'Basis','沿用 B 模型'))
    def save_report():
        (dest/'geometry.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    for k in ['Screen','ActiveArea','Crown','CrownShaft']:
        inherit(k,'Fixed')
    for side in ['Short','Long']:
        for key in ['Lug'+side+'L','Lug'+side+'R','Bar'+side,'BandRoot'+side,'Release'+side]:
            inherit(key,'Fixed')
    for k in ['Back','Baffle','Optic0','Optic1','Optic2','PpgBoard','Contact0','Contact1']:
        inherit(k,'Lid')
    for i in range(4):
        inherit('CoverScrew'+str(i),'Lid')
    # Cut an adhesive rebate in the previous front ledge; the original front glass outer face stays put.
    frame=src.MiddleFrame.Shape.copy()
    recess=ring(35.15,41.51,.17,5.03,32.82,39.14,3.78,12.88)
    frame=frame.cut(recess)
    adhesive=ring(34.95,41.27,.15,5.02,32.98,39.32,3.86,12.88)
    add('GlassTape','屏周边模切胶 · 0.15 mm',adhesive,'Fixed',(.85,.36,.48),'胶承在台阶，仅作名义试装；实际粘接边与厂商须核')
    pcb=src.MainPCB.Shape.translated(V(0,0,-.45))
    # Translate module and antenna notch together, to separate the speaker outline from the antenna.
    pcb=rounded(36.8,43.2,.8,4.6,z=BOARD).cut(box(18.3,12,2,-7.9,20.55,11.0))
    for k in ['ModuleParts','ModuleSubstrate','Antenna']:
        inherit(k,'Circuit',(-.9,0,-.45))
    # M1.2-class bosses enter clearance holes. Their shoulders touch the front PCB face.
    # No mounting load is applied to the display. Screw length and thread strength are provisional.
    hole_locs=[(9,-18.5),(-13.6,-10.8),(15.5,8.4)]
    for i,(x,y) in enumerate(hole_locs,1):
        pilot=cylinder(.52,1.45,(x,y,BOARD-.1))
        boss=cylinder(1.12,1.58,(x,y,BOARD)).cut(pilot)
        shoulder=cylinder(1.96,.70,(x,y,BOARD+.8)).cut(pilot)
        if i==1:
            arm=box(3.1,22.2+y,.70,x,(-22.2+y)/2,BOARD+.8)
        else:
            end=19.05 if x>0 else -19.05
            arm=box(abs(end-x),3,.70,(x+end)/2,y,BOARD+.8)
        frame=frame.fuse(boss).fuse(shoulder).fuse(arm.cut(pilot))
        pcb=pcb.cut(cylinder(1.24,1.4,(x,y,BOARD-.1)))
        screw=cylinder(1.52,.60,(x,y,BOARD-.6)).fuse(cylinder(.47,1.22,(x,y,BOARD)))
        add('Clamp'+str(i),'主板压紧螺钉 '+str(i),screw,'Fasteners',STEEL,'从后盖方向操作；Ø2.48 板孔与 Ø2.24 凸肩，名义螺纹啮合 1.22，需选标准件及承载验证')
    # Probe force closes through this frame rib, not the glass.
    support=box(14.6,3.5,.65,8.75,20.40,BOARD+.8).fuse(box(5.8,6.8,.65,9,17.35,BOARD+.8))
    support=support.fuse(box(3.6,8.3,.65,15,17.15,BOARD+.8))
    frame=frame.fuse(support)
    # Battery position retains B's footprint. Relocate one tray lug so the module has a rear exit.
    cell=inherit('Battery','Cell',(0,0,-.15))
    bz=cell.Shape.BoundBox.ZMin;tray=rounded(35.65,30.65,.4,.9,0,-4.9,bz-.45)
    for x in [-19,19]:
        tray=tray.cut(cylinder(1.61,1,(x,-14,bz-.6)))
    tray_locs=[(-12.5,-20.77),(17.5,10.96)]
    for i,(x,y) in enumerate(tray_locs,1):
        through=cylinder(.60,2.3,(x,y,bz-1))
        if i==1:
            extension=box(3.2,2.30,.4,x,-20.35,bz-.45)
            shelf=box(3.45,2.2,1.05,x,-21.60,bz-.05).cut(through)
            pcb=pcb.cut(box(4.05,3.0,1.4,x,-21.55,BOARD-.2))
        else:
            extension=rounded(1.75,2.0,.4,.27,x,y-.28,bz-.45)
            shelf=box(2.30,1.45,1.05,18.05,y+.10,bz-.05).cut(through)
            pcb=pcb.cut(box(3.25,2.15,1.4,18.17,y+.10,BOARD-.2))
        tray=tray.fuse(extension).cut(through)
        frame=frame.fuse(shelf)
        add('TrayScrew'+str(i),'电池托板螺钉 '+str(i),cylinder(.98,.45,(x,y,bz-.9)).fuse(cylinder(.50,1.4,(x,y,bz-.45))),'Cell',STEEL)
    add('Tray','可拆电芯承托板',tray.removeSplitter(),'Cell',CREAM)
    # Motor remains on its own frame shelf. A PCB scallop clears both when lifting the board.
    for k in ['Motor','MotorSeat']:
        inherit(k,'Fixed',(0,0,-.5))
    motor_cut=box(13.2,11.15,1.4,13.15,-5,BOARD-.2)
    pcb=pcb.cut(motor_cut).cut(box(2.3,2.35,1.4,18.35,0,BOARD-.2))
    for x in [-19,19]:
        for y in [-14,14]:
            pcb=pcb.cut(cylinder(1.66,1.4,(x,y,BOARD-.2)))
    # Sleeve for the speaker is carried by the back. The original side opening is plugged and recut lower.
    frame=frame.fuse(box(1.8,4.8,1,19.65,15.8,6.0)).cut(box(4.9,4.2,1,20,16.0,3.20))
    add('Frame','中框 · 含屏台阶、主板凸肩、托板支座',frame.removeSplitter(),'Fixed',GREY)
    add('Board','主板 · 四处盖柱避让 / 电机与托架退板口',pcb.removeSplitter(),'Circuit',(.18,.54,.42))
    # A screen BTB separated along the board normal. The proxy is retained pending actual panel measurements.
    add('ScreenSocket','24P 板座 · 接合高度预算 0.70',box(6.85,1.94,.50,7,-13.4,BOARD+.8),'Circuit',GOLD)
    add('ScreenTail','屏端插头与排线补强 · 位置待核',box(6.85,1.94,.20,7,-13.4,BOARD+1.30),'Fixed',PEACH)
    add('ScreenFlex','屏尾软排线 · 方向与半径待实物',box(5.4,6.6,.10,7,-11.0,BOARD+1.50),'Fixed',(.80,.49,.18))
    # Molex 781710003 / 781720003. The housing is a clearance illustration, not vendor tooling geometry.
    cx,cy=9.,15.45
    outline=rounded(4.53,5.40,1.08,.22,cx,cy,BOARD-1.08)
    pocket=box(3.62,4.58,.86,cx,cy,BOARD-1.10)
    add('BatHeader','三芯电池板座 · 781710003 包络',outline.cut(pocket),'Circuit',CREAM,
        'Molex 781710003 / 781720003；总接合高 1.55，平面 5.40×4.53；壳体分界为示意')
    plug=box(3.54,4.48,.74,cx,cy,BOARD-1.02).fuse(rounded(4.45,5.24,.53,.18,cx,cy,BOARD-1.55))
    add('BatPlug','三芯活动线壳 · 从后侧垂直分离',plug,'Mated',(.94,.71,.32),'3 条线一起沿接合轴操作；拟配 28 AWG 压接线束，针序/NTC 阻值依到手电池')
    # Retain adequate room to turn at the battery edge. Curves are indicative wire centrelines.
    wire_data=[]
    for i,(name,col) in enumerate([('BAT+',RED),('NTC',GOLD),('BAT−',(.22,.23,.24))]):
        off=(i-1)*.66
        points=[(cx+2.19,cy+off,BOARD-1.32),(12.05+i*.29,cy+off,BOARD-1.37),
                (12.12+i*.29,13.80,9.15),(12.05+i*.31,11.25,8.0),(11.6+i*.54,10.06,6.74)]
        add('Lead'+str(i),name+' · 线束路径假设',wire(points,.19),'Mated',col)
        wire_data.append((points,col,name))
    add('Speaker','1511 带腔喇叭 · 后盖承载',rounded(15,11,4.3,.82,8.90,16.0,1.18),'Lid',BLUE)
    lip=rounded(15.2,11.05,.28,.90,8.82,15.94,.9)
    add('SpeakerBed','喇叭定位底托与固定胶位置',lip,'Lid',TEAL)
    gasket=box(2.30,3.95,.96,17.55,16,3.22)
    add('Outlet','侧孔导音密封垫 · 后盖带走',gasket.cut(box(2.5,3.35,.57,17.55,16,3.41)),'Lid',(.38,.49,.49),'装配压缩、出声朝向和盖开方向的侧唇待核')
    # A rear electrical island makes the lid removable without a dangling service cable.
    # The flex runs ON the rear subassembly; paired spring fingers disconnect in the normal direction.
    tower=box(.95,2.0,4.88,17.36,16.23,.9)
    tower=tower.fuse(box(11.23,2.0,.60,12.24,16.2,5.78)).fuse(box(2.3,5.65,.60,7.0,13.7,5.78))
    tower=tower.fuse(box(8.50,2.24,3.94,7.1,11.55,6.38))
    add('LidTower','后盖接口支架 · 支承在后盖',tower.removeSplitter(),'Lid',TEAL)
    # Schematic flex strip (does not model exact bend radii, not a released flex design).
    flex=box(3.5,7.5,.10,-2,7.85,1.55)
    flex=flex.fuse(box(3.5,.12,4.75,-2,11.55,1.55)).fuse(box(11.1,1.02,.10,1.75,11.74,6.22))
    add('LidFlex','背盖 PPG 柔性板 · 示意路径',flex,'Lid',(.73,.44,.13),'PPG 与光窗固定在后盖；软排线走至接触岛。铜与覆盖膜厚、静态弯折、声道及充电走线留给硬件')
    signals=['光学供电','LED 供电','信号地','SCL','SDA','INT','SPK+','SPK−','CHG IN','CHG GND']
    for i,signal in enumerate(signals):
        x=3.9+(i%5)*1.60;y=11.05+(i//5)*1.16
        add('LPad'+str(i),signal+' · 主板后盖接点',cylinder(.40,.045,(x,y,BOARD-.045)),'Circuit',GOLD)
        add('LFinger'+str(i),signal+' · 后盖弹触占位',cylinder(.26,BOARD-.045-10.32,(x,y,10.32)),'Lid',GOLD,'独立弹触，电流额定、行程、抗汗及接地顺序待选型；工程建议分开大电流回路')
    padpos=[]
    for i,sig in enumerate(['GND','VTref · 只检测','PA18 / RX','PA19 / TX','电源循环 / RTS']):
        x=3.+2*i;y=20.0
        add('DBG'+str(i),sig,cylinder(.57,.05,(x,y,BOARD-.05)),'Circuit',GOLD)
        padpos.append((x,y,'D'+str(i)))
    for i,label in enumerate(['电池整机','OLED','屏侧 3.3 V']):
        y=14.38+i*1.78
        for side,x in enumerate([14.10,15.90]):
            add('I'+str(i)+str(side),label+(' 电源侧' if not side else ' 负载侧'),cylinder(.51,.05,(x,y,BOARD-.05)),'Circuit',GOLD)
            padpos.append((x,y,'I'+str(i)+str(side)))
    # A removable shorting key with three independent jumpers. Housing and sockets need hardware selection.
    keybody=rounded(3.70,5.12,.52,.29,15,16.16,9.42)
    add('KeyBody','三路短接帽 · 绝缘提手',keybody,'Links',(.64,.58,.77),'维修结构提案；接触电阻、锁止力、额定电流和静态压降须由硬件选型')
    for i in range(3):
        y=14.38+i*1.78
        short=box(2.08,.48,.14,15,y,9.94)
        for j,x in enumerate([14.10,15.90]):
            sleeve=cylinder(.425,.72,(x,y,BOARD-.77)).cut(cylinder(.325,.76,(x,y,BOARD-.79)))
            add('Sleeve'+str(i)+str(j),'测流端子 / 短接触点 · 接口占位',sleeve,'Circuit',GOLD)
            short=short.fuse(cylinder(.29,BOARD-.05-10.08,(x,y,10.08)))
        add('Short'+str(i),'独立短接片 '+str(i+1),short,'Links',STEEL)
    # Physical guide comb; registration uses the two emptied rear-cover screw bores.
    jig=box(39.1,3.2,2.2,0,14,-4.4).fuse(box(14.8,7.6,2.2,8.85,16.9,-4.4))
    for x in [-19,19]:
        jig=jig.fuse(cylinder(.60,3.55,(x,14,-2.2))).fuse(cylinder(1.20,3.1,(x,14,-2.2))).cut(cylinder(.49,7,(x,14,-4.5)))
        bolt=cylinder(1.12,.50,(x,14,-4.9)).fuse(cylinder(.44,6.9,(x,14,-4.4)))
        add('JigBolt'+('L' if x<0 else 'R'),'夹具定位肩与压紧螺钉',bolt,'Jig',STEEL,'同两个后盖螺孔压紧，肩部承担反力；公差、螺纹及扭矩待打样')
    jig=jig.cut(rounded(7.10,7.10,2.70,.55,9,15.45,-4.6))
    nose=rounded(10.3,1.55,11.3,.38,7,20.0,-2.2)
    meter=rounded(3.65,5.45,13.60,.40,15,16.16,-4.50).fuse(rounded(4.25,6.05,.55,.40,15,16.16,-4.95))
    jig=jig.cut(rounded(3.83,5.63,2.7,.43,15,16.16,-4.6))
    for x,y,_ in padpos:
        nose=nose.cut(cylinder(.43,11.5,(x,y,-2.24)))
        meter=meter.cut(cylinder(.43,14.3,(x,y,-5.05)))
        jig=jig.cut(cylinder(.43,2.7,(x,y,-4.58)))
    add('Fixture','定位针架与串口导向鼻 · 开盖后压入',jig.fuse(nose),'Jig',(.32,.55,.70),'两定位脚仅作概念，配合/弹簧工作行程/线径及工装需复核')
    add('Mguide','可卸测流导向块',meter,'Meter',TEAL)
    for x,y,k in padpos:
        add('Probe'+k,'弹簧针 '+k,cylinder(.30,BOARD-.05+5.2,(x,y,-5.2)),
            'Meter' if k.startswith('I') else 'Jig',(.85,.61,.23))
    # Witness solids only. Usable with rear cover and jig both removed.
    tool=rounded(6.4,6.6,BOARD-1.55+4,.45,cx,cy,-4)
    add('PullAccess','绝缘提拔工具 / 整束线 · 6.4 × 6.6 通道',tool,'Tools',(.69,.43,.76)).ViewObject.Transparency=74
    parked=plug.translated(V(0,0,-3.0))
    add('FreePlug','拔出 3.0 mm 的线壳位置',parked,'FreeHarness',(.86,.50,.23))
    def flex_path(points,t):
        return [(x,y,z-t*f) for (x,y,z),f in zip(points,[1,1,.72,.30,0])]
    for i,(points,col,name) in enumerate(wire_data):
        add('FreeLead'+str(i),name+' · 拔出后示意',wire(flex_path(points,3.0),.19),'FreeHarness',col)
    doc.recompute()
    report={'base_sha256':digest,'units':'mm','FreeCAD':App.Version()[:3],'cut_x':CUT_X,
            'board_z':[BOARD,BOARD+.8],'battery_mating':'Molex 781710003 / 781720003 candidate',
            'coordinates_origin':'body centre XY, flat back outer Z=0, rear access -Z','checks':{},'notes':[]}
    def check(key,moving,against):
        hits={}; smallest=None
        for k,shape in against:
            if not moving.BoundBox.intersect(shape.BoundBox):continue
            overlap=moving.common(shape);v=overlap.Volume
            if v>.001:
                b=overlap.BoundBox
                hits[k]=[round(v,5),[round(n,2) for n in (b.XMin,b.XMax,b.YMin,b.YMax,b.ZMin,b.ZMax)]]
        report['checks'][key]=hits
        return hits
    def objects(names):
        return [(k,parts[k].Shape) for k in names]
    def group(*names):
        return [k for k,g in owner.items() if g in names]
    fixed=group('Fixed');board=group('Circuit');battery=group('Cell');leads=group('Mated');lid=group('Lid')
    links=group('Links')
    clamp=group('Fasteners')
    check('battery tool through the installed UART and meter fixture',tool,objects(group('Jig','Meter')+links))
    check('connector axial removal through the installed fixture',down_prisms(plug,3),objects(group('Jig','Meter')+links))
    assert len(parts['Fixture'].Shape.Solids)==1,'fixture must be a continuous bridge around its access window'
    check('three-way cap extraction: unplugged battery / fixture absent',
          Part.makeCompound([down_prisms(parts[k].Shape,15) for k in links]),
          objects(fixed+board+battery+clamp+['BatHeader']))
    check('serial-only fixture with shorting cap retained',
          Part.makeCompound([down_prisms(parts[k].Shape,15) for k in group('Jig')]),objects(links))
    check('screw fixings and board / glass',Part.makeCompound([parts[k].Shape for k in clamp]),objects(fixed+board))
    check('coupon to pogo carrier seated',parts['Mguide'].Shape,objects(group('Jig')))
    check('rear shell and outlet lift',Part.makeCompound([down_prisms(parts[k].Shape,16) for k in ['Back','Outlet']]),objects(fixed+board+battery+clamp+leads+links))
    check('lid parts vs shorting links closed',Part.makeCompound([parts[k].Shape for k in lid]),objects(links))
    for step in range(7):
        changing=Part.makeCompound([wire(flex_path(points,step*.5),.19) for points,_,_ in wire_data])
        check('lead clearance sample '+str(step*.5),changing,objects(fixed+board+clamp+['Tray']+links+group('Jig','Meter')))
    check('tool corridor cover off / jig absent',tool,objects(fixed+battery+[k for k in board if k not in ['BatHeader','Board']]))
    check('connector swept 3 mm',down_prisms(plug,3),objects(fixed+battery+[k for k in board if k not in ['BatHeader','Board']]))
    check('whole jig insert: upper positions swept from rear',
          Part.makeCompound([down_prisms(parts[k].Shape,15) for k in group('Jig','Meter')]),
          objects(fixed+battery+leads+[k for k in board if not (k.startswith('DBG') or k.startswith('I'))]))
    # The back opens straight down: fasteners have first been removed. Deformable side gasket is checked separately.
    check('lid removal: carrier / speaker / contacts / flex / PPG',
          Part.makeCompound([down_prisms(parts[k].Shape,16) for k in lid if k not in ['Back','Outlet'] and not k.startswith('CoverScrew')]),
          objects(fixed+battery+leads+board+group('Fasteners')))
    check('cell removal: connector free / tray screws removed',
          Part.makeCompound([down_prisms(cell.Shape,16),down_prisms(tray,16)]),
          objects(fixed+board+group('Fasteners')))
    for i,(x,y) in enumerate(hole_locs,1):
        check('driver access after battery removal '+str(i),cylinder(1.30,BOARD-.6+4,(x,y,-4)),
              objects(fixed+board))
    for i,(x,y) in enumerate(tray_locs,1):
        check('tray screw access '+str(i),cylinder(.93,bz-.9+4,(x,y,-4)),objects(fixed+['Battery']))
    # All solids fixed to the main board translate together. Frame mounting shoulders remain above it.
    moved=Part.makeCompound([down_prisms(parts[k].Shape,20) for k in board])
    check('PCB / module rear removal: continuous axial sweep',moved,objects(fixed))
    # Static cable and seated components against other assembly groups; designed support contacts allowed.
    check('mounted battery assembly vs board',Part.makeCompound([parts[k].Shape for k in battery]),objects(board))
    check('mounted wire run vs lid structure + PCB + frame',Part.makeCompound([parts['Lead'+str(i)].Shape for i in range(3)]),objects(lid+board+fixed))
    check('speaker vs battery',parts['Speaker'].Shape,objects(battery))
    for k in ['LidTower','LidFlex','Speaker','SpeakerBed','Tray','Battery']:
        check('trace '+k,down_prisms(parts[k].Shape,16),objects(
            (fixed+leads+board+battery) if k not in ['Tray','Battery'] else fixed))
    # Positive controls ensure the tests actually detect a blocked extraction / covered contact.
    assert parts['Speaker'].Shape.common(tool).Volume>1
    assert parts['Screen'].Shape.common(adhesive.translated(V(0,0,.015))).Volume>.1
    report['screen_adhesive_contact_area_mm2']=round(adhesive.Volume/.15,2)
    report['battery_to_module_nominal_mm']=round(cell.Shape.distToShape(parts['ModuleParts'].Shape)[0],3)
    report['screen_to_board_nominal_mm']=round(parts['Screen'].Shape.distToShape(pcb)[0],3)
    report['plug_tool_xy_mm']=[6.4,6.6];report['plug_travel_test_mm']=3
    report['fixture_pads']=padpos;report['board_supports_xy']=hole_locs
    report['tray_supports_xy']=tray_locs
    report['antenna_bound_mm']=[getattr(parts['Antenna'].Shape.BoundBox,'{}{}'.format(ax,end)) for ax in 'XYZ' for end in ['Min','Max']]
    final_board=parts['Board'].Shape
    assert final_board.isValid() and abs(final_board.Volume/.8-max(f.Area for f in final_board.Faces))<1e-5
    report['board_removed_area_vs_B_mm2']=round((src.MainPCB.Shape.Volume-final_board.Volume)/.8,2)
    report['board_old_new_area_mm2']=[round(s.Volume/.8,2) for s in [src.MainPCB.Shape,final_board]]
    save_report()
    bad={k:v for k,v in report['checks'].items() if v}
    if bad:raise ValueError(str(bad))
    App.closeDocument(src.Name)
    # A real thin X-section across both the first PCB fixing and the battery connector.
    thin=box(.50,54,25,CUT_X,0,-5); slice_parts=[]
    for k in fixed+board+battery+leads+lid+group('Fasteners'):
        if k.startswith(('Bar','Lug','Release','BandRoot')):continue
        s=thin.common(parts[k].Shape)
        if s.Volume<.00001:continue
        sc=add('Sec'+k,parts[k].Label+' · 剖',s,'Slice',parts[k].ViewObject.ShapeColor[:3])
        slice_parts.append(sc)
    for k in lid:
        if k.startswith('CoverScrew'):continue
        copy=add('Park'+k,parts[k].Label+' · 移开',parts[k].Shape.translated(V(51,0,-3)),
                 'LidAside',parts[k].ViewObject.ShapeColor[:3])
    def visibility(active):
        for k,g in groups.items():g.ViewObject.Visibility=k in active
        for k,o in parts.items():o.ViewObject.Visibility=owner[k] in active
        for k,o in parts.items():
            if k.startswith(('BandRoot','Release','Bar')):o.ViewObject.Visibility=False
    def render(label,active,angle,tw=1360,th=850):
        visibility(active);doc.recompute();v=Gui.getDocument(doc.Name).activeView()
        v.setCameraType('Orthographic');aim(v,angle);v.fitAll();Gui.updateGui()
        v.saveImage(str(dest/(label+'.png')),tw,th,'White')
    base=['Fixed','Circuit','Cell','Mated','Fasteners']
    render('parked',base+['Links','LidAside'],'inside',1600,850)
    render('probe',base+['Jig','Meter'],'inside')
    render('unplug',['Fixed','Circuit','Cell','Fasteners','Links','FreeHarness','Tools'],'inside')
    render('console',base+['Jig','Links'],'inside')
    render('open',base+['Links'],'plan')
    openfile=dest/'WristFlow-B-Open.FCStd'
    doc.saveAs(str(openfile))
    render('section',['Slice'],'section',1500,700)
    bounds=Part.makeCompound([o.Shape for o in slice_parts]).BoundBox
    report['section_yz_bounds']=[bounds.YMin,bounds.YMax,bounds.ZMin,bounds.ZMax]
    file=dest/'WristFlow-B-Service.FCStd'
    doc.recompute();doc.saveAs(str(file))
    origvol={k:o.Shape.Volume for k,o in parts.items()}
    App.closeDocument(doc.Name);again=App.openDocument(str(file));again.recompute()
    for k,vol in origvol.items():
        s=again.getObject(k).Shape
        assert s.isValid() and abs(s.Volume-vol)<.002,k
    report['reopened_shapes']=len(origvol)
    report['fcstd_sha256']=hashlib.sha256(file.read_bytes()).hexdigest()
    App.closeDocument(again.Name)
    again=App.openDocument(str(openfile));again.recompute()
    for k,vol in origvol.items():
        assert abs(again.getObject(k).Shape.Volume-vol)<.002,k
    assert again.Board.ViewObject.Visibility and again.KeyBody.ViewObject.Visibility and not again.Back.ViewObject.Visibility and not again.SecBoard.ViewObject.Visibility
    report['open_fcstd_sha256']=hashlib.sha256(openfile.read_bytes()).hexdigest()
    report['notes']=['Nominal CAD volumes, contacts and friction not simulated. Coordinate frame and intended usage are specified in the companion handoff.']
    save_report();App.closeDocument(again.Name)
    (dest/'reopened.ok').write_text('ok',encoding='utf8')
