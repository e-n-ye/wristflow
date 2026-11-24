"""B variant, local bearing and removable daughterboard for the detented crown."""
from pathlib import Path
import json
import sys
import FreeCAD as App
import Part

V = App.Vector
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from workspace import study_dir
HERE = study_dir(__file__)
SOURCE = HERE.parent / 'B-bridge/mirror-port-3/WristFlow-B-Coordinated.FCStd'
Y, Z = 15.20, 5.35

def block(x, y, z, dx, dy, dz):
    return Part.makeBox(dx, dy, dz, V(x,y,z))

def axial(x, length, radius, y=Y, z=Z):
    return Part.makeCylinder(radius, length, V(x,y,z), V(1,0,0))

def prism(points, direction):
    vertices = [V(*p) for p in points]
    return Part.Face(Part.makePolygon(vertices + [vertices[0]])).extrude(V(*direction))

def union(parts):
    return parts[0].multiFuse(parts[1:]).removeSplitter() if len(parts)>1 else parts[0]

def bend(cx, cz, y0, r, signs):
    # Constant-thickness quarter cylinder, extruded across the width of the ribbon.
    w, t = 3.5, .10
    def roll(rad):
        return Part.makeCylinder(rad,w,V(cx,y0,cz),V(0,1,0))
    h = r+t
    return roll(r+t/2).cut(roll(r-t/2)).common(
        block(cx if signs[0]>0 else cx-h, y0, cz if signs[1]>0 else cz-h, h,w,h))

def shapes(doc):
    item={}
    def new(name,label,solid,color):
        item[name]=(label,solid,color)
        return solid
    frame0=doc.Frame.Shape
    # Replace the previous central shaft bore and cut the side-wall counterbore on the new axis.
    frame=frame0.fuse(axial(18.75,1.80,1.101,0,8))
    frame=frame.cut(axial(17.45,4,1.29)).cut(axial(20.22,1.,3.43)).cut(axial(20.08,.14,1.57))
    rails=[]
    for side in (-1,1):
        inner=Y+side*3.52
        outer=Y+side*4.47
        yl=min(inner,outer)
        rail=block(13.21,yl,3.52,5.86,.95,3.93)
        # A slot on each side captures the edges of the stiffener against axial load.
        edge=min(Y+side*3.24,inner)
        rail=rail.fuse(block(13.21,edge,2.32,.40,.28,5.13))
        rail=rail.fuse(block(14.17,edge,2.32,.32,.28,5.13))
        rail=rail.fuse(block(13.21,edge,7.45,1.28,.28,.17))
        rail=rail.fuse(block(13.21,yl,2.32,3.69,.95,1.21))
        rail=rail.cut(Part.makeCylinder(.32,1.55,V(15.15,(outer+inner)/2,2.32)))
        rails.append(rail)
    frame=union([frame]+rails)
    original=doc.Crown.Shape
    crown=original.translated(V(0,Y,Z-8)).cut(axial(20.60,1.65,.60))
    # The .74 square and insertion depth are trial values pending physical shaft fit.
    shaft=union([block(15.05,Y-.37,Z-.37,3.0,.74,.74),axial(18.05,2.55,.80),
                 axial(18.49,.20,1.07),axial(20.60,1.50,.60)])
    bush=axial(18.72,1.50,1.26).fuse(axial(20.08,.14,1.53)).cut(axial(18.70,1.54,.86))
    new('GuideSleeve','表冠导套 · Ø1.72 内孔，台肩定位，外圆粘接试装',bush,'#55a6b2')
    # Mounting face at 14.15, nose at 17.65. Front-to-back and width per approval drawing.
    face=14.15
    profile=[(-2.42,-2.15),(2.42,-2.15),(2.42,1.05),(1.0,1.75),(-1.0,1.75),(-2.42,1.05)]
    housing=prism([(face,Y+a,Z+b) for a,b in profile],(3.2,0,0))
    housing=housing.cut(axial(14.33,3.2,.99))
    rotor=axial(14.35,3.30,.96).cut(block(14.34,Y-.40,Z-.40,3.33,.8,.8))
    new('EncoderRotor','转子 · 0.80 mm 方孔，孔深示意',rotor,'#dcabc7')
    new('SwitchFollower','轴端与按键推件接触 · 位置示意，待测预压',
        block(14.80,Y-.36,Z-.36,.25,.72,.72),'#b96348')
    board=block(13.61,Y-3.50,1.98,.54,7.,5.47)
    pegz=Z-1.37
    for yy in (Y-1.50,Y+1.50):
        board=board.cut(axial(13.55,.7,.30,yy,pegz))
    new('Daughter','表冠小板 · 竖装刚挠板补强区',board,'#42846e')
    pins=[axial(13.70,.45,.245,yy,pegz) for yy in (Y-1.50,Y+1.50)]
    pads=[]
    for dy,dz,w,h in [(-1.5,-1.6,.80,.60),(0,-1.6,.80,.60),(1.5,-1.6,.80,.60),
                        (-.75,-.60,.50,.80),(.75,-.60,.50,.80),(-2.55,2.08,1.30,.95),(2.55,2.08,1.30,.95)]:
        pad=block(face,Y+dy-w/2,pegz+dz-h/2,.045,w,h)
        pads.append(pad)
        # Lower external leads reach the catalogue seating plane. Metal ears lie outside the case.
        if dz < -1:
            pins.append(block(face+.045,Y+dy-.2,pegz+dz-.11,.20,.4,1.08))
        if dz>1:
            pins.append(block(face+.045,Y+dy-.45,pegz+dz-.30,.25,.90,.60))
    # Expose the pad and terminal recesses on the seating face.
    new('EncoderBody','BMQ22-N1A1-2012-A · 额定外形，内部简化',
        housing.cut(Part.makeCompound(pins+pads)),'#a86a9b')
    new('EncoderPins','定位柱、焊脚及两侧金属固定耳 · 简化',Part.makeCompound(pins),'#9ca5ad')
    new('EncoderPads','小板焊盘 · A、GND、B、按键两端及外壳焊盘',Part.makeCompound(pads),'#d9af5a')
    new('Retainer','小板下端可拆压条 · 两端托住板边',
        union([block(14.49,Y-4.45,1.83,1.32,8.90,.49)] +
              [block(13.46,yy,1.83,1.03,.73,.15) for yy in (Y-3.50,Y+2.77)]).cut(
            Part.makeCompound([Part.makeCylinder(.42,.7,V(15.15,Y+i*3.995,1.75)) for i in (-1,1)])),'#82a2a7')
    screws=[]
    for sign in (-1,1):
        yy=Y+sign*3.995
        screws += [Part.makeCylinder(.32,1.68,V(15.15,yy,1.83)),
                   Part.makeCylinder(.58,.28,V(15.15,yy,1.55))]
    new('RetainerScrews','压条螺钉 · 名义包络，螺纹待选',Part.makeCompound(screws),'#758b97')
    lower=[(12.91,Y),(11.05,15.7),(9.15,18.5),(8.10,18.5)]
    edge=[(x,y-1.75,1.18) for x,y in lower]+[(x,y+1.75,1.18) for x,y in reversed(lower)]
    ribbon=union([bend(12.91,1.98,Y-1.75,.75,(1,-1)),prism(edge,(0,0,.10)),
                  bend(8.10,1.98,16.75,.75,(-1,-1)),block(7.30,16.75,1.98,.10,3.5,8.05),
                  bend(6.60,10.03,16.75,.75,(1,1)),block(3.45,16.75,10.73,3.15,3.50,.1)])
    new('CrownFlex','表冠专用 FPC · 0.10 mm，三处 R0.75 示意弯',ribbon,'#e19a38')
    new('FlexStiffener','FPC 插接端补强 · 合计 0.30 mm',block(3.45,16.75,10.53,2.55,3.5,.20),'#8e6caa')
    # FH34 catalogue envelope. Tongue and entry kept as distinct solids.
    socket=block(1.6,16.,10.3,3.8,5,1).cut(block(3.42,16.69,10.50,2.05,3.62,.36))
    contacts=[];fingers=[]
    for i in range(6):
        yy=18.5+(i-2.5)*.50
        fingers.append(block(3.45,yy-.14,10.83,1.42,.28,.012))
        contacts.append(union([block(3.20,yy-.08,10.842,1.04,.16,.06),
                               block(3.04,yy-.08,10.842,.16,.16,.458)]))
    metal=Part.makeCompound(contacts)
    new('CrownZIF','主板 6P ZIF · FH34SRJ-6S-0.5SH(50) 外形包络',socket.cut(metal),'#39708f')
    new('ZIFContacts','座内六个接点至主板焊接面 · 导通路径示意',metal,'#e3b75b')
    new('GoldFingers','排线插接端 · 六个金手指，五路使用',Part.makeCompound(fingers),'#e3b75b')
    return item,frame,crown,shaft

def run(dest):
    dest=Path(dest)
    doc=App.openDocument(str(SOURCE))
    groups='Fixed Circuit Fasteners Cell Mated Lid Links Display Band'.split()
    base={o.Name:o.Shape.copy() for g in groups for o in doc.getObject(g).Group}
    added,frame,crown,shaft=shapes(doc)
    frame0,original_crown=base['Frame'],base['Crown']
    base.update(Frame=frame,Crown=crown,CrownShaft=shaft)
    for name,sh in [('Frame',frame),('Crown',crown),('CrownShaft',shaft)]:
        doc.getObject(name).Shape=sh
    doc.Crown.Label='冠帽 · M1.2 可拆连接试案，简化螺纹包络'
    doc.CrownShaft.Label='表冠轴 · Ø1.60 转 0.74 方榫，冠帽端 M1.2 啮合 1.50'
    doc.Frame.Label='B 中框 · 表冠换位，双导轨、导套孔和按压沉台'
    grp=doc.addObject('App::DocumentObjectGroup','CrownAssembly');grp.Label='表冠 · 编码器、小板和主板连接'
    for key,(label,shape,col) in added.items():
        o=doc.addObject('Part::Feature',key);o.Label=label;o.Shape=shape;grp.addObject(o)
        o.ViewObject.ShapeColor=tuple(int(col[i:i+2],16)/255 for i in (1,3,5))
    oldEnc=block(14.15,-2.42,5.85,3.50,4.84,3.90)
    data={'base':str(SOURCE),'axis_mm':{'before':[0,8],'after':[Y,Z]},'old_battery_overlap_mm3':oldEnc.common(base['Battery']).Volume,'sources':{
        'encoder':'https://atta.szlcsc.com/upload/public/pdf/source/20251110/C68E3E78D266BC0D180E9022C28D3E55.pdf#page=2',
        'socket':'https://www.hirose.com/en/product/p/CL0580-1236-1-50'},
        'model_limits':['503035T pack nominal 35 x 30 x 5 mm with unchanged placement; finished pack/PCM limits not measured.','Simplified encoder and ZIF; shaft insertion, button preload, mounting hole centre datum and seal need prototype.','Motion endpoints at 0 and 0.25 mm; axial stop and panel fit are nominal. No fatigue or RF test.']}
    allsh={**base,**{k:v[1] for k,v in added.items()}}
    for k,s in allsh.items():
        assert s.isValid() and not s.isNull(),k
    hits={}
    review=set(added)|{'Crown','CrownShaft','Frame'}
    for k in sorted(review):
        for j in allsh:
            if j==k or (j in review and j<k):continue
            if k=='Frame' and j not in review:
                old=frame0.common(allsh[j]).Volume if frame0.BoundBox.intersect(allsh[j].BoundBox) else 0
            else:old=0
            if not allsh[k].BoundBox.intersect(allsh[j].BoundBox):continue
            v=allsh[k].common(allsh[j]).Volume-old
            if v>.00005:hits[k+' / '+j]=round(v,6)
    # New motion tested against every adjacent installed solid.
    for mode,delta in [('pressed',-.25)]:
        for k in ('Crown','CrownShaft','SwitchFollower'):
            sh=allsh[k].translated(V(delta,0,0))
            for j,s in allsh.items():
                if j in ['Crown','CrownShaft','SwitchFollower'] or not sh.BoundBox.intersect(s.BoundBox):continue
                v=sh.common(s).Volume
                if v>.00005:hits[mode+' '+k+' / '+j]=round(v,6)
    data['clashes_mm3']=hits
    pairs=[('Crown','Frame'),('EncoderBody','Battery'),('Daughter','Battery'),('Frame','Battery'),
           ('Daughter','MotorSeat'),('CrownFlex','MotorSeat'),('CrownFlex','Back'),('CrownFlex','Battery'),
           ('CrownFlex','LidTower'),('CrownFlex','Lead2'),('CrownZIF','LidTower'),
           ('CrownZIF','Board'),('CrownFlex','Daughter'),('CrownShaft','EncoderRotor')]
    data['clearances_mm']={a+' / '+b:round(allsh[a].distToShape(allsh[b])[0],4) for a,b in pairs}
    data['button_pressed_to_frame_mm']=round(crown.translated(V(-.25,0,0)).distToShape(frame)[0],4)
    data['socket_land_mm2']=round(block(1.6,16,11.30,3.8,5,.1).common(base['Board']).Volume/.1,4)
    data['shaft_radial_play_mm']=.06
    data['outer_crown_x_mm']={'original':round(original_crown.BoundBox.XMax,3),'trial':round(crown.BoundBox.XMax,3)}
    data['retail_screening_only']={'503030_450_30x30x5':{'volume_cm3':4.5,'old_axis_body_to_pack_mm':round(oldEnc.distToShape(block(-17.5,-19.9,2.7,30,30,5))[0],3)},
        '502535_500_35x25x5':{'volume_cm3':4.375,'old_axis_body_and_pack_overlap_mm3':round(oldEnc.common(block(-17.5,-17.4,2.7,35,25,5)).Volume,3)},
        'conditions':'Retail nominal rectangular envelope only. 35 mm along X in original pack; same negative-X edge and same thickness reference. Alternate packs not installed.'}
    (dest/'fit-check.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
    assert not hits,hits
    assert data['socket_land_mm2']==19 and len(added['CrownFlex'][1].Solids)==1
    assert max(allsh[a].distToShape(allsh[b])[0] for a,b in [('CrownFlex','Daughter'),('CrownZIF','Board'),('GoldFingers','CrownFlex'),('GoldFingers','ZIFContacts'),('ZIFContacts','Board'),('CrownShaft','SwitchFollower')])<1e-6
    # Save an independently selectable, actual axial section rather than a transparent whole watch.
    sec=doc.addObject('App::DocumentObjectGroup','CrownSection');sec.Label=f'剖面 A-A · 表冠轴线 Y = {Y:g}'
    slab=block(7,Y-.07,.05,17,.14,13.1)
    for key in ['Frame','MotorSeat','Motor','EncoderBody','EncoderRotor','SwitchFollower','Daughter','CrownFlex','CrownShaft','GuideSleeve','Crown','Back','Retainer']:
        sl=allsh[key].common(slab)
        if sl.Volume<1e-6:continue
        o=doc.addObject('Part::Feature','AX'+key);o.Label='剖 · '+doc.getObject(key).Label;o.Shape=sl;sec.addObject(o)
        o.ViewObject.ShapeColor=doc.getObject(key).ViewObject.ShapeColor
        o.ViewObject.Visibility=False
    sec.Visibility=False
    mate=doc.addObject('App::DocumentObjectGroup','SocketSection');mate.Label='剖面 B-B · 主板插座接点 Y = 18.75'
    plane=block(.1,18.72,.14,14,.06,12.86)
    for key in ['Board','CrownFlex','FlexStiffener','GoldFingers','ZIFContacts','CrownZIF','Daughter','Motor','MotorSeat','Back']:
        sl=allsh[key].common(plane)
        if sl.Volume<1e-6:continue
        o=doc.addObject('Part::Feature','BX'+key);o.Label='剖 · '+doc.getObject(key).Label;o.Shape=sl;mate.addObject(o)
        o.ViewObject.ShapeColor=doc.getObject(key).ViewObject.ShapeColor;o.ViewObject.Visibility=False
    mate.Visibility=False
    doc.ViewCut.Label='原装配其他剖面 · 上版参考'
    # Uncover the battery-side electrical connection in the default opening view.
    for g in ['Lid','Band','Cell','Mated','Jig','Tools','Meter','FreeHarness','ViewCut']:
        doc.getObject(g).Visibility=False
    for g in ['Circuit','Fixed','Display','Fasteners','Links']:
        doc.getObject(g).Visibility=True
    doc.Frame.ViewObject.Transparency=74
    import FreeCADGui as Gui
    Gui.activeDocument().activeView().setCameraOrientation(App.Rotation(V(1,0,0),156).multiply(App.Rotation(V(0,0,1),-24)))
    Gui.activeDocument().activeView().fitAll()
    doc.Label='WristFlow B · 电机下方的段落表冠'
    doc.recompute()
    out=dest/'WristFlow-B-Crown.FCStd';doc.saveAs(str(out));App.closeDocument(doc.Name)
    check=App.openDocument(str(out))
    for k in review | {'Battery','Board','Tray','CN1'}:
        sh=check.getObject(k).Shape
        assert sh.isValid() and abs(sh.Volume-allsh[k].Volume)<1e-5,k
    original_battery=base['Battery']
    assert abs(check.Battery.Shape.common(original_battery).Volume-original_battery.Volume)<1e-6
    App.closeDocument(check.Name)
    (dest/'reopened.ok').write_text('ok')
