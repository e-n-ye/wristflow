"""Review a 10 x 3 mm aperture in the published B assembly."""
import hashlib
import json
import sys
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / '2026-11-21' / 'B-integrated'
sys.path.insert(0, str(SOURCE.parents[1]))
from workspace import WORKSPACE
sys.path.insert(0,str(SOURCE))
from assembly import App, Part, V, B, F, box, rounded, cylinder, down_prisms
from checks import test
from space import calculate
import FreeCADGui as Gui

OLD_X, Y, R = -.455, 9.99, .5
NEW_W, NEW_H = 10., 3.
NEW_X = OLD_X-7.+NEW_W/2


def slot(width, height, x, bottom, thick):
    return rounded(width,height,thick,R,x,Y,bottom)


def projection_outline(board):
    face=max((f for f in board.Faces if abs(f.BoundBox.ZMin-F)<1.e-6),key=lambda f:f.Area)
    return [[(round(p.x,4),round(p.y,4)) for p in wire.discretize(Deflection=.003)]
            for wire in [face.OuterWire]+[w for w in face.Wires if not w.isSame(face.OuterWire)]]


def run(dest):
    dest=Path(dest)
    width,height,center=NEW_W,NEW_H,NEW_X
    model_dir=WORKSPACE/'2026-11-21/B-integrated'
    source=model_dir/'WristFlow-B-Integrated.FCStd'
    prior=json.loads((model_dir/'geometry.json').read_text(encoding='utf8'))
    assert hashlib.sha256(source.read_bytes()).hexdigest()==prior['files'][source.name]['sha256']
    doc=App.openDocument(str(source))
    group_ids='Fixed Circuit Fasteners Cell Mated Lid Tools Jig Meter Links FreeHarness Display Band'.split()
    groups={obj.Name:key for key in group_ids for obj in doc.getObject(key).Group}
    shapes={k:doc.getObject(k).Shape for k in groups}
    old=shapes['Board']
    blank=old.fuse(slot(14,3,OLD_X,B,F-B)).removeSplitter()
    assert len(blank.Solids)==1
    def recut(width,height,x):
        return blank.cut(slot(width,height,x,B-.4,F-B+.8)).removeSplitter()
    trial={}
    for label,test_w,test_h,x in [('original',14.,3.,OLD_X),('two_sided',12.,3.,OLD_X),
             ('right_only',12.,3.,OLD_X-1),('narrow',12.,2.5,OLD_X-1),('shorter',11.5,3.,OLD_X-1.25)]:
        pcb=recut(test_w,test_h,x)
        trial[label]={'dimensions':[test_w,test_h,R],'center':[x,Y]}
        for k in ['Crossing','FeedSpace']:
            target=shapes[k]
            trial[label][k]={'interference_mm3':target.common(pcb).Volume,'distance_mm':target.distToShape(pcb)[0]}
    def probe(angle):
        head=box(3.65,7.8,.75,0,0,-.375)
        head.rotate(V(),V(0,0,1),90)
        head.rotate(V(),V(1,0,0),angle)
        head.translate(V(NEW_X,Y,(B+F)/2))
        return head
    def passage(head):
        bb=head.BoundBox
        return box(bb.XLength+.20,bb.YLength+.20,18,NEW_X,Y,-5.65)
    new=recut(width,height,center)
    compact={}
    for short_side in [2.,2.3,2.5,2.8,3.]:
        candidate=recut(NEW_W,short_side,NEW_X)
        angles={}
        for angle in (65,70,75,80):
            head=probe(angle)
            guide=passage(head)
            angles[angle]={'projected_short_mm':head.BoundBox.YLength,
                           'guide_clearance_mm':guide.distToShape(candidate)[0],
                           'guide_interference_mm3':guide.common(candidate).Volume}
        compact[short_side]={'angles_deg':angles,'fold_gap_mm':candidate.distToShape(shapes['Crossing'])[0]}
    held=probe(65)
    guide=passage(held)
    assert abs(new.distToShape(shapes['Crossing'])[0]-trial['original']['Crossing']['distance_mm'])<1.e-6
    assert abs(new.distToShape(guide)[0]-trial['original']['FeedSpace']['distance_mm'])<1.e-6
    shapes['FeedSpace']=guide
    assert compact[NEW_H]['angles_deg'][70]['guide_clearance_mm']>.17
    for k in ['Crossing','FeedSpace']:
        old_dist=trial['original'][k]['distance_mm']
        assert old_dist>0 and abs(trial['right_only'][k]['distance_mm']-old_dist)<1.e-6
    assert trial['two_sided']['Crossing']['interference_mm3']>.001
    assert trial['narrow']['FeedSpace']['distance_mm']<.05
    assert abs((new.Volume-old.Volume)/(F-B)-(14*3-width*height))<1.e-5

    # Replace each marked relief virtually, then drop the added material towards the rear.
    stock=rounded(36.8,43.2,F-B,4.6,z=B)
    reliefs=[('upper_round',cylinder(1.66,2,(19,14,B-.5)),'Frame'),
             ('centre_square',box(2.3,2.35,2,18.35,0,B-.5),'CrownShaft'),
             ('lower_round',cylinder(1.66,2,(19,-14,B-.5)),'Frame'),
             ('bottom_square',box(4.05,3,2,-12.50,-21.55,B-.5),'Frame'),
             ('upper_square',box(3.25,2.15,2,18.17,11.06,B-.5),'Frame')]
    controls={}
    for k,tool,obstacle in reliefs:
        fill=stock.common(tool).cut(new)
        fixed=shapes[obstacle]
        closed=fill.common(fixed).Volume
        released=down_prisms(fill,20).common(fixed).Volume
        assert closed<.00001 and released>.005,(k,closed,released)
        controls[k]={'obstacle':obstacle,'installed_mm3':closed,'extraction_sweep_mm3':released}

    shapes['Board']=new
    results=test(shapes,groups,held)
    assert not results['penetrations_mm3'],results['penetrations_mm3']
    stats,*_=calculate(shapes,groups)
    delta={side:[dict(height_mm=n['height_mm'],added_mm2=n['area_mm2']-p['area_mm2'])
                 for n,p in zip(rows,prior['heights'][side])] for side,rows in stats.items()}
    doc.Board.Shape=new
    doc.Board.Label='B 主板 · 10 × 3 mm 穿线窗试装'
    doc.Board.Basis='R0.50，X=−7.455…+2.545，Y=8.490…11.490。保留照片红圈银色折返端原孔边，4 mm 全从黑色端收回。黑色部位的高度与装拆待实物复核。'
    doc.FeedSpace.Shape=guide
    doc.FeedSpace.Label='穿头通道 · 在新窗中居中，长边顺窗，侧倾 65°'
    doc.FeedSpace.Basis='中心 X=−2.455、Y=9.990。补强头的名义包络 7.80 × 3.65 × 0.75，通道在投影每边另加 0.10 mm。带线整体仍待试穿。'
    section=new.common(Part.makeBox(.45,49,20,V(-4.225,-24.5,-2)))
    assert abs(section.Volume-doc.CutBoard.Shape.Volume)<1.e-6
    doc.CutBoard.Shape=section
    doc.Label=f'WristFlow B · {width:g} × {height:g} mm 窗试装'
    doc.recompute()
    tag='10x3'
    target=dest/f'WristFlow-B-{tag}.FCStd'
    doc.saveAs(str(target))
    App.closeDocument(doc.Name)
    again=App.openDocument(str(target))
    pcb=again.Board.Shape
    assert pcb.isValid() and len(pcb.Solids)==1 and abs(pcb.Volume-new.Volume)<1.e-6
    for name in shapes:
        assert again.getObject(name).Shape.isValid(),name
    step=dest/f'PCB-{tag}.step'
    pcb.exportStep(str(step))
    imported=Part.read(str(step))
    assert imported.isValid() and len(imported.Solids)==1 and imported.cut(pcb).Volume+pcb.cut(imported).Volume<1.e-6
    result={'board_mm2':pcb.Volume/(F-B),'cn1_xy':results['cn1_xy'],'head_projection_mm':results['head_projected_size_mm'],
            'head_nominal_xyz_mm':[7.8,3.65,.75],'angle_deg':65,'checked_corridor_deg':[65],
            'slot_xy':[center,Y],'width_height_radius':[width,height,R],'short_side_study':compact,
            'study_geometry_mm':[NEW_W,NEW_X],'trial':trial,'notch_controls':controls,
            'candidate_fold_gap_mm':new.distToShape(shapes['Crossing'])[0],'candidate_guide_gap_mm':new.distToShape(guide)[0],
            'checks':results['checks'],'heights':stats,'delta':delta,'outline_mm':projection_outline(pcb),
            'reopened_valid':True,'units':'mm','freecad':App.Version()[:3],
            'basis':'原装配中屏尾和扣座位置固定。补强头为估计包络。装配检验为代表姿态及直线穿入通道，未验证带线连续翻转。'}
    (dest/'slot-check.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf8')
    App.closeDocument(again.Name)
    (dest/'reopened.ok').write_text('ok')
