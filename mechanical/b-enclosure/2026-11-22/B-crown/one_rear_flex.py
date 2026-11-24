"""Rear-lid FPC with a component island and a separately bonded FR-4 stiffener."""
from pathlib import Path
import sys,json
sys.path.insert(0,str(Path(__file__).resolve().parent))
from routing_review import App,Part,V,HERE,block,cylinder,rounded,prism,union,routed_flex

BASE=HERE/'B-connections/WristFlow-B-Connections.FCStd'
FPC=.12
Z0=1.02
AMBER=(.84,.51,.22)
def speaker_branch():
    outline=[(-1.4,13.62),(-.97,13.62),(-.97,11.18),(-.53,10.60),(.52,10.60),
             (.52,11.55),(.13,11.55),(.04,11.67),(.04,14.93),(-.18,15.17),(-1.4,15.17)]
    return prism([(x,y,Z0) for x,y in outline],(0,0,FPC))
def fixed_flex():
    island=rounded(12.,10.,FPC,.75,0,0,Z0)
    # Blend the optical island into the tail across the full 5.5 mm width.
    neck=prism([(.45,4.0,Z0),(5.95,4.0,Z0),(5.95,5.08,Z0),(.45,5.08,Z0)],(0,0,FPC))
    charge_pad=rounded(4.9,2.10,FPC,.22,0,-14.69,Z0)
    branch=prism([(-1.05,-14.10,Z0),(1.05,-14.10,Z0),(1.05,-5.58,Z0),
                  (1.62,-5.0,Z0),(1.62,-4.77,Z0),(-1.62,-4.77,Z0),
                  (-1.62,-5.0,Z0),(-1.05,-5.58,Z0)],(0,0,FPC))
    return union([island,neck,charge_pad,branch]),speaker_branch()
def assembled(region,spk,opening):
    tail,length=routed_flex(opening)
    shift=V(0,0,-opening)
    body=union([tail,region.translated(shift),spk.translated(shift)])
    assert body.isValid() and len(body.Solids)==1
    return body,length
def run(dest):
    dest=Path(dest);doc=App.openDocument(str(BASE))
    def add(key,label,sh,group,col):
        o=doc.getObject(key) or doc.addObject('Part::Feature',key)
        if o not in group.Group:group.addObject(o)
        o.Label=label;o.Shape=sh;o.ViewObject.ShapeColor=col
        return o
    lid=doc.Lid; service=doc.LidService6
    for name in ['Open6PpgBoard','Open6ChargeBranch','Open6SpeakerBranch','AXPpgBoard','ChargeBranch','SpeakerBranch','PpgBoard']:
        if doc.getObject(name):doc.removeObject(name)
    region,spk=fixed_flex();full,length=assembled(region,spk,0)
    body=add('LidFlexNew','整张后盖 FPC · 充电端、血氧贴装区、喇叭支路、10P 尾端',full,lid,AMBER)
    glue=add('PpgStiffAdhesive','血氧区补强胶 · 0.05 mm 试放',rounded(10.8,8.5,.05,.60,0,0,Z0+FPC),lid,(.87,.76,.52))
    stiff=add('PpgStiffener','血氧区 FR-4 补强 · 0.30 mm 试放',rounded(10.8,8.5,.30,.60,0,0,Z0+FPC+.05),lid,(.35,.56,.48))
    sensors=doc.OpticalDice;sensors.Shape=sensors.Shape.translated(V(0,0,.02));sensors.Label='FPC 贴装面 · 发光 / 接收器朝三只光窗'
    for i,x in enumerate([-1.75,1.75]):
        doc.getObject('ChargeTab'+str(i)).Shape=cylinder(.46,Z0-.89,(x,-15,.89))
    # Only fixed sections are bonded to the lid. The optical gasket has three through-holes.
    foot=union([block(.45,5,.9,5.50,6.65,.12),block(-2.42,-15.70,.9,4.84,2.06,.12),
                block(-1.03,-13.65,.9,2.06,8.65,.12),spk.translated(V(0,0,-.12))])
    for x in [-1.75,1.75]:foot=foot.cut(cylinder(.51,.3,(x,-15,.88)))
    doc.LidBond.Shape=foot.cut(doc.SpeakerBed.Shape).removeSplitter()
    gasket=rounded(10.9,8.6,.12,.70,0,0,.90)
    for x,y in [(-2.70,0),(2.70,0),(0,2.40)]:gasket=gasket.cut(cylinder(1.13,.20,(x,y,.87)))
    seat=add('PpgMount','血氧区遮光定位胶 · 对准后盖三窗',gasket,lid,(.21,.25,.25))
    # Refresh the matching axial cuts, and display the true full flex in the open-cover study.
    clip=block(-18,-.07,0,42,.14,20)
    for obj in (body,stiff,glue,seat):
        sl=obj.Shape.common(clip)
        if sl.Volume>1e-7:
            o=add('AX'+obj.Name,'剖 · '+obj.Label,sl,doc.CrownSection,obj.ViewObject.ShapeColor[:3]);o.Visibility=False
    for obj in (stiff,glue,seat,sensors):
        o=add('Open6'+obj.Name,'开盖 · '+obj.Label,obj.Shape.translated(V(0,0,-6)),service,obj.ViewObject.ShapeColor[:3]);o.Visibility=False
    opened=doc.Open6LidFlexNew;opened.Shape=assembled(region,spk,6.)[0];opened.Label='开盖 6 mm · 整张后盖 FPC'
    opened.ViewObject.ShapeColor=AMBER;opened.Visibility=False
    for grp in (service,doc.SocketSection,doc.CrownSection):grp.Visibility=False
    excluded={'LidFlexNew','OpticalDice','RearFingers','SpeakerTags','LidEnd','PpgMount','PpgStiffAdhesive',
              'PpgStiffener','ChargeTab0','ChargeTab1','LidBond'}
    fixed=[o for g in ('Fixed','Circuit','Fasteners','Cell','Mated','Links','Display','CrownAssembly')
           for o in doc.getObject(g).Group if hasattr(o,'Shape')]
    back=[o for o in lid.Group if hasattr(o,'Shape') and o.Name not in excluded]
    hits={}
    for opening in (0.,1.5,3.,4.5,6.):
        sh,dev=assembled(region,spk,opening)
        assert abs(dev-length)<1e-7
        for a in (sh,stiff.Shape.translated(V(0,0,-opening)), glue.Shape.translated(V(0,0,-opening)),
                  seat.Shape.translated(V(0,0,-opening)), sensors.Shape.translated(V(0,0,-opening))):
            for o,b in [(o,o.Shape) for o in fixed]+[(o,o.Shape.translated(V(0,0,-opening))) for o in back]:
                if a.BoundBox.intersect(b.BoundBox):
                    v=a.common(b).Volume
                    if v>.0001:hits[(opening,o.Name)]=v
    assert not hits,hits
    members=[body,sensors,stiff,glue,seat,doc.LidBond,doc.SpeakerTags,doc.RearFingers,doc.LidEnd,doc.ChargeTab0,doc.ChargeTab1]
    for i,o in enumerate(members):
        for p in members[i+1:]:
            if o.Shape.BoundBox.intersect(p.Shape.BoundBox):assert o.Shape.common(p.Shape).Volume<.0001,(o.Name,p.Name)
    for i in range(3):
        chip=sensors.Shape.Solids[i];assert chip.distToShape(full)[0]<1e-6
    for obj in [glue,seat,doc.ChargeTab0,doc.ChargeTab1,doc.SpeakerTags,doc.RearFingers,doc.LidEnd]:
        assert full.distToShape(obj.Shape)[0]<1e-6,obj.Name
    assert glue.Shape.distToShape(stiff.Shape)[0]<1e-6
    clearance=stiff.Shape.distToShape(doc.Tray.Shape)[0]
    assert clearance>.35
    for x,y in [(-2.70,0),(2.70,0),(0,2.40)]:
        ray=cylinder(.4,1.27,(x,y,-.83))
        for s in [seat.Shape,full,doc.Back.Shape,doc.Baffle.Shape]:assert ray.common(s).Volume<1e-6
    # Open showing the complete rear module so the same FPC is immediately selectable.
    for o in doc.RootObjects:
        if o!=lid and hasattr(o,'Visibility'):o.Visibility=False
    lid.Visibility=True
    for o in lid.Group:o.Visibility=True
    for k in ['Back','Baffle','LidBond','PpgMount','CoverScrew0','CoverScrew1','CoverScrew2','CoverScrew3']:
        doc.getObject(k).Visibility=False
    import FreeCADGui as Gui
    Gui.activeDocument().activeView().setCameraOrientation(App.Rotation(V(1,0,0),-20).multiply(App.Rotation(V(0,0,1),-12)))
    Gui.activeDocument().activeView().fitAll()
    doc.Label='WristFlow B · 后盖一体 FPC 与血氧局部补强'
    doc.recompute();fp=dest/'WristFlow-B-Rear-FPC.FCStd';doc.saveAs(str(fp));App.closeDocument(doc.Name)
    d=App.openDocument(str(fp));assert not d.getObject('PpgBoard') and not d.getObject('ChargeBranch')
    for key in ['LidFlexNew','Open6LidFlexNew','PpgStiffener','PpgStiffAdhesive','PpgMount']:
        sh=d.getObject(key).Shape;assert sh.isValid() and len(sh.Solids)==1,key
    report={'file':fp.name,'single_continuous_FPC':True,'FPC_thickness_mm':FPC,
        'PPG_stiffener_xy_mm':[10.8,8.5],'PPG_stiffener_thickness_mm':.30,'stiffener_adhesive_mm':.05,
        'PPG_stiffener_to_tray_mm':round(clearance,3),'tested_opening_mm':[0,1.5,3,4.5,6],
        'working_tail_length_mm':round(length,3),'prototype_only':True}
    (dest/'fpc-check.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
    App.closeDocument(d.Name);(dest/'reopened.ok').write_text('ok')
