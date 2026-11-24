"""Mechanical comparison for the web between the motor opening and the 10 x 3 screen slot."""
from pathlib import Path
import hashlib
import json
import sys

SCRIPTS=Path(__file__).resolve().parent
sys.path.insert(0,str(SCRIPTS.parents[1]))
from workspace import study_dir
HERE=study_dir(__file__)
sys.path.insert(0,str(SCRIPTS.parents[1]/'2026-11-21/B-integrated'))
from assembly import App,Part,V,B,F,box,rounded,cylinder,turned,HOLES,down_prisms
from checks import test
from layout_space import shadow
import FreeCADGui as Gui

BASE=HERE.parent/'B-slot/fold-end-fixed'
SOURCE=BASE/'WristFlow-B-10x3.FCStd'
MOTOR_X,MOTOR_Y=-13.35,16.55
CUT_R=5.45

def outline(s):
    z=s.BoundBox.ZMax
    face=max((f for f in s.Faces if abs(f.BoundBox.ZMin-z)<1.e-6),key=lambda f:f.Area)
    return face,[face.OuterWire]+[w for w in face.Wires if not w.isSame(face.OuterWire)]

def board_data(s):
    face,wires=outline(s)
    aperture=next(w for w in wires if abs(w.BoundBox.XLength-10.)<1.e-5 and abs(w.BoundBox.YLength-3.)<1.e-5)
    distance,pairs,_=aperture.distToShape(face.OuterWire)
    def point(p):return [round(p.x,5),round(p.y,5)]
    return {'web_mm':distance,'witness_xy':[point(p) for p in pairs[0]],
            'area_mm2':face.Area,'loops_xy':[[point(p) for p in w.discretize(Deflection=.008)] for w in wires]}

def run(dest):
    dest=Path(dest)
    doc=App.openDocument(str(SOURCE))
    names='Fixed Circuit Fasteners Cell Mated Lid Tools Jig Meter Links FreeHarness Display Band'.split()
    groups={o.Name:key for key in names for o in doc.getObject(key).Group}
    s={k:doc.getObject(k).Shape for k in groups}
    board=s['Board']
    stock=rounded(36.8,43.2,F-B,4.6,z=B)
    old_cut=box(13.2,14.,2.,-14.3,16.925,B-.5)
    rf_cut=box(11.,18.34,2.,-16.60,-8.30,B-.5)
    slot=rounded(10.,3.,F-B,.5,-2.455,9.99,B)
    # Remove only metal that belongs to named cuts. Restore the underlying holes
    # and case reliefs explicitly where their cutters historically overlapped.
    def recut(openings_to_refill,new_openings):
        new=board
        for opening in openings_to_refill:
            new=new.fuse(stock.common(opening))
        cutters=list(new_openings)+[slot]
        cutters.extend(cylinder(1.66,2,(x,y,B-.5)) for x in (-19,19) for y in (-14,14))
        cutters.extend(cylinder(1.24,2,(x,y,B-.5)) for x,y in HOLES)
        cutters.extend([box(2.3,2.35,2,18.35,0,B-.5),box(4.05,3,2,-12.5,-21.55,B-.5),box(3.25,2.15,2,18.17,11.06,B-.5)])
        for solid in cutters:new=new.cut(solid)
        new=new.removeSplitter()
        assert new.isValid() and len(new.Solids)==1
        return new
    base=board_data(board)
    # Left/right exchange as seen from the battery: reverse the module by a
    # physical 180-degree rotation, with its centre moving to the opposite X.
    swap_cut=box(13.2,14.,2.,14.3,16.925,B-.5)
    swap_board=recut([old_cut,rf_cut],[swap_cut,box(11.,18.34,2.,16.60,-8.30,B-.5)])
    swap_motor=s['Motor'].translated(V(26,0,0))
    swap_module={k:turned(s[k],(-3.30,-8.30,0),180,(6.60,0,0)) for k in ['ModuleSubstrate','ModuleParts','Antenna']}
    swap=board_data(swap_board)
    swap['window_to_motor_notch_mm']=slot.distToShape(swap_cut)[0]
    swap['motor_solid_overlaps_mm3']={k:swap_motor.common(s[k]).Volume for k in ['BatHeader','BatPlug','KeyBody','PullAccess','Mguide','Fixture']}
    swap['radiator_cell_overlap_mm2']=shadow(swap_module['Antenna']).intersection(shadow(s['Battery'])).area
    swap['unsupported_pads']=[]
    for k in s:
        if k.startswith(('LPad','DBG')) or (k.startswith('I') and k[1:].isdigit()):
            pad=s[k].translated(V(0,0,.05))
            if pad.cut(swap_board).Volume>.00001:swap['unsupported_pads'].append(k)
    assert all(v>.001 for v in swap['motor_solid_overlaps_mm3'].values())

    # Quarter-circle tangencies to the top and side exits keep the original
    # corner open. The narrow support tongue runs outwards towards the frame.
    x,y=MOTOR_X,MOTOR_Y
    z=B-.5
    cut=cylinder(CUT_R,2,(x,y,z)).fuse(Part.makeBox(x+30,30-y+CUT_R,2,V(-30,y-CUT_R,z)))
    cut=cut.fuse(Part.makeBox(x+30+CUT_R,30-y,2,V(-30,y,z))).removeSplitter()
    new_board=recut([old_cut],[cut,rf_cut])
    motor=s['Motor'].translated(V(x+13,y-15.5,0))
    seat_z=s['MotorSeat'].BoundBox.ZMin
    thick=s['MotorSeat'].BoundBox.ZLength
    seat=cylinder(5.20,thick,(x,y,seat_z)).fuse(Part.makeBox(x+20.20,4.4,thick,V(-20.20,y-2.2,seat_z)))
    outer=rounded(41.1,47.5,16,6.8,z=0)
    seat=seat.common(outer).cut(s['Frame']).removeSplitter()
    assert seat.isValid() and len(seat.Solids)==1
    local=board_data(new_board)
    local['xy_center']=[x,y]
    local['shift_xy_mm']=[x+13,y-15.5]
    local['slot_to_motor_cut_mm']=slot.distToShape(cut)[0]
    local['motor_to_frame_mm']=motor.distToShape(s['Frame'])[0]
    # OCC solid common omits boundary-only faces. Intersect the boundary shells directly.
    local['seat_to_frame_contact_mm2']=sum(f.Area for f in seat.Shells[0].common(s['Frame'].Shells[0]).Faces)
    local['motor_seating_area_mm2']=sum(f.Area for f in motor.Shells[0].common(seat.Shells[0]).Faces)
    local['board_extraction_to_seat_mm']=down_prisms(new_board,20).distToShape(seat)[0]
    local['board_extraction_to_motor_mm']=down_prisms(new_board,20).distToShape(motor)[0]
    assert local['motor_seating_area_mm2']>78 and local['seat_to_frame_contact_mm2']>1
    assert local['board_extraction_to_seat_mm']>.249 and local['board_extraction_to_motor_mm']>.449
    assert abs(local['web_mm']-local['slot_to_motor_cut_mm'])<1.e-6
    s.update(Board=new_board,Motor=motor,MotorSeat=seat)
    for k,part in s.items():
        if k!='MotorSeat' and part.BoundBox.intersect(seat.BoundBox):
            assert seat.common(part).Volume<.0005,k
    head=box(3.65,7.8,.75,0,0,-.375)
    head.rotate(V(),V(0,0,1),90);head.rotate(V(),V(1,0,0),65)
    head.translate(V(-2.455,9.99,(B+F)/2))
    checks=test(s,groups,head)
    assert not checks['penetrations_mm3'],checks['penetrations_mm3']
    local['geometric_checks']=checks['checks']
    local['threshold_mm3']=checks['threshold_mm3']
    local['rf']=checks['rf']
    local['module_to_fold_mm']=checks['module_to_folded_tail_mm']
    take={'baseline':base,'exchange':swap,'local':local,'freecad':App.Version()[:3],
          'input_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
          'scope':'名义几何与装拆检查；6 层叠构、材料、公差、导线焊耳、装配载荷和疲劳强度尚未验证。'}
    for k in ['Board','Motor','MotorSeat']:
        doc.getObject(k).Shape=s[k]
        section=doc.getObject('Cut'+k)
        if section is not None:
            section.Shape=s[k].common(Part.makeBox(.45,49,20,V(-4.225,-24.5,-2)))
    doc.Board.Label='B 主板 · 10 × 3 原折返窗 / 加宽电机侧板桥'
    doc.Board.Basis='板厚 0.80 mm。R5.45 电机退板缺口与上下直线相切，屏窗至外缘名义净宽 %.3f mm。六层可靠性待样板验证。'%local['web_mm']
    doc.Motor.Basis='原 Ø10 mm 电机 · X −13.35，Y +16.55，原安装高度'
    doc.MotorSeat.Basis='R5.20、厚0.55 mm 圆形支承，外侧 4.4 mm 宽接中框。焊耳、走线、固定及材料需实配。'
    doc.Label='WristFlow B · 电机侧圆弧避让评审'
    doc.recompute()
    native=dest/'WristFlow-B-Motor-Web.FCStd';doc.saveAs(str(native));App.closeDocument(doc.Name)
    again=App.openDocument(str(native))
    for k in ['Board','Motor','MotorSeat']:
        solid=again.getObject(k).Shape
        assert solid.isValid() and abs(solid.Volume-s[k].Volume)<1.e-6
    step=dest/'PCB-B-Motor-Web.step';again.Board.Shape.exportStep(str(step))
    imported=Part.read(str(step))
    assert imported.isValid() and abs(board_data(imported)['web_mm']-local['web_mm'])<1.e-5
    App.closeDocument(again.Name)
    (dest/'comparison.json').write_text(json.dumps(take,indent=2,ensure_ascii=False),encoding='utf8')
    (dest/'reopened.ok').write_text('ok')
