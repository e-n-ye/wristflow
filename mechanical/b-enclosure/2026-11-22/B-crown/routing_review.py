"""Direct crown contacts, integrated rear flex, ALS light pipe and outward module trial."""
from pathlib import Path
import sys,math,json
sys.path.insert(0,str(Path(__file__).resolve().parent))
from shared_edge import App,Part,V,block,box,prism,union,section,HERE,cylinder,rounded
from layout_space import shadow

BASE=HERE/'B-layout-review/WristFlow-B-Shared-Recess.FCStd'
FLEX=.12
def face(points):
    vertices=[V(*p) for p in points]
    return Part.Face(Part.makePolygon(vertices+[vertices[0]]))
def rim(cx,y,z,r):
    return Part.Wire([Part.makeCircle(r,V(cx,y,z),V(0,0,1))])
def tube_loft(x,stations):
    return Part.makeLoft([rim(x,y,z,r) for y,z,r in stations],True,True)
def routed_flex(opening):
    # Keep both horizontal tangents. As the back opens, the fold translates by half the opening.
    w=5.5;x=.45;end=18.5-opening/2
    low=1.08-opening; R=1.05; high=low+2*R
    def bend(y,z,r,branch):
        c=Part.makeCylinder(r+FLEX/2,w,V(x,y,z),V(1,0,0)).cut(Part.makeCylinder(r-FLEX/2,w,V(x,y,z),V(1,0,0)))
        t=r+.08
        clip={'return':(y,z-t, t,2*t),'rise':(y-t,z-t,t,t),'entry':(y-t,z,t,t)}[branch]
        return c.common(block(x,clip[0],clip[1],w,clip[2],clip[3]))
    rise=12.80;top=10.78;lower_bend=rise+1.
    s=[block(x,5.,low-FLEX/2,w,end-5.,FLEX),bend(end,low+R,R,'return'),
       block(x,lower_bend,high-FLEX/2,w,end-lower_bend,FLEX),bend(lower_bend,high+1,1,'rise'),
       block(x,rise-FLEX/2,high+1,w,FLEX,top-1-(high+1)),
       bend(rise+1,top-1,1,'entry'),block(x,rise+1,top-FLEX/2,w,16.55-(rise+1),FLEX)]
    length=end-5 +math.pi*R+(end-lower_bend)+math.pi+(top-1-high-1)+16.55-(rise+1)
    return union(s),length
def run(dest):
    dest=Path(dest);doc=App.openDocument(str(BASE))
    groups='Fixed Circuit Fasteners Cell Mated Lid Links Display Band CrownAssembly'.split()
    src={o.Name:o.Shape.copy() for g in groups for o in doc.getObject(g).Group}
    old={k:s.copy() for k,s in src.items()};s=dict(src);modified=set()
    colors={'pcb':(.13,.49,.39),'gold':(.87,.67,.28),'flex':(.83,.51,.22),'metal':(.57,.65,.65),
            'optic':(.40,.73,.77),'mask':(.19,.24,.25)}
    def setp(k,shape,label=None,group=None,color=None):
        o=doc.getObject(k)
        if o is None:
            o=doc.addObject('Part::Feature',k);doc.getObject(group).addObject(o)
        o.Shape=shape;s[k]=shape;modified.add(k)
        if label:o.Label=label
        if color:o.ViewObject.ShapeColor=colors[color]
        return shape
    def omit(k):
        doc.removeObject(k);s.pop(k,None)
    displaced=['CrownFlex','FlexStiffener','CrownZIF','ZIFContacts','GoldFingers','LidTower','ChargeLeads',
               'LidFlex','SpeakerFlex']+[f'{tag}{i}' for tag in ['LPad','LFinger'] for i in range(10)]
    for k in displaced:omit(k)
    # Every spring is on the motherboard. A retaining shelf above the mating PCB closes the mechanical load path.
    top=union([s['Daughter'],block(15.91,-3.35,10.10,.54,6.70,3.025)])
    setp('Daughter',top,'表冠立板 · 上缘接直角焊板')
    ledge=setp('CrownBridge',block(11.77,-4.75,13.125,4.68,9.50,.60),'表冠水平接触板 · 刚性直角焊接','CrownAssembly','pcb')
    frame=s['Frame'].cut(block(15.68,-3.58,9.82,.99,7.16,3.97))
    frame=frame.cut(block(11.69,-4.85,13.08,4.94,9.70,.68))
    frame=frame.fuse(block(15.78,-4.70,13.725,3.35,9.4,.45)).removeSplitter()
    signals=['A','GND','B','SW1','SW2']
    cf=[];cp=[];joints=[]
    for i,key in enumerate(signals):
        y=(i-2)*1.90
        # Compressed envelope, not the spring tooling. One spring per signal.
        profile=[(0,0),(3,0),(3,.13),(.36,.13),(.18,.26),(.24,.40),(2.63,.84),(2.63,1.0),
                 (2.42,1.0),(.17,.55),(0,.40)]
        cf.append(setp('CrownLeaf'+str(i),prism([(12.+a,y-.75,12.10+b) for a,b in profile],(0,1.5,0)),
             key+' · 主板弹片 S7241-45R，压缩态包络','Circuit','gold'))
        cp.append(setp('CrownPad'+str(i),block(12.15,y-.83,13.10,2.76,1.66,.025),
                      key+' · 表冠对接金面','CrownAssembly','gold'))
        yy=(i-2)*1.24
        joints.append(prism([(15.91,yy-.22,12.84),(15.91,yy-.22,13.125),(15.63,yy-.22,13.125)],(0,.44,0)))
    setp('CrownJoints',Part.makeCompound(joints),'五路直角焊端 · 铜箔扇出在板内','CrownAssembly','gold')
    # Move the complete module and carry only its antenna end into the frame pocket.
    for k in ('ModuleSubstrate','ModuleParts','Antenna'):setp(k,s[k].translated(V(0,-1,0)))
    # Open the relief to the rear assembly face so the installed radio module can travel with the board.
    frame=frame.cut(block(-15.66,-22.36,.89,18.32,3.36,12.19))
    pcb=s['Board']
    for x in (-15.10,2.10):
        pcb=pcb.fuse(block(x-1.06,-15.69,11.3,2.12,1.11,.8))
    setp('Board',pcb.removeSplitter(),'B 主板 · 天线前移 1 mm，补足两枚外侧焊盘')
    # Front-facing ambient light window and light guide. It does not use the AMOLED cover as an optical aperture.
    ax=8.25; ay=20.25
    setp('AmbientALS',block(ax-1,ay-1,12.10,2,2,.70),'环境光 LTR-303ALS-01 · 朝前','Circuit','mask')
    stations=[(ay,12.94,.42),(ay+.18,13.58,.55),(21.72,15.42,.61),(22.13,16.12,.61),(22.13,18.23,.61)]
    guide=tube_loft(ax,stations)
    setp('ALSPipe',guide,'前框环境光导光柱 · 光学材料与标定待定','Fixed','optic')
    large=[(y,z,r+.16) for y,z,r in stations]
    sleeve=tube_loft(ax,large).cut(guide)
    setp('ALSBaffle',sleeve,'环境光独立黑色遮光套','Fixed','mask')
    frame=frame.cut(tube_loft(ax,[(y,z,r+.20) for y,z,r in stations]))
    frame=frame.cut(block(ax-1.12,ay-1.12,12.085,2.24,2.24,.87))
    frame=frame.cut(cylinder(.86,.46,(ax,22.13,18.20))).cut(cylinder(1.08,.21,(ax,22.13,18.62)))
    lens=cylinder(.76,.40,(ax,22.13,18.23)).fuse(cylinder(.94,.17,(ax,22.13,18.63)))
    setp('ALSLens',lens,'独立前窗 · 透光面 Ø1.88，不开原屏','Fixed','optic')
    adhesive=cylinder(.86,.39,(ax,22.13,18.23)).cut(cylinder(.76,.42,(ax,22.13,18.22)))
    setp('ALSSeal',adhesive,'前窗周向胶位 · 密封待验证','Fixed','mask')
    setp('Frame',frame.removeSplitter())
    # Rear flex exits the sensor board at the perimeter; light travels through the back windows.
    rear,length=routed_flex(0)
    setp('LidFlexNew',rear,'后盖唯一插接尾 · 10 路，预留开盖折返','Lid','flex')
    setp('LidEnd',block(.45,14.12,10.54,5.5,2.43,.18),'后盖插接补强 · 合计 0.30 mm','Lid','pcb')
    shell=block(-.30,14.65,10.30,7.,3.80,1.).cut(block(.37,14.64,10.50,5.66,1.96,.38))
    comb=[];landing=[]
    for i in range(10):
        x=3.20+(i-4.5)*.50
        landing.append(block(x-.14,15.22,10.84,.28,1.33,.012))
        comb.append(union([block(x-.08,15.75,10.852,.16,.96,.066),
                           block(x-.08,16.71,10.852,.16,.15,.448)]))
    copper=Part.makeCompound(comb)
    setp('RearSocket',shell.cut(copper),'后盖 ZIF · FH34SRJ-10S-0.5SH(50) 外形包络','Circuit','mask')
    setp('RearFingers',Part.makeCompound(landing),'十路金手指 · 光学六路、喇叭两路、充电两路','Lid','gold')
    setp('RearContactTips',copper,'座内接点到主板焊接面 · 位置示意','Circuit','gold')
    # Distributed copper on the SAME rear flex. The two external power pins land on separate pads.
    charge=union([block(-2.45,-15.74,.96,4.9,2.10,.12),block(-1.05,-13.64,.96,2.1,8.64,.12)])
    setp('ChargeBranch',charge,'充电支路 · 正负各自走铜，沿盖贴附','Lid','flex')
    for i,x in enumerate((-1.75,1.75)):
        setp('ChargeTab'+str(i),cylinder(.46,.07,(x,-15,.89)),'充电柱尾部焊盘 '+str(i+1),'Lid','gold')
    spk=block(-1.4,13.62,1.02,1.85,1.55,.12)
    setp('SpeakerBranch',spk,'喇叭支路 · 两路在后盖柔板汇合','Lid','flex')
    stabs=[block(-1.59,yy,1.14,.32,.28,.04) for yy in (13.79,14.65)]
    setp('SpeakerTags',Part.makeCompound(stabs),'喇叭端短焊片 · 出线方向待实物确认','Lid','gold')
    setp('SpeakerBed',s['SpeakerBed'].cut(block(-1.67,13.55,.89,.58,1.72,.31)),
         '后盖喇叭底托 · 内侧留焊片出口')
    # A ledge and adhesive land hold the bonded portion on the rear cover. Keep all bends free.
    bed=union([block(.45,5.,.90,5.50,6.65,.12),block(-2.42,-15.7,.90,4.84,2.10,.06),
               block(-1.04,-13.61,.90,2.08,8.60,.06),block(-1.36,13.64,.90,1.79,1.51,.12)])
    for x in (-1.75,1.75):bed=bed.cut(cylinder(.52,.2,(x,-15,.88)))
    # Clear the rear optical blackout insert where it already supplies a mounting surface.
    bed=bed.cut(s['Baffle'])
    setp('LidBond',bed,'后盖柔板固定胶位 · U 形折返不粘','Lid','metal')
    optical=[]
    for i,(x,y) in enumerate([(-2.70,0),(2.70,0),(0,2.40)]):
        optical.append(cylinder(.68,.58,(x,y,.42)))
    setp('OpticalDice',Part.makeCompound(optical),'PPG 发光与接收封装占位 · 三条独立光路对准原窗','Lid','optic')
    doc.PpgBoard.Label='后盖光学刚板 · 连接整体柔板，LED / 接收器朝皮肤'
    # Numerically compare translating the board as a stack (normal to its faces).
    heights=[]
    for dz in (0,.25,.50,1.,1.20):
        heights.append(dict(toward_lid=dz,board_back_z=11.30-dz,display_clear=round(15.83-(12.1-dz),2),
            cell_component_height=round(11.30-dz-7.75-.30-.20,2),
            gap_to_original_daughter_top=round(11.30-dz-10.10,2)))
    EPS=.0001
    for k in modified:assert s[k].isValid() and s[k].Volume>0,k
    hits={}
    for k in modified:
        for j,t in s.items():
            if j==k or (j in modified and j<k) or not s[k].BoundBox.intersect(t.BoundBox):continue
            v=s[k].common(t).Volume
            if k in old and j in old and old[k].BoundBox.intersect(old[j].BoundBox):v-=old[k].common(old[j]).Volume
            if v>EPS:hits[k+'/'+j]=[round(v,4),str(s[k].common(t).BoundBox)]
    opening_test={}
    for gap in (0,1.5,3.,4.5,6.):
        flex,dev=routed_flex(gap);assert abs(dev-length)<1e-6
        fixed=['Board','Battery','Tray','Frame','BatPlug','CN1','Lead0','Lead1','Lead2','RearSocket','CrownBridge']
        overlap={k:round(flex.common(s[k]).Volume,5) for k in fixed if flex.BoundBox.intersect(s[k].BoundBox)}
        for k in ('Speaker','Back','Baffle','PpgBoard','LidBond'):
            t=s[k].translated(V(0,0,-gap));overlap[k]=round(flex.common(t).Volume,5)
        opening_test[gap]={k:v for k,v in overlap.items() if v>EPS}
    gap=lambda a,b:round(s[a].distToShape(s[b])[0],4)
    antenna=shadow(s['Antenna']);screen=shadow(s['Screen'])
    oldant=shadow(old['Antenna'])
    antips=section(s['Antenna'],s['Antenna'].BoundBox.Center.z)
    boardface=section(s['Board'],11.65).translated(V(0,0,s['Antenna'].BoundBox.Center.z-11.65))
    pads=[]
    for sign in (-1,1):
        for i in range(18):pads.append(box(1.8,.70,.04,-6.5+sign*8.6,-22.20+7.1+i*1.1,12.05))
    for i in range(14):pads.append(box(.7,1.8,.04,-6.5-7.15+i*1.1,5.70-.4,12.05))
    for i in range(4):
        for j in range(3):pads.append(box(1.1,.8,.04,-6.5-2.28+j*1.8,-22.2+12.45+i*1.2,12.05))
    for i in range(3):
        for j in (0,2):pads.append(box(1.1,.8,.04,-6.5-2.28+j*1.8,5.7-3.45-i*1.2,12.05))
    sweep=section(s['Board'],11.70).translated(V(0,0,.40)).extrude(V(0,0,-16))
    sweepcheck={k:round(sweep.common(s[k]).Volume,5) for k in ('Frame','Daughter','Retainer','Motor','MotorSeat','CrownBridge')}
    # Use the full swept envelopes of the projecting module, ALS, and spring strips during back-side removal.
    loaded_sweep={}
    for k in ('ModuleSubstrate','ModuleParts','Antenna','AmbientALS','RearSocket')+tuple('CrownLeaf'+str(i) for i in range(5)):
        bb=s[k].BoundBox
        travel=block(bb.XMin,bb.YMin,bb.ZMin-16,bb.XLength,bb.YLength,bb.ZLength+16)
        for j in ('Frame','Daughter','Retainer','Motor','MotorSeat','CrownBridge'):
            if travel.BoundBox.intersect(s[j].BoundBox):
                v=travel.common(s[j]).Volume
                if v>EPS:loaded_sweep[k+'/'+j]=round(v,5)
    chain=['CrownPad0/CrownLeaf0','CrownLeaf0/Board','CrownBridge/Daughter','CrownBridge/Frame','RearFingers/RearContactTips',
           'LidFlexNew/PpgBoard','ChargeBranch/PpgBoard','ChargeTab0/Contact0','SpeakerBranch/LidFlexNew',
           'SpeakerTags/Speaker','SpeakerTags/SpeakerBranch','ALSLens/ALSPipe','ALSPipe/AmbientALS','ALSSeal/ALSLens',
           'ALSLens/Screen','ALSBaffle/Screen','ModuleSubstrate/Frame','Antenna/Frame','Antenna/Motor','Antenna/Screen',
           'LidFlexNew/Battery','LidFlexNew/Tray']
    oldpath=[(15.21,0),(14.6,0),(14.04,.2),(13.52,.68),(13.12,1.5),(12.8,5.9),(12.8,11.75),(11.05,16),(9.8,17.75),(9.38,18.26),(8.93,18.5),(8.1,18.5)]
    data=dict(board_height_trial=heights,old_crown_fpc_length=round(2.65+3*math.pi*.75/2+8.05+3.15+sum(math.dist(a,b) for a,b in zip(oldpath,oldpath[1:])),2),
        new_crown_fpc_length=0,antenna_mm2=round(antenna.area,2),old_screen_out_mm2=round(oldant.difference(screen).area,2),
        new_screen_out_mm2=round(antenna.difference(screen).area,2),outside_both_screen_cell=round(antenna.difference(screen.union(shadow(s['Battery']))).area,2),
        min_Y_wall_below_screen_mm=1.12,pcb_under_antenna_mm2=round(antips.common(boardface).Area,5),
        unsupported_68_pads_mm2=round(sum(max(0,p.Volume-p.common(s['Board']).Volume) for p in pads)/.04,6),installed_delta_intersections=hits,
        rear_service_opening_hits=opening_test,rear_service_length_mm=round(length,3),board_sweep_hits=sweepcheck,
        loaded_removal_envelope_hits=loaded_sweep,
        distances_mm={k:gap(*k.split('/')) for k in chain})
    (dest/'connection-check.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
    assert not hits,hits
    assert all(not v for v in opening_test.values()),opening_test
    assert max(sweepcheck.values())<EPS,sweepcheck
    assert not loaded_sweep,loaded_sweep
    assert data['pcb_under_antenna_mm2']<EPS and data['unsupported_68_pads_mm2']<EPS
    for k in chain[:12]:assert data['distances_mm'][k]<.0001,(k,data['distances_mm'][k])
    assert s['RearContactTips'].distToShape(s['Board'])[0]<1e-6
    assert s['ALSSeal'].distToShape(s['Frame'])[0]<1e-6
    for i,(x,y) in enumerate([(-2.7,0),(2.7,0),(0,2.4)]):
        aperture=cylinder(.40,1.25,(x,y,-.83))
        for k in ('Back','Baffle','ChargeBranch','LidFlexNew'):assert aperture.common(s[k]).Volume<EPS,(k,i)
    for i in range(5):
        assert cf[i].distToShape(cp[i])[0]<1e-6
        assert abs(cf[i].BoundBox.ZLength-1.)<1e-6
    # Refresh both inherited reference cuts to avoid displaying the removed ZIF in an old section.
    for group,prefix,slab in [('CrownSection','AX',block(-18,-.07,0,42,.14,20)),
                             ('SocketSection','BX',block(-.30,15.89,.14,7.,.06,18.86))]:
        sub=doc.getObject(group)
        existing={o.Name[len(prefix):]:o for o in sub.Group}
        for key in set(existing)|{'CrownLeaf2','CrownPad2','CrownBridge','RearSocket','LidFlexNew','RearFingers','PpgBoard'}:
            o=existing.get(key);sl=s[key].common(slab) if key in s else None
            if sl is None or sl.Volume<1e-6:
                if o:doc.removeObject(o.Name)
                continue
            if not o:o=doc.addObject('Part::Feature',prefix+key);sub.addObject(o)
            o.Shape=sl;o.Label='剖 · '+doc.getObject(key).Label;o.ViewObject.ShapeColor=doc.getObject(key).ViewObject.ShapeColor;o.Visibility=False
        sub.Visibility=False
    doc.SocketSection.Label='剖面 · 后盖 10P 插座'
    # A separate fold is selectable to inspect the 6 mm service position.
    service=doc.addObject('App::DocumentObjectGroup','LidService6');service.Label='后盖打开 6 mm · 等长折返示意'
    for key in ['Back','PpgBoard','Speaker','LidFlexNew','ChargeBranch','SpeakerBranch']:
        o=doc.addObject('Part::Feature','Open6'+key);o.Label='开盖 · '+doc.getObject(key).Label
        o.Shape=routed_flex(6)[0] if key=='LidFlexNew' else s[key].translated(V(0,0,-6))
        service.addObject(o);o.ViewObject.ShapeColor=doc.getObject(key).ViewObject.ShapeColor;o.Visibility=False
    service.Visibility=False
    doc.Label='WristFlow B · 表冠免排线 / 后盖单尾 / 独立环境光窗'
    doc.recompute();target=dest/'WristFlow-B-Connections.FCStd';doc.saveAs(str(target));App.closeDocument(doc.Name)
    d=App.openDocument(str(target))
    for k in modified:assert d.getObject(k).Shape.isValid() and abs(s[k].Volume-d.getObject(k).Shape.Volume)<1e-5,k
    d.Board.Shape.exportStep(str(dest/'B-PCB-outline.step'));App.closeDocument(d.Name)
    (dest/'reopened.ok').write_text('ok')
