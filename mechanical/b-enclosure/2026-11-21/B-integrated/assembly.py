"""Rebuild the B mechanical studies around the native screen/adapter datums. Units mm."""
import json
import sys
from pathlib import Path
import FreeCAD as App
import Part
import shapely as sh
from shapely.geometry import box as rect

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
sys.path.insert(0, str(ROOT.parent))
from workspace import WORKSPACE
sys.path[:0]=[str(ROOT/'B-service-section'),str(ROOT/'B-board-space'),str(ROOT/'source')]
from target_model import V,box,rounded,cylinder
from service import ring,down_prisms
from layout_space import shadow,native

B=11.30
F=12.10
HOLES=[(9,-19.25),(-13.6,4),(15.5,8.4)]
SLOT=(-.455,9.99)
MODULE=(-3.30,-8.30)
TEN=[(-6.35+1.6*(i%5),13.60+1.16*(i//5)) for i in range(10)]
REAR_FLEX=['RearRun','FlexPlug','Stiffener']
RF_INNER=-11.10
FLAT_END_Y=-3.9
TRANSITION_END_Y=-1.9

def seat_front_film(film,screen):
    # The wide end follows the back of the glass. Blend only on the screen side;
    # the reference aperture, the bend in it and the short return keep their datums.
    def strip(a,b):
        return film.common(Part.makeBox(80,b-a,10,V(-40,a,10)))
    rise=screen.BoundBox.ZMin-film.BoundBox.ZMax
    flat=strip(-40,FLAT_END_Y).translated(V(0,0,rise))
    gradient=rise/(FLAT_END_Y-TRANSITION_END_Y)
    matrix=App.Matrix()
    matrix.A32=gradient
    matrix.A34=-gradient*TRANSITION_END_Y
    transition=strip(FLAT_END_Y,TRANSITION_END_Y).transformGeometry(matrix)
    neck=strip(TRANSITION_END_Y,40)
    out=flat.fuse(transition).fuse(neck).removeSplitter()
    assert len(out.Solids)==1 and out.isValid() and abs(out.Volume-film.Volume)<.002
    return out
def turned(s,pivot,angle,shift):
    q=s.copy();q.rotate(V(*pivot),V(0,0,1),angle);q.translate(V(*shift));return q
def comp(shapes):
    shapes=list(shapes);return Part.makeCompound(shapes)

def build():
    opened=[App.openDocument(str(p)) for p in [
        WORKSPACE/ROOT.name/'B-service-section/WristFlow-B-Open.FCStd',
        WORKSPACE/ROOT.name/'B-display-route/WristFlow-B-Display-Route.FCStd',
        WORKSPACE/ROOT.name/'WristFlow-B.FCStd']]
    old,route,original=opened
    shapes={}; labels={}; groups={}; colors={}; comments={}
    def put(k,s,g,l,c,note='沿用 B 装配的名义包络'):
        assert s.isValid() and not s.isNull() and s.Volume>1e-8,k
        shapes[k]=s;groups[k]=g;labels[k]=l;colors[k]=c;comments[k]=note
    def take(d,k,g):
        o=d.getObject(k);c=o.ViewObject.ShapeColor[:3] if o.ViewObject else (.5,.6,.65)
        put(k,o.Shape.copy(),g,o.Label,c,getattr(o,'DesignBasis',getattr(o,'Basis','沿用 B 装配的名义包络')))
    omit={'ScreenTail','ScreenFlex','ScreenSocket'}
    for g in ['Fixed','Circuit','Cell','Mated','Lid','Fasteners','Jig','Meter','Tools','Links','FreeHarness']:
        for o in old.getObject(g).Group:
            if o.Name not in omit:take(old,o.Name,g)
    for k in ['BandTailShort','BandTailLong','Buckle','BuckleTongue','Keeper']:
        take(original,k,'Band')
    for k in list(shapes):
        if k.startswith(('BandRoot','Bar','Release','Lug')):groups[k]='Band'
    for k in ['FrontRun','Crossing',*REAR_FLEX,'CN1']:
        take(route,k,'Circuit' if k=='CN1' else 'Display')
    shapes['FrontRun']=seat_front_film(shapes['FrontRun'],shapes['Screen'])
    labels['FrontRun']='屏背宽片随屏固定 · 窄尾向穿线窗过渡'
    comments['FrontRun']='实物确认宽片平贴。模型沿用 0.12 mm 名义片厚，过渡形状示意。原转接板可贴合至沿 Z 分开约 15 mm。'
    assert shapes['FrontRun'].distToShape(shapes['Crossing'])[0]<1e-5
    for k in ['ModuleParts','ModuleSubstrate','Antenna']:
        shapes[k]=turned(shapes[k],(-7.9,6.7,0),90,(MODULE[0]+7.9,MODULE[1]-6.7,0))
    for k in ['Motor','MotorSeat']:
        shapes[k]=turned(shapes[k],(12,-5,0),180,(-25,20.5,0))
    frame=original.MiddleFrame.Shape.copy()
    frame=frame.cut(ring(35.15,41.51,.17,5.03,32.82,39.14,3.78,12.88))
    pcb=rounded(36.8,43.2,.8,4.6,z=B)
    # Open the board's left edge to the entire new 18 x 6.1 radiator projection.
    pcb=pcb.cut(box(11,18.34,1.2,RF_INNER-5.5,MODULE[1],B-.2))
    pcb=pcb.cut(rounded(14,3,1.2,.5,*SLOT,z=B-.20))
    # The motor and its frame shelf release towards the rear through this board-edge scallop.
    # Let this notch exit both board edges, avoiding a cantilevered 0.53 mm strip at the far corner.
    pcb=pcb.cut(box(13.2,14.0,1.25,-14.3,16.925,B-.2))
    pcb=pcb.cut(box(2.30,2.35,1.3,18.35,0,B-.2)) # crown shaft when backing out
    front_hold=shadow(shapes['FrontRun']).buffer(.20)
    relief=native(front_hold,12.55,.5)
    for i,(x,y) in enumerate(HOLES,1):
        pilot=cylinder(.52,1.6,(x,y,B-.1))
        support=cylinder(1.12,1.58,(x,y,B)).fuse(cylinder(1.96,.70,(x,y,F)))
        if i==1:arm=box(3.1,22.2+y,.70,x,(-22.2+y)/2,F)
        else:
            end=19.05 if x>0 else -19.05
            arm=box(abs(end-x),3,.70,(end+x)/2,y,F)
        frame=frame.fuse(support.fuse(arm).cut(pilot).cut(relief))
        pcb=pcb.cut(cylinder(1.24,1.4,(x,y,B-.1)))
        before=[(9,-18.5),(-13.6,-10.8),(15.5,8.4)][i-1]
        shapes['Clamp'+str(i)]=shapes['Clamp'+str(i)].translated(V(x-before[0],y-before[1],0))
    support=box(14.6,3.5,.65,8.75,20.40,F).fuse(box(5.8,6.8,.65,9,17.35,F)).fuse(box(3.6,8.3,.65,15,17.15,F))
    # A second backing rib takes the cover contact spring reaction. It meets the positive-Y frame wall.
    support=support.fuse(box(8.4,2.5,.49,-3.15,14.18,F)).fuse(box(1.5,9.3,.49,-3.15,18.05,F))
    frame=frame.fuse(support.cut(relief))
    bz=shapes['Battery'].BoundBox.ZMin
    for i,(x,y) in enumerate([(-12.5,-20.77),(17.5,10.96)]):
        if i==0:
            shelf=box(3.45,2.2,1.05,x,-21.60,bz-.05)
            pcb=pcb.cut(box(4.05,3.0,1.4,x,-21.55,B-.2))
        else:
            shelf=box(2.30,1.45,1.05,18.05,y+.10,bz-.05)
            pcb=pcb.cut(box(3.25,2.15,1.4,18.17,y+.10,B-.2))
        frame=frame.fuse(shelf.cut(cylinder(.60,2.3,(x,y,bz-1))))
    for x in [-19,19]:
        for y in [-14,14]:pcb=pcb.cut(cylinder(1.66,1.4,(x,y,B-.2)))
    shapes['Board']=pcb.removeSplitter()
    shapes['Frame']=frame.fuse(box(1.8,4.8,1,19.65,15.8,6)).cut(box(4.9,4.2,1,20,16,3.20)).removeSplitter()
    outer=rounded(41.1,47.5,16,6.8,z=0)
    shapes['MotorSeat']=shapes['MotorSeat'].common(outer).cut(shapes['Frame']).removeSplitter()
    assert shapes['MotorSeat'].distToShape(shapes['Frame'])[0]<1e-5
    labels['Board']='B 主板 · 0.8 mm / 原屏穿线窗 / 左侧天线退让'
    # Back cover charge contacts and THROUGH holes match the provisional photographed 3.5 mm pitch.
    back=shapes['Back']
    for x in (-3.20,3.20):back=back.fuse(cylinder(1.16,.9,(x,-15,0)))
    for i,x in enumerate([-1.75,1.75]):
        back=back.cut(cylinder(1.13,1.25,(x,-15,-.10)))
        shapes['Contact'+str(i)]=cylinder(1.05,.89,(x,-15,0))
    shapes['Back']=back.removeSplitter()
    for i,(x,y) in enumerate(TEN):
        shapes['LPad'+str(i)]=cylinder(.40,.045,(x,y,B-.045))
        shapes['LFinger'+str(i)]=cylinder(.26,B-.045-10.32,(x,y,10.32))
    # Rear island: a shallow tray on two uprights. Rigid shell rises independently of cell and optical board.
    tower=rounded(7.9,2.68,.45,.40,-3.15,14.18,9.77)
    for x in [-6.25,-.10]:
        tower=tower.fuse(rounded(.80,2.18,8.88,.25,x,14.18,.90))
    shapes['LidTower']=tower.cut(box(2.58,.40,.60,-2,13.55,9.71)).removeSplitter()
    # Flex routes are insulating material envelopes. Leave the spring contact solder area at its top.
    strip=box(2.3,9.2,.10,-2.0,9.0,1.55)
    strip=strip.fuse(box(2.30,.13,8.67,-2,13.55,1.55)).fuse(rounded(7.60,2.38,.10,.28,-3.15,14.18,10.22))
    shapes['LidFlex']=strip.removeSplitter()
    assert shapes['LidFlex'].common(shapes['LidTower']).Volume<.00001
    for i in range(10):assert shapes['LFinger'+str(i)].distToShape(shapes['LidFlex'])[0]<.00001
    # Keep charge and speaker wiring in the back assembly. Individual insulation, adhesive and tracks still require selection.
    from service import wire
    charge=wire([(-1.75,-15,.88),(-1.75,-15,1.11),(-8.7,-14.8,1.11),(-8.7,6.2,1.11),(-2.0,12.7,1.40),(-2.0,13.52,1.55)],.09)
    charge=charge.fuse(wire([(1.75,-15,.88),(1.75,-15,1.11),(1.75,-16.00,1.11),(-8.39,-16,1.11),(-8.39,6.02,1.11),(-1.70,12.55,1.40),(-1.70,13.52,1.55)],.09))
    put('ChargeLeads',charge,'Lid','充电触点至后盖柔板 · 线路示意',(.70,.44,.22))
    sp=box(3.38,.85,.1,-.25,13.17,1.55)
    put('SpeakerFlex',sp,'Lid','喇叭至后盖柔板 · 出线端待核',(.70,.44,.22))
    # Workspace after disconnecting battery and removing the tray and lid. The head is fed before seating the board.
    put('CN1Access',box(5.2,8.4,16,10.545,5.135,-5.5),'Tools','CN1 解扣工具净空 · 电池托板已取下',(.64,.44,.76))
    held=box(3.65,7.8,.75,0,0,-.375)
    held.rotate(V(),V(0,0,1),90);held.rotate(V(),V(1,0,0),65)
    held.translate(V(0,9.99,(B+F)/2))
    # Conservative axis-aligned silhouette enclosing the tipped stiffener throughout straight feed.
    bb=held.BoundBox
    put('FeedSpace',box(bb.XLength+.20,bb.YLength+.20,18,0,9.99,-5.65),'Tools','补强头穿窗走廊 · 长轴顺孔，侧倾 65° 示意',(.60,.45,.76))
    for k in shapes:assert shapes[k].isValid() and shapes[k].Volume>1e-8,k
    for d in opened:App.closeDocument(d.Name)
    return shapes,groups,labels,colors,comments,held

if __name__=='__main__':
    build()
