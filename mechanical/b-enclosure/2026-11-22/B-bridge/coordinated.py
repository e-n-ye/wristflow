"""Move the motor, radio module and support services together around the fixed OLED tail."""
from pathlib import Path
import sys,json,math
sys.path.insert(0,str(Path(__file__).resolve().parent))
from compare import (App,Gui,Part,V,B,F,box,rounded,cylinder,turned,HOLES,
                     board_data,SOURCE,HERE,shadow,test,down_prisms)
from assembly import native
from service import wire,ring
from space import calculate

def mirror(shape):
    return shape.mirror(V(),V(1,0,0))

def routed_lead(points):
    pts=[V(*p) for p in points]
    edges=[];start=pts[0]
    for a,b,c in zip(pts,pts[1:],pts[2:]):
        u=(b-a).normalize();v=(c-b).normalize()
        turn=u.getAngle(v)
        d=min(.65,(b-a).Length*.36,(c-b).Length*.36)
        r=d/math.tan(turn/2)
        assert r>.19
        center=b+(v-u).normalize()*(d/math.sin(turn/2))
        left,right=b-u*d,b+v*d
        mid=center+(b-center).normalize()*r
        edges.append(Part.makeLine(start,left))
        edges.append(Part.Arc(left,mid,right).toShape());start=right
    edges.append(Part.makeLine(start,pts[-1]))
    profile=Part.Wire([Part.makeCircle(.19,pts[0],pts[1]-pts[0])])
    return Part.Wire(edges).makePipeShell([profile],True,False)

def run(dest):
    dest=Path(dest)
    opposite_port=dest.name.startswith('mirror-port')
    doc=App.openDocument(str(SOURCE))
    names='Fixed Circuit Fasteners Cell Mated Lid Tools Jig Meter Links FreeHarness Display Band'.split()
    groups={o.Name:key for key in names for o in doc.getObject(key).Group}
    old={k:doc.getObject(k).Shape for k in groups}
    s=dict(old)
    board_holes=[(-x,y) for x,y in HOLES]
    tray_holes=[(12.5,-20.77),(-17.5,10.96)]
    # Keep the original full aperture, installed fold and crown shaft in world coordinates.
    pcb=rounded(36.8,43.2,.8,4.6,z=B)
    cutters=[box(11.,18.34,2.,16.60,-8.30,B-.5),box(13.2,14.,2.,14.3,16.925,B-.5),
             rounded(10.,3.,2,.5,-2.455,9.99,B-.5),box(2.30,2.35,2.,18.35,0,B-.5),
             box(4.05,3.,2.,12.5,-21.55,B-.5),box(3.25,2.15,2.,-18.17,11.06,B-.5)]
    cutters.extend(cylinder(1.24,2.,(x,y,B-.5)) for x,y in board_holes)
    cutters.extend(cylinder(1.66,2.,(x,y,B-.5)) for x in [-19,19] for y in [-14,14])
    for cutter in cutters:pcb=pcb.cut(cutter)
    s['Board']=pcb.removeSplitter()
    for k in ['ModuleParts','ModuleSubstrate','Antenna']:
        s[k]=turned(old[k],(-3.30,-8.30,0),180,(6.60,0,0))
    s['Motor']=old['Motor'].translated(V(26,0,0))
    for k in old:
        if groups[k] in ['Jig','Meter','Links','Fasteners'] or k.startswith(('DBG','Sleeve','TrayScrew')) or (k.startswith('I') and k[1:].isdigit()) or k=='Tray':
            s[k]=mirror(old[k])
    # The catalogue connector rotates in its PCB plane, rather than reflecting a footprint.
    xb=-9.0 if opposite_port else -9.5
    for k in ['BatHeader','BatPlug','FreePlug','PullAccess']:
        s[k]=turned(old[k],(9.,15.45,0),-90,(xb-9.,0,0))
    s['Fixture']=s['Fixture'].cut(rounded(7.10,7.10,2.70,.55,xb,15.45,-4.6)).removeSplitter()
    for i in range(3):
        off=(i-1)*.66
        # Wires leave the same (+X,+Y) edge of the pack, then cross above
        # the upper edge on separate tracks. Pin assignments stay explicit.
        track=10.78+.60*i
        points=[(xb+off,13.26,9.98),(xb+off,12.78,9.94),
                (xb+off,track+.50,7.80),
                (xb+off+.65,track,7.22),(10.95+.54*i,track,7.22),
                (11.6+.54*i,10.06,6.74)]
        s['Lead'+str(i)]=routed_lead(points)
        parked=[(x,y,z-dz) for (x,y,z),dz in zip(points,[3.,3.,1.6,.9,.35,0.])]
        s['FreeLead'+str(i)]=routed_lead(parked)
    back_bank=[k for k in old if k.startswith(('LPad','LFinger'))]
    if opposite_port:
        # Custom rear flex and two matched contact banks reverse together.
        for k in back_bank+['LidTower','LidFlex','ChargeLeads','SpeakerFlex','SpeakerBed','Outlet']:
            s[k]=mirror(old[k])
        s['Speaker']=turned(old['Speaker'],(8.9,16.,0),180,(-17.8,0,0))
        # Keep the original external charging polarity: connect the same two
        # contact positions to the corresponding flex tracks after reflection.
        from service import wire as cable
        a=cable([(-1.75,-15,.88),(-1.75,-15,1.12),(-1.75,-16.15,1.12),
                 (8.70,-16.15,1.12),(8.70,6.2,1.12),(2.,12.7,1.40),(2.,13.52,1.55)],.09)
        b=cable([(1.75,-15,.88),(1.75,-15,1.12),(8.39,-14.8,1.12),
                 (8.39,6.02,1.12),(1.70,12.55,1.40),(1.70,13.52,1.55)],.09)
        s['ChargeLeads']=a.fuse(b)
    else:
        for k in back_bank+['LidTower','LidFlex','SpeakerFlex']:
            s[k]=old[k].translated(V(.60,0,0))
        a=wire([(-1.75,-15,.88),(-1.75,-15,1.11),(-8.7,-14.8,1.11),(-8.7,6.2,1.11),(-1.4,12.7,1.40),(-1.4,13.52,1.55)],.09)
        b=wire([(1.75,-15,.88),(1.75,-15,1.11),(1.75,-16,1.11),(-8.39,-16,1.11),(-8.39,6.02,1.11),(-1.1,12.55,1.4),(-1.1,13.52,1.55)],.09)
        s['ChargeLeads']=a.fuse(b)
    # Rebuild the frame from its unchanged screen seat and crown opening so
    # the OLED tail relief is machined on the actual side of the board.
    base=App.openDocument(str(HERE.parents[1]/'2026-11-21/WristFlow-B.FCStd'))
    frame=base.MiddleFrame.Shape.copy();App.closeDocument(base.Name)
    frame=frame.cut(ring(35.15,41.51,.17,5.03,32.82,39.14,3.78,12.88))
    relief=native(shadow(s['FrontRun']).buffer(.20),12.55,.5)
    for i,(x,y) in enumerate(board_holes):
        boss=cylinder(1.12,1.58,(x,y,B)).fuse(cylinder(1.96,.70,(x,y,F)))
        if i==0:arm=box(3.1,22.2+y,.70,x,(-22.2+y)/2,F)
        else:
            end=19.05 if x>0 else -19.05
            arm=box(abs(end-x),3,.70,(end+x)/2,y,F)
        frame=frame.fuse(boss.fuse(arm).cut(cylinder(.52,1.6,(x,y,B-.1))).cut(relief))
    ribs=box(14.6,3.5,.65,-8.75,20.40,F).fuse(box(5.8,6.8,.65,xb,17.35,F)).fuse(box(3.6,8.3,.65,-15,17.15,F))
    rx=3.15 if opposite_port else -2.55
    ribs=ribs.fuse(box(8.4,2.5,.49,rx,14.18,F)).fuse(box(1.5,9.3,.49,rx,18.05,F))
    frame=frame.fuse(ribs.cut(relief))
    bz=s['Battery'].BoundBox.ZMin
    for (x,y),shelf in zip(tray_holes,[box(3.45,2.2,1.05,12.5,-21.6,bz-.05),box(2.3,1.45,1.05,-18.05,11.06,bz-.05)]):
        frame=frame.fuse(shelf.cut(cylinder(.60,2.3,(x,y,bz-1))))
    outlet_x=-20 if opposite_port else 20
    s['Frame']=frame.fuse(box(1.8,4.8,1,19.65,15.8,6)).cut(box(4.9,4.2,1,outlet_x,16,3.20)).removeSplitter()
    s['MotorSeat']=mirror(old['MotorSeat']).cut(s['Frame']).removeSplitter()
    head=box(3.65,7.8,.75,0,0,-.375)
    head.rotate(V(),V(0,0,1),90);head.rotate(V(),V(1,0,0),65);head.translate(V(-2.455,9.99,(B+F)/2))
    for k,sh in s.items():
        assert sh.isValid() and not sh.isNull(),k
    checks=test(s,groups,head,board_holes,tray_holes)
    hits=checks['penetrations_mm3']
    for k in ['MotorSeat','BatHeader','BatPlug','KeyBody',*[f'Lead{i}' for i in range(3)]]:
        if k=='MotorSeat':others=[j for j in old if j!='MotorSeat']
        elif k.startswith('Lead'):others=['Motor','MotorSeat','Frame','LidTower','LidFlex','Speaker','Outlet','Tray','Board','FrontRun','RearRun','Stiffener','ModuleParts','Antenna']
        else:others=['Motor','MotorSeat','LidTower','Speaker','FrontRun','RearRun','ModuleParts','Antenna']
        for j in others:
            if s[k].BoundBox.intersect(s[j].BoundBox):
                v=s[k].common(s[j]).Volume
                if v>.0005:hits[k+' versus '+j]=v
    for free in ['', 'Free']:
        for i,j in [(0,1),(0,2),(1,2)]:
            k=free+'Lead'+str(i);l=free+'Lead'+str(j)
            v=s[k].common(s[l]).Volume
            if v>.0005:hits[k+' versus '+l]=v
    (dest/'clashes.json').write_text(json.dumps(hits,indent=2),encoding='utf8')
    assert not hits,hits
    assert s['MotorSeat'].distToShape(s['Frame'])[0]<.00001
    assert s['Outlet'].distToShape(s['Speaker'])[0]<.00001
    for i in range(10):
        finger=s['LFinger'+str(i)]
        assert finger.distToShape(s['LPad'+str(i)])[0]<1.e-6 and finger.distToShape(s['LidFlex'])[0]<1.e-6
    # Independently verify the documented RF geometry and the board area held for RF.
    def rfinfo(parts,positive):
        ant=parts['Antenna'];pos=11.10 if positive else -30
        tool=Part.makeBox(18.9,60,3,V(pos,-30,B-1))
        return {'rf_held_host_mm2':parts['Board'].common(tool).Volume/(F-B),
                'antenna_mm2':shadow(ant).area,'host_under_mm2':shadow(ant).intersection(shadow(parts['Board'])).area,
                'cell_overlap_mm2':shadow(ant).intersection(shadow(parts['Battery'])).area,
                'cell_gap_mm':ant.distToShape(parts['Battery'])[0],
                'motor_to_radiator_mm':ant.distToShape(parts['Motor'])[0],
                'screen_flex_overlap_mm2':shadow(ant).intersection(shadow(parts['FrontRun'])).area,
                'antenna_to_crown_shaft_mm':ant.distToShape(parts['CrownShaft'])[0]}
    stats,*_=calculate(s,groups,board_holes,True)
    result={'coordinated':board_data(s['Board']),'baseline':rfinfo(old,False),'mirror':rfinfo(s,True),
            'heights':stats,'checks':checks['checks'],'hits':hits,'rf':checks['rf'],
            'acoustic_port_mirrored':opposite_port,'header_xy':[xb,15.45],'rear_bank_xy':[rx,14.18],
            'slot_xy':[-2.455,9.99],'cn1_xy':checks['cn1_xy'],'clamp_xy':board_holes,'tray_xy':tray_holes,
            'module_xy':checks['module_center_xy'],'antenna_flex_mm':s['Antenna'].distToShape(s['FrontRun'])[0]}
    # Counterfactual: motor and its cut move, radio module stays on its old side.
    motor_cut=box(13.2,14,2,-14.3,16.925,B-.5);stock=rounded(36.8,43.2,.8,4.6,z=B)
    one=old['Board'].fuse(stock.common(motor_cut)).cut(box(13.2,14,2,14.3,16.925,B-.5))
    one=one.cut(cylinder(1.66,2,(-19,14,B-.5))).removeSplitter()
    result['motor_only']=rfinfo(dict(old,Board=one,Motor=s['Motor']),False)
    for k,part in s.items():
        if part is not old[k]:
            obj=doc.getObject(k);obj.Shape=part
            obj.Basis='联动换边方案 · 名义装配，连接器与模组以实际正面旋转，焊盘与针序需复核。'
    doc.Board.Label='B 主板 · 联动换边 / 左右按电池侧实际视向'
    doc.Board.Basis='原屏 10 × 3 mm 穿窗，银色折返端及 CN1 原位。主板 0.8 mm，六层为待定叠层。'
    # Recreate the display-only cross section because a different set of parts can now intersect it.
    cut_group=doc.getObject('ViewCut')
    plane=Part.makeBox(.45,49,20,V(-4.225,-24.5,-2))
    for k in s:
        if groups[k] not in {'Fixed','Circuit','Display','Cell','Mated','Lid','Fasteners','Links'}:continue
        key='Cut'+k;obj=doc.getObject(key);sl=plane.common(s[k])
        if sl.Volume<.00001:
            if obj:doc.removeObject(obj.Name)
        else:
            if obj is None:
                obj=doc.addObject('Part::Feature',key);cut_group.addObject(obj)
            obj.Label=doc.getObject(k).Label+' · 剖面';obj.Shape=sl
            obj.ViewObject.ShapeColor=doc.getObject(k).ViewObject.ShapeColor;obj.ViewObject.Visibility=False
    doc.Label='WristFlow B · 电机、天线与接口联动换边'
    doc.recompute()
    target=dest/'WristFlow-B-Coordinated.FCStd'
    doc.saveAs(str(target));App.closeDocument(doc.Name)
    verify=App.openDocument(str(target))
    for k in s:
        restored=verify.getObject(k).Shape
        assert restored.isValid() and abs(restored.Volume-s[k].Volume)<1.e-6,k
    st=dest/'PCB-B-Coordinated.step';verify.Board.Shape.exportStep(str(st))
    assert abs(board_data(Part.read(str(st)))['web_mm']-result['coordinated']['web_mm'])<1.e-5
    (dest/'layout-check.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf8')
    App.closeDocument(verify.Name)
    (dest/'reopened.ok').write_text('ok')
