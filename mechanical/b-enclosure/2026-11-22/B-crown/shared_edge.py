"""A flat-sided crown and a single lower PCB recess for a screen-side module and motor."""
from pathlib import Path
import sys,json
sys.path.insert(0,str(Path(__file__).resolve().parent))
from center_fit import App,Part,V,HERE,SOURCE,block,prism,bend,union,shaft,rounded_wire
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'B-bridge'))
from coordinated import routed_lead,box,rounded,cylinder
from service import ring
from shapely.geometry import LineString

FROM=HERE/'centered/WristFlow-B-Centered-Crown.FCStd'
DZ=2.80
INSET=2.40
DATASHEET='https://www.lipolbattery.com/LiPo-Battery-Datahseet/LiPo-Battery-LP503030-450mAh.pdf'
def section(s,z):
    return s.common(Part.Face(Part.makePolygon([V(x,y,z) for x,y in
        [(-40,-40),(40,-40),(40,40),(-40,40),(-40,-40)]])))
def section_lift(s,z,h):
    lower=s.common(block(-40,-40,-10,80,80,z+10))
    upper=s.common(block(-40,-40,z,80,80,40))
    band=section(s,z).extrude(V(0,0,h))
    return lower.fuse(band).fuse(upper.translated(V(0,0,h))).removeSplitter()
def mod_turn(s):
    s=s.copy();s.rotate(V(3.30,-8.30,0),V(0,0,1),-90)
    s.translate(V(-9.80,1.05,0))
    # Rotate about the module long axis rather than mirroring a catalogue part.
    s.rotate(V(-6.5,-7.25,11.70),V(0,1,0),180)
    return s
def run(dest):
    dest=Path(dest)
    base=App.openDocument(str(SOURCE));stock=base.Frame.Shape.copy();App.closeDocument(base.Name)
    bare=App.openDocument(str(HERE.parents[1]/'2026-11-21/WristFlow-B.FCStd'))
    inner=bare.MiddleFrame.Shape.copy();App.closeDocument(bare.Name)
    inner=inner.cut(ring(35.15,41.51,.17,5.03,32.82,39.14,3.78,12.88))
    doc=App.openDocument(str(FROM))
    groups='Fixed Circuit Fasteners Cell Mated Lid Links Display Band CrownAssembly'.split()
    initial={o.Name:o.Shape.copy() for g in groups for o in doc.getObject(g).Group}
    changed={}
    def put(key,s,label=None):
        o=doc.getObject(key);o.Shape=s;changed[key]=s
        if label:o.Label=label
        return s
    def add(key,s,label,group,color):
        o=doc.addObject('Part::Feature',key);o.Label=label;o.Shape=s;doc.getObject(group).addObject(o)
        o.ViewObject.ShapeColor=color;changed[key]=s
        return s
    vi=V(-INSET,0,0)
    for k in 'EncoderBody EncoderPins EncoderPads EncoderRotor SwitchFollower Daughter CrownShaft Crown GuideSleeve Retainer RetainerScrews'.split():
        put(k,initial[k].translated(vi))
    # Keep the screw bosses and pressure-test pads at main board height while lifting the screen seat.
    zsplit=12.56
    oldtop=inner.common(block(-40,-40,zsplit,80,80,25))
    frame=stock.cut(oldtop)
    oldclamp=block(-11.15,-23,11.28,4.32,5.95,1.65)
    frame=frame.cut(stock.common(oldclamp).cut(inner))
    oldright=block(11.4,1.96,11.28,7.72,4.09,1.65)
    frame=frame.cut(stock.common(oldright).cut(inner))
    frame=union([frame,section(inner,zsplit).extrude(V(0,0,DZ)),oldtop.translated(V(0,0,DZ))])
    # Move the centred cartridge inside the original 20.55 mm side wall.
    pod=Part.makeLoft([rounded_wire(x-INSET,w,h,z,r) for x,w,h,z,r in
        [(18.30,11,8.25,7.65,1),(20.45,10.7,8,7.65,1.2),
         (22.14,7.4,6.7,7.88,1),(22.94,5.8,5.5,8,1.1)]],True,True)
    pod=pod.cut(block(14,-6,11.07,4.75,12,2))
    frame=frame.fuse(pod)
    for cut in [block(17.75,-3.62,4.49,1.1,7.24,5.73),
                block(18.84,-3.28,4.51,.36,6.56,5.57),block(18.84,-2.52,5.73,3.31,5.04,4.17),
                shaft(22.11,.85,1.18),shaft(22.89,.08,1.29),
                block(17.70,-1.98,1.30,1.,3.96,3.55),block(17.87,-5,4.49,.44,10,5.80)]:
        frame=frame.cut(cut.translated(vi))
    # Recess for the guide flange in the original flush wall.
    frame=frame.cut(shaft(20.54,.2,1.55))
    for y in (-4.42,4.42):frame=frame.cut(shaft(18.27-INSET,2.12,.43,y,8.72))
    # Close the former right-hand antenna/motor scallops in the laminate only.
    rawboard=rounded(36.8,43.2,.8,4.6,z=11.3)
    board=initial['Board'].fuse(rawboard.common(block(11.10,-17.47,11.30,16,18.34,.8)))
    board=board.fuse(rawboard.common(block(7.70,9.925,11.30,15,14,.8)))
    # Retain the case pillar, battery fastener and crown clearances in the rebuilt wing.
    for x,y in ((19,14),(19,-14)):
        board=board.cut(cylinder(1.66,2,(x,y,11.0)))
    board=board.cut(box(2.30,2.35,2.,18.35,0,11.0))
    # One open, stepped edge. Extend the two arms through one another without a web in between.
    notch=rounded(36.5,10.05,2,.5,-15.76+18.25,-25.+5.025,11.).fuse(
        rounded(14.8,14.60,2,.5,13.75,-25+7.30,11.))
    # Allow the entire motherboard to lift off toward the opened back, past the crown cartridge.
    notch=notch.fuse(rounded(7.0,11.55,2,.55,18.72,0,11.))
    board=board.fuse(rawboard.common(cylinder(1.26,2,(13.6,4,11.)))).cut(notch)
    bx,by=5.15,-9.80
    rx,ry=14.55,9.70
    board=board.cut(cylinder(1.24,2,(bx,by,11.0))).cut(cylinder(1.24,2,(rx,ry,11.0))).removeSplitter()
    put('Board',board,'B 主板 · 下边共用缺口 / 左天线、右电机')
    put('Clamp1',initial['Clamp1'].translated(V(bx+9.,by+19.25,0)),'主板压紧螺钉 1 · 移出天线净空')
    put('Clamp2',initial['Clamp2'].translated(V(rx-13.60,ry-4.,0)),'主板压紧螺钉 2 · 避开表冠安装槽')
    shoulder=cylinder(1.12,1.58,(rx,ry,11.30)).fuse(cylinder(1.96,.68,(rx,ry,12.10)))
    frame=frame.fuse(shoulder.fuse(block(rx,ry-1.48,12.1,19.0-rx,2.96,.66)).cut(cylinder(.52,1.7,(rx,ry,11.27))))
    boss=cylinder(1.12,1.58,(bx,by,11.3)).fuse(cylinder(1.96,.68,(bx,by,12.1)))
    arm=block(bx-.65,by-.55,12.1,1.30,-8.4-(by-.55),.65).fuse(block(bx-.65,-9.55,12.1,18.90-bx+.65,1.3,.65))
    frame=frame.fuse(boss.fuse(arm).cut(cylinder(.52,1.7,(bx,by,11.27))))
    for key in ('ModuleSubstrate','ModuleParts','Antenna'):
        put(key,mod_turn(initial[key]))
    # The vibration motor is independently carried by the case and spans the PCB thickness.
    motor=put('Motor',initial['Motor'].translated(V(-.9,-31.5,2.60)),'振动电机 · 同一右下缺口，沉入板厚')
    seat=box(12.05,10.40,.55,12.89,-16.,10.40).cut(frame)
    put('MotorSeat',seat.removeSplitter(),'电机托架 · 固定于中框右内壁')
    # Extend the straight, vertical tangent in the original fold. Cable take-up must be checked on a real screen.
    for key in ('Screen','ActiveArea','GlassTape','FrontRun'):
        put(key,initial[key].translated(V(0,0,DZ)))
    put('Crossing',section_lift(initial['Crossing'],11.68,DZ),'原屏折返 · 按排线可释放 2.80 mm 的路径占位')
    # Use the protected pack's maximum body dimensions from FD_3212_10. Leads are accounted separately.
    drop=.35
    xmin,ymin,zmin=-17.00,-20.40,2.70-drop
    pack=put('Battery',block(xmin,ymin,zmin,32.0,30.5,5.3),'LiPol LP503030 450 mAh · 成品上限 32 × 30.5 × 5.3')
    doc.Battery.Basis='LiPol FD_3212_10 Rev.1，两线带 PCM、无内置 NTC；矩形最大实体，不含出线弯折。'+DATASHEET
    put('Separator',block(xmin,ymin,8.0-drop,32,30.5,.10),'电芯表面 0.10 mm 绝缘层占位')
    xt,yt=12.50,-21.25
    # Move the bottom tray lug away from the corner of the rectangular maximum battery envelope.
    oldshelf=box(3.45,2.2,1.05,12.5,-21.60,2.65)
    frame=frame.cut(oldshelf.cut(inner))
    through=cylinder(.60,2.3,(xt,yt,1.7))
    shelf=box(3.45,2.20,1.05,xt,-21.82,2.65).cut(through)
    frame=frame.fuse(shelf).cut(through).cut(cylinder(1.09,1.45,(xt,yt,.8)))
    # Local clearance under the outer corner of the substrate on the inside of the screen wall.
    mb=changed['ModuleSubstrate'].BoundBox
    frame=frame.cut(block(mb.XMin-.16,mb.YMin-.16,12.07,18.32,28.22,.98))
    put('Frame',frame.removeSplitter(),'直边中框 · Y=0 单冠 / 屏侧垫高 2.80 mm')
    put('TrayScrew1',initial['TrayScrew1'].translated(V(xt-12.5,yt+20.77,-drop)))
    put('TrayScrew2',initial['TrayScrew2'].translated(V(0,0,-drop)))
    # Keep the left lug; shorten the tray at the crown and add the new lower lug.
    tray=initial['Tray'].common(block(-30,-25,1.5,45.44,42,2))
    tray=tray.cut(block(10.65,-23.,2.23,3.80,2.95,.45))
    tray=tray.fuse(block(xmin-.12,ymin-.05,2.25,32.42,30.7,.4))
    tray=tray.fuse(box(3.20,2.65,.4,xt,-20.68,2.25)).cut(through)
    tray=tray.cut(inner).removeSplitter()
    tray=tray.cut(cylinder(1.61,1,(-19,-14,2.0))).removeSplitter()
    tray=tray.translated(V(0,0,-drop))
    tray=tray.cut(block(-16.68,10.20,1.6,15.55,3,1.1))
    for x,y in [(xt,yt),(-17.5,10.96)]:
        collar=cylinder(.82,drop,(x,y,2.65-drop)).cut(cylinder(.60,drop+.1,(x,y,2.60-drop)))
        tray=tray.fuse(collar.cut(inner)).removeSplitter()
    put('Tray',tray,'可拆托板 · 下沉 0.35 mm，两处安装耳补高')
    # Fold the encoder ribbon just outside the shortened tray.
    start=17.61-INSET
    pts=[(start,0),(14.60,0),(14.04,.20),(13.52,.68),(13.12,1.50),(12.8,5.90),
         (12.8,11.75),(11.05,16),(9.8,17.75),(9.38,18.26),(8.93,18.5),(8.1,18.5)]
    clip=LineString(pts).buffer(1.75,cap_style=2,join_style=1)
    edge=[(x,y,1.18) for x,y in list(clip.exterior.coords)[:-1]]
    ribbon=union([block(18.31-INSET,-1.75,1.98,.1,3.5,2.65),bend(start,1.98,-1.75,.75,(1,-1)),
                  prism(edge,(0,0,.10)),bend(8.1,1.98,16.75,.75,(-1,-1)),block(7.30,16.75,1.98,.10,3.5,8.05),
                  bend(6.6,10.03,16.75,.75,(1,1)),block(3.45,16.75,10.73,3.15,3.5,.1)])
    put('CrownFlex',ribbon)
    # Harness for the optional external two-terminal NTC. Positive and negative reach the PCM edge.
    bead=add('SurfaceNTC',block(10.0,8.15,8.10-drop,2.1,1.0,.32),'外贴 NTC · 封装、胶带与热响应待选',
             'Mated',(.38,.63,.62))
    lead=add('NTCReturn',routed_lead([(12.68,10.10,6.74),(12.68,10.53,6.90),
             (12.58,10.72,7.88),(12.45,10.48,8.34),(12.04,9.12,8.34)]).translated(V(0,0,-drop)),
             '外贴 NTC 回线 · 接 BAT− 线束端','Mated',(.28,.33,.32))
    for i in range(3):
        off=(i-1)*.66; track=10.78+.60*i;xx=-9.+off
        pts=[(xx,13.26,9.98),(xx,12.78,9.94),(xx,track+.50,7.80),(xx+.65,track,7.22)]
        if i==1:pts.extend([(9.8,track,7.22),(10.1,10.84,7.94),(10.2,10.42,8.36),(10.35,9.12,8.34)])
        else:pts.extend([(10.95+.54*i,track,7.22),(11.60+.54*i,10.10,6.74)])
        pts=[(x,y,z-(drop if k>=3 else 0)) for k,(x,y,z) in enumerate(pts)]
        put('Lead'+str(i),routed_lead(pts))
        parked=[(x,y,z-([3.,3.,1.6,.9]+[.35]+[0.]*(len(pts)-5))[k])for k,(x,y,z)in enumerate(pts)]
        put('FreeLead'+str(i),routed_lead(parked))
    for i,xx in enumerate((11.75,12.45)):
        land=add('MotorLand'+str(i),block(xx-.26,-7.98,12.10,.52,1.1,.06),'电机焊盘 '+str(i+1),'Circuit',(.79,.62,.33))
        wire=routed_lead([(xx,-11.016,12.99),(xx,-10.55,13.04),(xx,-8.18,13.04),
                          (xx,-7.47,12.57),(xx,-7.42,12.16)])
        add('MotorWire'+str(i),wire,'电机引线 '+str(i+1)+' · 端子朝向试放','Fixed',(.74,.35,.26) if i==0 else (.27,.29,.32))
    # Physical assembly verification. Shared contacts and subassembly interfaces are listed explicitly.
    parts={o.Name:o.Shape for g in groups for o in doc.getObject(g).Group}
    parts['Separator']=doc.Separator.Shape
    interfaces={frozenset(p.split()) for p in ('MotorWire0 Motor','MotorWire1 Motor','MotorWire0 MotorLand0','MotorWire1 MotorLand1','NTCReturn Lead2','NTCReturn SurfaceNTC','Lead1 SurfaceNTC',
        'Lead0 Battery','Lead2 Battery','NTCReturn Battery','Lead1 BatPlug','Lead0 BatPlug','Lead2 BatPlug')}
    hits={}
    for k,s in changed.items():
        assert s.isValid() and s.Volume>0,k
        if k not in parts:continue
        for j,t in parts.items():
            if k==j or (j in changed and j<k) or frozenset((k,j)) in interfaces:continue
            if not s.BoundBox.intersect(t.BoundBox):continue
            v=s.common(t).Volume
            if k in initial and j in initial and initial[k].BoundBox.intersect(initial[j].BoundBox):
                v-=initial[k].common(initial[j]).Volume
            if v>.0001:hits[k+'/'+j]=[round(v,5),str(s.common(t).BoundBox)]
    moving=('Crown','CrownShaft','SwitchFollower')
    for key in moving:
        s=parts[key].translated(V(-.25,0,0))
        for j,t in parts.items():
            if j not in moving and s.BoundBox.intersect(t.BoundBox):
                v=s.common(t).Volume
                if v>.0001:hits['pressed '+key+'/'+j]=round(v,5)
    gap=lambda a,b:round(parts[a].distToShape(parts[b])[0],4)
    measurements={k:gap(*k.split('/')) for k in ['Motor/Antenna','ModuleParts/Screen','ModuleParts/FrontRun',
        'Retainer/Battery','CrownFlex/Tray','CrownFlex/Back','MotorSeat/Frame','Tray/Battery',
        'ModuleParts/Frame','Motor/Screen','ModuleSubstrate/Clamp1','Lead1/SurfaceNTC','NTCReturn/SurfaceNTC',
        'NTCReturn/Lead2','Retainer/Daughter','Retainer/Frame','Daughter/Frame','CrownFlex/Daughter',
        'Crossing/FrontRun','Crossing/RearRun','SurfaceNTC/Separator','Tray/Frame','Tray/LidFlex','Tray/PpgBoard','Tray/ChargeLeads','Tray/Back','Tray/Speaker','Antenna/Clamp1']}
    front=initial['Screen'];fpc=initial['FrontRun']
    ant=parts['Antenna'];poly=section(ant,ant.BoundBox.Center.z)
    host=section(board,11.65).translated(V(0,0,ant.BoundBox.Center.z-11.65))
    # Bounding rectangles for Fig 8-2, including the outside portions of the castellated lands.
    tip=-21.20;tail=6.70;cx=-6.5
    lands=[]
    for sign in (-1,1):
        for i in range(18):
            lands.append(box(1.80,.70,.04,cx+sign*8.60,tip+7.1+i*1.10,12.05))
    for i in range(14):
        lands.append(box(.70,1.80,.04,cx-7.15+i*1.10,tail-.40,12.05))
    for i in range(4):
        for j in range(3):
            lands.append(box(1.1,.8,.04,cx-2.28+j*1.8,tip+12.45+i*1.2,12.05))
    for i in range(3):
        for j in (0,2):
            lands.append(box(1.1,.8,.04,cx-2.28+j*1.8,tail-3.45-i*1.2,12.05))
    missing_land_mm2=sum(max(0.,p.Volume-p.common(board).Volume) for p in lands)/.04
    metrics={'delta_screen_mm':DZ,'tray_drop_mm':drop,'bounds':{k:[round(getattr(parts[k].BoundBox,v),3) for v in
        ('XMin','XMax','YMin','YMax','ZMin','ZMax')] for k in ('Frame','Crown','Board','ModuleSubstrate','ModuleParts','Antenna','Motor','Battery')},
        'gaps_mm':measurements,'press_mm':{k:round(parts['Crown'].translated(V(-.25,0,0)).distToShape(parts[k])[0],4) for k in ['Frame','GuideSleeve']},
        'interferences_mm3':hits,'antenna_host_overlap_mm2':round(poly.common(host).Area,4),
        'original_stack_intersection_mm3':{k:round(parts['ModuleParts'].common(s).Volume,3)for k,s in [('screen',front),('screen_flex',fpc)]},
        'pad_count':len(lands),'unsupported_pad_area_mm2':round(missing_land_mm2,5),
        'battery_datasheet':DATASHEET,'limits':'Assembly envelopes only. FPC take-up, battery harness/NTC, motor lead orientation, electrical footprint, RF and housing tolerance to be verified.'}
    pcb_sweep=section(board,11.70).translated(V(0,0,.40)).extrude(V(0,0,-16.0))
    metrics['pcb_rearward_sweep_overlap_mm3']={k:round(pcb_sweep.common(parts[k]).Volume,5)
        for k in ['Frame','Motor','MotorSeat','Daughter','Retainer','EncoderBody']}
    (dest/'layout-check.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2),encoding='utf8')
    assert not hits,hits
    assert metrics['antenna_host_overlap_mm2']<1e-5 and len(board.Solids)==len(frame.Solids)==len(ribbon.Solids)==1
    assert len(lands)==68 and missing_land_mm2<.0001
    assert max(metrics['pcb_rearward_sweep_overlap_mm3'].values())<.0001,metrics['pcb_rearward_sweep_overlap_mm3']
    for k in ['MotorSeat/Frame','Retainer/Daughter','Retainer/Frame','Daughter/Frame','CrownFlex/Daughter','Crossing/FrontRun','Crossing/RearRun']:
        assert measurements[k]<1e-5,(k,measurements[k])
    # Rebuild review sections including the screen-side module and new cell.
    for group,prefix,slab in [('CrownSection','AX',block(-18,-.07,0,42,.14,20)),
                             ('SocketSection','BX',block(.1,18.72,.14,14,.06,18.86))]:
        sub=doc.getObject(group);existing={o.Name[len(prefix):]:o for o in sub.Group}
        for key in set(existing)|{'ModuleParts','Screen','ModuleSubstrate','FrontRun','Board','Battery','Tray','Frame'}:
            sl=parts[key].common(slab);o=existing.get(key)
            if sl.Volume<1e-6:
                if o:doc.removeObject(o.Name)
                continue
            if not o:o=doc.addObject('Part::Feature',prefix+key);sub.addObject(o)
            o.Label='剖 · '+doc.getObject(key).Label;o.Shape=sl;o.ViewObject.ShapeColor=doc.getObject(key).ViewObject.ShapeColor
            o.Visibility=False
        sub.Visibility=False
    doc.Label='WristFlow B · 屏侧纵向模组 / 下沿共用缺口 / 直边单冠'
    for k in ('Frame','Screen','ActiveArea','GlassTape','FrontRun','Crossing','RetainerScrews'):
        doc.getObject(k).Visibility=False
    doc.Separator.Visibility=False
    import FreeCADGui as Gui
    Gui.activeDocument().activeView().viewTop();Gui.activeDocument().activeView().fitAll()
    doc.recompute();path=dest/'WristFlow-B-Shared-Recess.FCStd'
    doc.saveAs(str(path));App.closeDocument(doc.Name)
    check=App.openDocument(str(path))
    for key,s in changed.items():
        t=check.getObject(key).Shape
        assert t.isValid() and abs(t.Volume-s.Volume)<1e-5,key
    stp=dest/'B-PCB-outline.step'
    check.Board.Shape.exportStep(str(stp))
    imported=Part.read(str(stp))
    assert imported.isValid() and len(imported.Solids)==1 and abs(imported.Volume-board.Volume)<1e-5
    App.closeDocument(check.Name);(dest/'reopened.ok').write_text('ok')
