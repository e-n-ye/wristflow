"""Native FreeCAD geometry for the WristFlow wearability review. Dimensions in mm."""
import hashlib
import json
from pathlib import Path

import FreeCAD as App
import FreeCADGui as Gui
import Part
from pivy import coin

V = App.Vector
BASE = Path(__file__).resolve().parent
CASE_W, CASE_L, CASE_T = 41.1, 47.5, 16.0
BOARD_Z = 11.75
GRAPHITE = (.21,.245,.27)
SILVER = (.69,.74,.76)
GREEN = (.12,.49,.36)
GOLD = (.95,.69,.18)
BLUE = (.26,.57,.81)
PURPLE = (.55,.39,.72)
TEAL = (.19,.54,.55)


def box(w, l, h, x, y, z):
    return Part.makeBox(w, l, h, V(x-w/2, y-l/2, z))


def rounded(w, l, h, radius, x=0, y=0, z=0):
    s = box(w,l,h,x,y,z)
    return s.makeFillet(radius, [edge for edge in s.Edges if edge.BoundBox.ZLength > h-.001])


def cylinder(rad, length, start, direction=(0,0,1)):
    return Part.makeCylinder(rad,length,V(*start),V(*direction))


def strap_face(y, width, z, height):
    points = [V(-width/2,y,z),V(width/2,y,z),V(width/2,y,z+height),V(-width/2,y,z+height)]
    return Part.makePolygon(points+[points[0]])


def make_model(variant, dest):
    thin = variant == 'B'
    name = 'WristFlow_' + variant
    doc = App.newDocument(name)
    doc.Label = 'WristFlow · ' + ('薄电池局部重叠' if thin else '现电池与模组并排')
    categories = {}
    for key, label in [('Exterior','中框与侧键'),('Display','屏幕与前侧接口'),('Electronics','主板与可换电池'),('Sound','声音与振动'),('Rear','后盖与贴肤光学'),('Band','通用表带与装配'),('Reference','可选 · 红米带根外形对照')]:
        group = doc.addObject('App::DocumentObjectGroup',key)
        group.Label = label
        categories[key] = group
    parts = {}
    def add(key, label, shape, group, color, evidence='结构假设，单位 mm', source='', hidden=False):
        obj=doc.addObject('Part::Feature',key)
        obj.Label=label
        obj.Shape=shape
        obj.addProperty('App::PropertyString','Basis','Review','尺寸证据').Basis=evidence
        if source:
            obj.addProperty('App::PropertyString','Source','Review','来源或检索入口').Source=source
        for prop,val in [('SizeX',shape.BoundBox.XLength),('SizeY',shape.BoundBox.YLength),('SizeZ',shape.BoundBox.ZLength)]:
            obj.addProperty('App::PropertyLength',prop,'Envelope','该零件当前全局包络').__setattr__(prop,val)
            obj.setEditorMode(prop,1)
        categories[group].addObject(obj)
        obj.ViewObject.ShapeColor=color
        obj.ViewObject.LineColor=(.12,.16,.17)
        obj.ViewObject.LineWidth=1.0
        obj.ViewObject.Visibility=not hidden
        parts[key]=obj
        return obj

    # Common screen seat; Z=0 denotes the outside of the flat back.
    outer=rounded(CASE_W,CASE_L,15.1,6.8,z=.9)
    cavity=rounded(37.5,43.9,16,5,z=.8)
    frame=outer.cut(cavity)
    ledge=rounded(37.6,44, .45,5,z=12.58).cut(rounded(32.7,39,.65,3.7,z=12.5))
    bezel=rounded(41.1,47.5,2.97,6.8,z=13.03).cut(rounded(35.35,41.71,3.2,5.25,z=13))
    frame=frame.fuse(ledge).fuse(bezel).removeSplitter()
    sound_y,sound_z=(15.8,6.0) if thin else (13,5.1)
    sound_slit=box(5,4.8,1,20,sound_y,sound_z)
    crown_hole=cylinder(1.1,5,(17.5,0,8),(1,0,0))
    frame=frame.cut(sound_slit).cut(crown_hole)
    cover_fasteners=[(x,y) for x in [-19,19] for y in [-14,14]]
    for x,y in cover_fasteners:
        boss=cylinder(1.37,3.75,(x,y,.9))
        frame=frame.fuse(boss).cut(cylinder(.64,3.2,(x,y,.8)))
    frame=frame.removeSplitter()
    add('MiddleFrame','中框 · 屏台阶 / 侧出声口',frame,'Exterior',GRAPHITE)
    add('Crown','表冠 · 外形与按压位置',cylinder(3.3,2.8,(20.6,0,8),(1,0,0)),'Exterior',SILVER)
    add('CrownShaft','表冠轴 · 示意',cylinder(.8,3.0,(17.65,0,8),(1,0,0)),'Exterior',SILVER)
    add('Screen','AMOLED 屏组件 · W-010 同类包络',rounded(35.11,41.47,2.955,5.05,z=13.03),'Display',(.075,.095,.11),
        '同类 HARESAN 屏组件 35.11×41.47×2.97；W-010 机械版待核。表面示意层含在 2.97 内。',
        'https://wiki.lckfb.com/zh-hans/hspi-sf32lb52/hardware/board.html')
    add('ActiveArea','显示有效区 · 同类屏正面参照',rounded(30.75,35.48,.015,3.75,z=15.985),'Display',(.12,.16,.18),'HARESAN 同类面板的有效区；开口和圆角待核')
    add('DisplayBTB','屏端 24 针座 · 示意包络',box(6.85,1.94,.45,7,-13.4,12.55),'Display',(.69,.52,.22),
        'OK-118RF024/2-35 系列；6.85 mm 长有图，实际扣合高度和位置待核；模型高度仅示意')
    add('FpcRoute','屏 FPC · 走向待核',box(5.4,6.4,.07,7,-10.7,12.98),'Display',(.76,.48,.19),'路径示意，不据此定 FPC 最小弯曲半径')

    mod_x,mod_y=(-7.0,6.7) if thin else (-2.95,11)
    mw,ml=(18,27.9) if thin else (27.9,18)
    ant_x,ant_y=(mod_x,mod_y+ml/2-3.05) if thin else (mod_x-mw/2+3.05,mod_y)
    rf_cut=box(18.3,12,2,mod_x,mod_y+ml/2-.1,11.3) if thin else box(14,18.3,2,-17.65,mod_y,11.3)
    pcb=rounded(36.8,43.2,.8,4.6,z=BOARD_Z).cut(rf_cut)
    add('MainPCB','首板板框 · 0.8 / 天线缺口',pcb,'Electronics',GREEN,'设计轮廓；屏座、射频及锁件实配后再定')
    module_z=BOARD_Z-3.1
    add('ModuleSubstrate','SF32LB52-MOD-1 · 基板',box(mw,ml,.8,mod_x,mod_y,BOARD_Z-.8),'Electronics',(.16,.35,.31),'原厂 18×27.9×3.1，含 0.8 基板；电子区用保守包络',
        'https://wiki.sifli.com/silicon/模组型号指南.html')
    sw,sl=(mw,ml-6.1) if thin else (mw-6.1,ml)
    sx,sy=(mod_x,mod_y-3.05) if thin else (mod_x+3.05,mod_y)
    add('ModuleParts','模组电子区 · 高度包络',box(sw,sl,2.3,sx,sy,module_z),'Electronics',SILVER)
    aw,al=(18,6.1) if thin else (6.1,18)
    add('Antenna','板载天线区 · 勿覆金属',box(aw,al,.02,ant_x,ant_y,BOARD_Z-.82),'Electronics',(.82,.58,.20),'DS5203 V0.2 图 8-2，18×6.1 mm；整机射频净空待验证')

    bw,bl,bh,by,bz=(35.,30.,5.,-4.9,2.85) if thin else (34.,21.,8.92,-9.45,2.55)
    cell_name='中顺芯 503035T · 型号码占位' if thin else '在手 LP902030 · 厚度实测'
    cell_src='https://m.tb.cn/h.jM5igkD' if thin else 'https://item.taobao.com/item.htm?id=572591491841'
    cell_basis=('500 mAh / 3.7 V / 4.2 V。35×30×5 为型号初估，带 PCM、封边、出线的成品上限待量。'
                if thin else 'XY 取商品本体上限 34×21；Z 为在手单体实测 8.92。商品厚度上限 10 mm，未含完整安装余量。')
    add('Battery',cell_name,rounded(bw,bl,bh,.65,0,by,bz),'Electronics',(.95,.65,.20),cell_basis,cell_src)
    tray=rounded(bw+.65,bl+.65,.4,.9,0,by,bz-.45)
    tab_y=by+bl/2+1.65
    for sign,side in [(-1,'L'),(1,'R')]:
        x=sign*14.4
        shaft=cylinder(.61,2.2,(x,tab_y,bz-.95))
        tab=rounded(4.2,4.2,.4,.65,x,tab_y-.3,bz-.45)
        tray=tray.fuse(tab).cut(shaft)
        anchor=box(6.3,2.5,1.05,sign*16.35,tab_y,bz-.05).cut(shaft)
        add('Seat'+side,'托板支座 · '+side,anchor,'Electronics',GRAPHITE,'连接中框的可拆托板支点；螺纹、扭矩及工具扫掠待核')
        screw=cylinder(1.12,.45,(x,tab_y,bz-.9)).fuse(cylinder(.52,1.4,(x,tab_y,bz-.45)))
        add('Screw'+side,'托板螺钉 · '+side,screw,'Electronics',SILVER)
    add('BatteryTray','可拆电芯托板 · 两处支点',tray.removeSplitter(),'Electronics',(.85,.72,.48),'几何支点接中框；电芯固定、膨胀与可维护净空待核')
    add('BatPlug','可拔电池接头 · 布置候选',box(4.4,2.25,1.75,-10,-19.6 if thin else 2.9,7.95),'Electronics',(.86,.86,.77),'选件、实际线序、可达方向与线束待定')
    for i,signal in enumerate(['GND','UART RX','UART TX']):
        xx,yy=(-7.2+i*1.9,-20.65) if thin else (11.9+i*1.9,17.6)
        add('Test'+str(i),signal+' · 救援探针',cylinder(.48,.04,(xx,yy,BOARD_Z-.04)),'Electronics',GOLD,'位置候选；下载模式、复位和治具方向待板级验证')
    ports=[(-.4,-20.65),(4,-20.65),(8.4,-20.65)] if thin else [(13.1,13.2),(16.5,13.2),(13.1,15.3)]
    for i,(xx,yy) in enumerate(ports):
        lands=Part.makeCompound([box(.74,.80,.05,xx+offset,yy,BOARD_Z-.05) for offset in [-.56,.56]])
        add('Shunt'+str(i),['BAT 整机','OLED 供电','屏侧 3.3 V'][i]+' · 双焊盘测流桥',lands,'Electronics',GOLD,'拆桥后串表；PCB 走线及防反供待核。这两个焊盘合为一个元件占位。')

    spx,spy,spz=(8.4,15.8,4.2) if thin else (.1,13,3.35)
    speaker=rounded(15,11,4.3,.82,spx,spy,spz)
    add('Speaker','海晨 1511 · 带腔喇叭',speaker,'Sound',BLUE,'淘宝店标题标称 15×11×4.3、8 Ω / 2 W。按本体包络画；出口位置与公差待样件核对',
        'https://tao.hooos.com/goods_KOVYjOGc3toVMxnzeJcmz2IJtW-XNReeAC05yvgRVDTp.html')
    start=spx+7.5
    add('AcousticPath','侧向声道 · 空气占位',box(20.55-start,4.35,.80,(start+20.55)/2,spy,sound_z+.10),'Sound',(.54,.76,.91),'透明实体表示通路；须核喇叭实际出声面与密封').ViewObject.Transparency=68
    mx,my,mz=(12,-5,8.85) if thin else (13.25,7.0,1.72)
    add('Motor','在手圆饼电机 · Ø10 × 2.7',cylinder(5,2.7,(mx,my,mz)),'Sound',PURPLE,'在手电机与商品图；焊点、胶及导线另计')
    ledge_start=mx-5.15
    add('MotorSeat','马达承力面 · 连接中框',box(19.15-ledge_start,10.4,.55,(19.15+ledge_start)/2,my,mz-.55),'Sound',(.55,.49,.62),'壳体支承并传振，胶层与紧固方式待验证')

    lid=rounded(41.1,47.5,.9,6.8).cut(cylinder(7.05,1.2,(0,0,-.1)))
    for i,(x,y) in enumerate(cover_fasteners):
        lid=lid.cut(cylinder(.68,1.2,(x,y,-.1))).cut(cylinder(1.18,.48,(x,y,-.05)))
        fastener=cylinder(1.13,.40,(x,y,.03)).fuse(cylinder(.54,2.9,(x,y,.43)))
        add('CoverScrew'+str(i),'后盖沉孔螺钉 '+str(i+1),fastener,'Rear',SILVER,'四点拆盖示意；头型、螺柱强度、攻丝或嵌件待选')
    contact_x=(-3.2,3.2)
    for x in contact_x:
        lid=lid.cut(cylinder(1.13,1.2,(x,-15,-.1)))
    add('Back','可拆后盖',lid,'Rear',GRAPHITE)
    optic=cylinder(8,1.5,(0,0,-1.5)).fuse(cylinder(7.0,.9,(0,0,0)))
    for x,y in [(-2.7,0),(2.7,0),(0,2.4)]:
        optic=optic.cut(cylinder(1.12,3,(x,y,-1.6)))
    add('Baffle','PPG 凸台 · 三窗间独立遮光',optic,'Rear',(.17,.20,.20),'外凸 1.5 为佩戴模型设定；光窗、面积和遮光隔墙按未选传感器作示意')
    for i,(x,y) in enumerate([(-2.7,0),(2.7,0),(0,2.4)]):
        add('Optic'+str(i),'光窗 '+str(i+1),cylinder(1.08,.65,(x,y,-1.48)),'Rear',(.25,.60,.48))
    add('PpgBoard','可换 PPG 小板 · 候选位置',rounded(12,10,.55,1.0,z=1.0),'Rear',TEAL,'12×10×0.55 为预留包络，传感器、光路和接口待比选')
    for i,x in enumerate(contact_x):
        add('Contact'+str(i),'充电触点 '+str(i+1),cylinder(1.05,.89,(x,-15,0)),'Rear',GOLD)

    # Standard straight-end strap. Source width is known; lengths, pin and hole geometry are concept values.
    lug_z=4.6
    for sign,side in [(1,'Short'),(-1,'Long')]:
        axis_y=sign*26.15
        end_y=sign*26.2
        for dx,tag in [(-1,'L'),(1,'R')]:
            lug=rounded(3.3,6.45,4.8,.85,dx*12.85,sign*25.45,lug_z-2.4)
            hole=cylinder(.64,4,(dx*12.85-2,axis_y,lug_z),(1,0,0))
            add('Lug'+side+tag,'表耳 · '+side+' '+tag,lug.cut(hole),'Band',GRAPHITE,'耳间 22.4；孔径、孔深、到壳边距离为试装预留')
        pin=cylinder(.85,21.96,(-10.98,axis_y,lug_z),(1,0,0))
        pin=pin.fuse(cylinder(.48,24,(-12,axis_y,lug_z),(1,0,0)))
        add('Bar'+side,'成品金属生耳 · '+side,pin,'Band',SILVER,'对应 22 mm 平口；杆、针尖与压缩行程是待核的名义包络')
        length=108 if sign<0 else 75
        near=[strap_face(end_y-sign*1.35,22,lug_z-1.5,3.0),strap_face(end_y+sign*4,22,3.10,3.0),strap_face(end_y+sign*15,21.5,1.5,3.0)]
        far=[near[-1],strap_face(end_y+sign*(length-4),20.0,-.5,3.0),strap_face(end_y+sign*length,18.1,-.75,3.0)]
        near_s=Part.makeLoft(near,True,True).cut(cylinder(.95,24,(-12,axis_y,lug_z),(1,0,0)))
        near_s=near_s.cut(rounded(5.5,1.55,2,.5,-5.3,axis_y,2.75))
        add('BandRoot'+side,'沐时 22 mm 平口带根 · '+side,near_s,'Band',(.18,.30,.32),'商品图带厚 3 mm，有拨杆。模型标称宽 22；配合、带根包轴和拨杆位置待实配',
            'https://www.taouq.com/id-Vj4Y4xwC6tw5dVaXNNc27qTvtV-KaykkBHwaJW57MdIP/')
        lever=cylinder(.42,1.4,(-6.5,axis_y,3.0)).fuse(rounded(1.5,1.15,.36,.3,-6.5,axis_y,2.80))
        add('Release'+side,'金属生耳拨杆 · '+side,lever,'Band',SILVER,'拨杆在带背一侧；滑动方向沿生耳轴，行程待实配')
        fs=Part.makeLoft(far,True,True)
        if sign<0:
            for i in range(9):
                fs=fs.cut(cylinder(1.35,8,(0,end_y+sign*(42+i*6.5),-3)))
        add('BandTail'+side,'通用硅胶带身 · '+side,fs,'Band',(.18,.30,.32),'带厚按商品图 3 mm；有效带长与孔距暂设，待确切 SKU 核对')
        ref=Part.makeLoft([strap_face(sign*23.8,28,3.0,3.0),strap_face(sign*37,22.6,1.0,2.6)],True,True)
        add('Redmi'+side,'红米原带轮廓 · 约 28 → 22.6',ref,'Reference',(.86,.42,.25),'带根由用户粗量约 28 mm；小米 FAQ 带宽 22.6 mm。单独对照，未建专用锁件',
            'https://www.mi.com/global/support/faq/details/KA-228656/',True)
    byoke=rounded(23.5,11,2.35,2,0,101.5,-.75).cut(rounded(19.8,7.7,3,1.1,0,101.5,-1))
    add('Buckle','扣框 · 外形示意',byoke,'Band',SILVER)
    add('BuckleTongue','扣舌 · 外形示意',box(1.35,8.4,1.1,0,101.45,1.30),'Band',SILVER)
    add('Keeper','带圈 · 外形示意',rounded(23.5,6,5.3,.9,0,78,-.9).cut(box(21.1,8,3.9,0,78,-.3)),'Band',(.16,.25,.26))

    doc.recompute()
    assert all(o.Shape.isValid() and o.Shape.Volume>0 for o in parts.values())
    report={'variant':variant,'native':App.Version()[:3],'base_mm':[CASE_W,CASE_L,CASE_T],'crown_included':False,'total_with_optical_mm':17.5,'parts':len(parts),'intersections_mm3':{},'source':'812d6c0'}
    checks=[('Battery',k) for k in ['MiddleFrame','MainPCB','ModuleParts','ModuleSubstrate','Antenna','Motor','MotorSeat','Speaker','PpgBoard','Back','BatPlug','BatteryTray']]
    checks += [('Motor',k) for k in ['Speaker','MainPCB','ModuleParts','Back','MiddleFrame']]
    checks += [('Speaker',k) for k in ['MiddleFrame','MainPCB','ModuleParts','MotorSeat','Back']]
    checks += [('MainPCB','Screen'),('MainPCB','MiddleFrame'),('Back','PpgBoard'),('CrownShaft','MiddleFrame')]
    for side in ['Long','Short']:
        checks += [('Bar'+side,k) for k in ['BandRoot'+side,'Lug'+side+'L','Lug'+side+'R']]
    checks += [('Battery',k) for k in ['SeatL','SeatR','ScrewL','ScrewR']]
    for a,b in checks:
        volume=parts[a].Shape.common(parts[b].Shape).Volume
        report['intersections_mm3'][a+' / '+b]=round(volume,5)
    report['overlaps']={k:v for k,v in report['intersections_mm3'].items() if v>.001}
    report['strap_rotation_intersection_mm3']={}
    for sign,side in [(1,'Short'),(-1,'Long')]:
        pieces=Part.makeCompound([parts['BandRoot'+side].Shape,parts['BandTail'+side].Shape])
        for angle in [0,15,30,45,60]:
            tilted=pieces.copy()
            tilted.rotate(V(0,26.15*sign,lug_z),V(1,0,0),-sign*angle)
            report['strap_rotation_intersection_mm3'][side+str(angle)]=round(tilted.common(frame).Volume,5)
    (dest/(name+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    assert not report['overlaps'],report['overlaps']
    return doc,parts,categories


def show(doc,parts,mode):
    for key,o in parts.items():
        group=o.InList[0].Name
        visible=group!='Reference'
        if mode!='full' and (key.startswith('BandTail') or key in ['Buckle','BuckleTongue','Keeper']):
            visible=False
        if mode in ['inside','plan'] and (group=='Rear' or key in ['BatteryTray','MotorSeat']):
            visible=False
        if mode=='full' and group not in ['Exterior','Display','Band','Rear']:
            visible=False
        o.ViewObject.Visibility=visible
    view=Gui.getDocument(doc.Name).activeView()
    view.setCameraType('Orthographic')
    aim(view,mode)
    view.fitAll()
    Gui.updateGui()


def aim(view,mode):
    if hasattr(view,'setAnimationEnabled'):
        view.setAnimationEnabled(False)
    normal={'front':(0,0,1),'full':(.40,-.42,1),'plan':(0,0,-1),'inside':(.42,-.52,-1),'back':(.42,-.52,-1),'section':(1,0,0),'exploded':(.95,-.55,.48)}[mode]
    up=(0,0,1) if mode in ('section','exploded') else (0,1,0)
    rot=App.Rotation(V(1,0,0),V(*up),V(*normal),'ZYX')
    view.setCameraOrientation(rot.Q)
    # Apply directly because hidden GUI windows can defer animated camera changes.
    view.getCameraNode().orientation.setValue(*rot.Q)


def run(dest):
    dest=Path(dest)
    Gui.activateWorkbench('PartWorkbench')
    Gui.updateGui()
    for variant in ['A','B']:
        doc,parts,groups=make_model(variant,dest)
        Gui.updateGui()
        view=Gui.getDocument(doc.Name).activeView()
        for mode in ['inside','plan','front','back','full']:
            show(doc,parts,mode)
            view.saveImage(str(dest/(variant+'-'+mode+'.png')),1200,950,'White')
        section=doc.addObject('App::DocumentObjectGroup','Section')
        section.Label='关键剖面 · X = −4 mm'
        clip=box(60,60,24,-34,0,-3)
        for o in parts.values():
            if o.InList[0].Name in ['Band','Reference']:
                continue
            shape=o.Shape.common(clip)
            if shape.Volume>.001:
                cut=doc.addObject('Part::Feature','Cut'+o.Name)
                cut.Shape=shape
                cut.Label=o.Label+' · 剖'
                cut.ViewObject.ShapeColor=o.ViewObject.ShapeColor
                section.addObject(cut)
        for o in parts.values():
            o.ViewObject.Visibility=False
        doc.recompute()
        aim(view,'section')
        view.fitAll()
        Gui.updateGui()
        view.saveImage(str(dest/(variant+'-section.png')),1200,700,'White')
        section.ViewObject.Visibility=False
        exploded=doc.addObject('App::DocumentObjectGroup','Exploded')
        exploded.Label='分解视图 · 分层移开'
        offsets={'Screen':26,'ActiveArea':26,'MainPCB':14,'DisplayBTB':14,'FpcRoute':17,
                 'ModuleSubstrate':7,'ModuleParts':7,'Antenna':7,'MiddleFrame':0,'Crown':0,
                 'Speaker':-10,'Motor':-10,'Battery':-16,'BatteryTray':-22,
                 'PpgBoard':-27,'Back':-34,'Baffle':-38,'Contact0':-34,'Contact1':-34}
        for key,dz in offsets.items():
            source=parts[key]
            copy=doc.addObject('Part::Feature','View'+key)
            copy.Label=source.Label+' · 分解'
            copy.Shape=source.Shape.translated(V(0,0,dz))
            copy.ViewObject.ShapeColor=source.ViewObject.ShapeColor
            exploded.addObject(copy)
        doc.recompute()
        aim(view,'exploded')
        view.fitAll()
        Gui.updateGui()
        view.saveImage(str(dest/(variant+'-exploded.png')),1000,1300,'White')
        exploded.ViewObject.Visibility=False
        show(doc,parts,'inside')
        fc=dest/('WristFlow-'+variant+'.FCStd')
        doc.recompute()
        doc.saveAs(str(fc))
        original={k:round(v.Shape.Volume,4) for k,v in parts.items()}
        App.closeDocument(doc.Name)
        again=App.openDocument(str(fc))
        again.recompute()
        for k,vol in original.items():
            assert again.getObject(k).Shape.isValid() and abs(again.getObject(k).Shape.Volume-vol)<.002
        App.closeDocument(again.Name)
        (dest/f'validation-{variant}.json').write_text(
            json.dumps({'sha256': hashlib.sha256(fc.read_bytes()).hexdigest()})+'\n',
            encoding='utf-8')
    (dest/'reopened.ok').write_text('Both FCStd files reopened and recalculated; all model solids verified.',encoding='utf-8')
