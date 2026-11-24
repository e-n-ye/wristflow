"""Review the through-board fold in the B assembly, using original artwork datums in config.json."""
from pathlib import Path
import json
import sys
from math import pi,tan,radians
from zipfile import ZipFile
import FreeCAD as A
import FreeCADGui as G
import Part
V=A.Vector
HERE=Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
from workspace import WORKSPACE

def box(w,h,zsize,x,y,z):
    return Part.makeBox(w,h,zsize,V(x-w/2,y-h/2,z))

def rounded_slot(w,h,r,x,y,z,depth):
    b=box(w,h,depth,x,y,z)
    return b.makeFillet(r,[e for e in b.Edges if e.BoundBox.ZLength>depth-.001])

def flat_artwork(cfg,z):
    with ZipFile(WORKSPACE / cfg['archive']) as zf:
        recs=[json.loads(l) for l in zf.read(cfg['pcb_member']).decode('utf8').splitlines() if l.strip()]
    placement=next(r for r in recs if r[:2]==['COMPONENT','e26'])
    aperture=next(r for r in recs if r[:2]==['FILL','e6'])
    assert placement[3]==2 and placement[6]==270 and aperture[7][0][0]=='R'
    assert abs(placement[4]*.0254-28.100)<.001 and abs(aperture[7][0][3]*.0254-14)<.001
    seq=next(r[6] for r in recs if r[:2]==['POLY','e654'])
    def v(x,y):return V(x*.0254-35.11/2,y*.0254-41.47/2,z)
    edges=[];last=v(*seq[:2]);i=2;op=''
    while i<len(seq):
        if isinstance(seq[i],str):op=seq[i];i+=1
        theta=seq[i] if op=='ARC' else None
        if theta is not None:i+=1
        end=v(*seq[i:i+2]);i+=2
        if theta is None:edge=Part.makeLine(last,end)
        else:
            diff=end-last
            mid=(end+last)*.5+V(diff.y,-diff.x,0)*(tan(radians(theta)/4)*.5)
            edge=Part.Arc(last,mid,end).toShape()
        edges.append(edge);last=end
    face=Part.Face(Part.Wire(edges));assert face.isValid() and face.Area>400
    return face

def fold(x,y,w,hi,lo,t,sgn):
    r=(hi-lo)/2;z=(hi+lo)/2
    def p(d,h):return V(x-w/2,y+sgn*d,z+h)
    edges=[Part.Arc(p(0,r+t/2),p(r+t/2,0),p(0,-r-t/2)).toShape(),
           Part.makeLine(p(0,-r-t/2),p(0,-r+t/2)),
           Part.Arc(p(0,-r+t/2),p(r-t/2,0),p(0,r-t/2)).toShape(),
           Part.makeLine(p(0,r-t/2),p(0,r+t/2))]
    return Part.Face(Part.Wire(edges)).extrude(V(w,0,0))

def run(dest):
    cfg=json.loads((HERE/'config.json').read_text(encoding='utf8'))
    G.activateWorkbench('PartWorkbench')
    src=A.openDocument(str(WORKSPACE/HERE.parent.name/'B-service-section/WristFlow-B-Open.FCStd'))
    board=src.Board.Shape.copy();old_area=board.Volume/board.BoundBox.ZLength
    F=board.BoundBox.ZMax;B=board.BoundBox.ZMin
    u,v=cfg['socket'];s,q=cfg['slot'];w,h=cfg['slot_size'];r=cfg['slot_radius']
    # Coordinates are defined in a rear photograph frame (right/up). Mirror X to view B from its rear.
    # Folded ribbon geometry is illustrative. The socket and opening implement the reference artwork.
    length,width=cfg['socket_length_width']
    zhi=F+.72;zlo=B-cfg['mated_budget']-.06
    def point(x,y):return (-x,y)
    cx,cy=point(u,v);sx,sy=point(s,q)
    # Equal arc-length flat-to-cylindrical mapping. The original slit lies at the folded symmetry axis.
    face=flat_artwork(cfg,zhi-.06)
    R=(zhi-zlo)/2; fy=sy-pi*R/2; yf=sy+pi*R/2
    fx=(436.0087+613.174)*.0254/2-35.11/2
    fw=(613.174-436.0087)*.0254
    sheet=face.extrude(V(0,0,.12))
    # Only the neck folds. The stiffened tip projects beside it into the same Y interval when flat.
    separated=sheet.cut(box(fw+.02,yf-fy,1,fx,sy,zhi-.5)).Solids
    assert len(separated)==2
    front,back=sorted(separated,key=lambda s:s.Volume,reverse=True)
    back.rotate(V(0,sy,(zhi+zlo)/2),V(1,0,0),180)
    bend=fold(fx,fy,fw,zhi,zlo,.12,1)
    assert abs((front.Volume+back.Volume+bend.Volume)-sheet.Volume)<.0001,(front.Volume,back.Volume,bend.Volume,sheet.Volume)
    assert front.distToShape(bend)[0]<.00001 and back.distToShape(bend)[0]<.00001
    socket=box(width,length,.7,cx,cy,B-.7).cut(box(width-.65,length-.65,.48,cx,cy,B-.71))
    head=box(3.50,length+.60,.14,cx,cy,zlo-.20)
    mate=box(width-.75,length-.75,.34,cx,cy,B-.70)
    slot=rounded_slot(w,h,r,sx,sy,B-.1,.8+.2)
    base={'Slot':slot,'FrontRun':front,'Crossing':bend,'RearRun':back,'CN1':socket,'FlexPlug':mate,'Stiffener':head}
    groups=['Fixed','Circuit','Cell','Fasteners','Lid','Mated','Links']
    omit={'Board','ScreenSocket','ScreenFlex','ScreenTail'}
    obstacles={o.Name:o.Shape.copy() for g in groups for o in src.getObject(g).Group if o.Name not in omit}
    def rotate(s,angle):
        out=s.copy()
        if angle:out.rotate(V(),V(0,0,1),angle)
        return out
    def inspect(items):
        new=board.cut(items['Slot']).removeSplitter()
        hits={};support=items['CN1'].translated(V(0,0,.7))
        for key,shape in items.items():
            if key=='Slot':continue
            for k,s in [('Board',new),*obstacles.items()]:
                if s.BoundBox.intersect(shape.BoundBox):
                    inter=s.common(shape).Volume
                    if inter>.0001:hits[key+'/'+k]=round(inter,5)
        # Checking an unobstructed way through the full aperture is separate from the installed ribbon clash.
        slot_prism=items['Slot'].translated(V(0,0,-.9))
        passage={k:round(s.common(slot_prism).Volume,4) for k,s in obstacles.items() if s.BoundBox.intersect(slot_prism.BoundBox) and s.common(slot_prism).Volume>.0001}
        return new,{'new_board_area_mm2':round(new.Volume/.8,3),'slot_removed_mm2':round((board.Volume-new.Volume)/.8,3),'slot_from_back_first_mm':passage,
                    'socket_foot_supported_fraction':round(support.common(new).Volume/support.Volume,3),
                    'nominal_collisions_mm3':hits,'separate_notes':'Z heights, strip widths, bend radius, support/tool paths and alignment relative to the B outline are provisional.'}
    trials={}
    for a in (0,180):
        items={k:rotate(s,a) for k,s in base.items()}
        new,data=inspect(items);trials[a]=(items,new,data)
    angle=cfg['display_pose'];parts,new,selected=trials[angle]
    doc=A.newDocument('B_Display');doc.Label='B 版屏尾过板与扣座 · 位置评审'
    def group(n,l):
        g=doc.addObject('App::DocumentObjectGroup',n);g.Label=l;return g
    model=group('Route','穿板、折返、扣座');ctx=group('Context','可选参照 · 原 B 结构');info=group('Review','坐标与干涉')
    def add(g,k,l,s,col,note=''):
        assert not s.isNull() and s.isValid(),k
        o=doc.addObject('Part::Feature',k);o.Label=l;o.Shape=s;g.addObject(o)
        o.addProperty('App::PropertyString','Basis','Evidence').Basis=note
        o.ViewObject.ShapeColor=col;o.ViewObject.LineWidth=1
        return o
    plate=add(model,'WindowedPCB','0.80 mm 主板与原板穿线窗候选位置',new,(.30,.50,.48),cfg['registration']);plate.ViewObject.Transparency=67
    color={'FrontRun':(.77,.45,.17),'Crossing':(.90,.55,.15),'RearRun':(.99,.72,.28),'CN1':(.18,.34,.49),'FlexPlug':(.59,.64,.70),'Stiffener':(.38,.25,.16)}
    labels={'FrontRun':'屏侧固定根部与去程 · 轮廓示意','Crossing':'长孔内翻到另一面 · 自然弯曲示意','RearRun':'电池侧回程 · 窄尾','CN1':'主板 CN1 竖向扣座 · 外形示意','FlexPlug':'屏尾插头 · 扣合示意','Stiffener':'屏尾端补强 · 示意'}
    for k,l in labels.items():add(model,k,l,parts[k],color[k],cfg['notes'])
    interference=Part.makeCompound([parts[k].common(obstacles['ModuleSubstrate']) for k in ['RearRun','Crossing']
                                    if parts[k].common(obstacles['ModuleSubstrate']).Volume>.0001])
    if not interference.isNull():
        add(info,'ModuleClash','与当前思澈模组基板重合的位置',interference,(.92,.19,.14),'红色为名义实体公共体积，可单独查看。')
    for k in ['Frame','Screen','Motor','Battery','Tray','ModuleParts','ModuleSubstrate','Antenna','Clamp1','Clamp2','Clamp3','BatHeader','PpgBoard']:
        o=add(ctx,'Ref'+k,src.getObject(k).Label,obstacles[k],(.58,.63,.65));o.ViewObject.Transparency=70
    meta=doc.addObject('App::FeaturePython','Basis');info.addObject(meta);meta.Label='原板资料、示意尺寸与限制'
    for k,val in {'Reference':cfg['reference'],'Registration':cfg['registration'],'Flex':cfg['notes'],'Clash':json.dumps(selected,ensure_ascii=False)}.items():
        meta.addProperty('App::PropertyString',k,'Review');setattr(meta,k,val)
    ctx.ViewObject.Visibility=False
    for o in ctx.Group:o.ViewObject.Visibility=False
    A.closeDocument(src.Name);A.setActiveDocument(doc.Name);doc.recompute()
    view=G.getDocument(doc.Name).activeView();view.setCameraType('Orthographic');view.setAnimationEnabled(False)
    def render(name,normal):
        view.setCameraOrientation(A.Rotation(V(1,0,0),V(0,1,0),V(*normal),'ZYX').Q);view.fitAll();G.updateGui()
        view.saveImage(str(dest/(name+'.png')),1100,860,'White')
    render('face',(.25,-.32,-1))
    render('screen',(.3,-.35,1))
    render('profile',(1,-.85,-.35))
    for k in ['RearRun','FlexPlug','Stiffener']:doc.getObject(k).ViewObject.Visibility=False
    render('socket',(.25,-.32,-1))
    for k in ['RearRun','FlexPlug','Stiffener']:doc.getObject(k).ViewObject.Visibility=True
    render('face',(.25,-.32,-1))
    file=dest/'WristFlow-B-Display-Route.FCStd';doc.recompute();doc.saveAs(str(file))
    vols={o.Name:o.Shape.Volume for o in doc.Objects if o.TypeId=='Part::Feature'}
    A.closeDocument(doc.Name);d=A.openDocument(str(file))
    for k,val in vols.items():assert d.getObject(k).Shape.isValid() and abs(val-d.getObject(k).Shape.Volume)<.0001
    assert selected['slot_removed_mm2']>0 and new.isValid()
    report={'input_area_mm2':old_area,'pose_shown_degrees':angle,'poses':{a:p[2] for a,p in trials.items()},'reopened_shapes':len(vols)}
    (dest/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    (dest/'reopened.ok').write_text('ok');A.closeDocument(d.Name)
