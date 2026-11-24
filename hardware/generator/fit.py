from place import *
import FreeCAD as App,Part
from workspace import CAD, OUTPUT
V=App.Vector
root=CAD
out=OUTPUT
d=App.openDocument(str(root/'WristFlow-B-Review-R2.FCStd'))
def face(poly,z):
    def wire(seq):return Part.makePolygon([V(x,y,z) for x,y in seq.coords])
    return Part.Face([wire(poly.exterior)]+[wire(w) for w in poly.interiors])
extra=Part.makeCompound([face(g,11.3).extrude(V(0,0,.8)) for g in tab_shapes])
baseline=Part.read(str(root/'WristFlow-B-R2-PCB.step'))
board=baseline.fuse(extra).removeSplitter()
assert board.isValid() and len(board.Solids)==1
board.exportStep(str(out/'WristFlow-B-R2-local-lands.step'))
back=Part.read(str(out/'WristFlow-B-R2-local-lands.step'))
assert abs(back.Volume-board.Volume)<.001
obstacle_names=['Frame','Motor','MotorSeat','LugLongL','LugLongR','CrownBridge','Daughter','Retainer','ParkedMotorWire0','ParkedMotorWire1']
travel=Part.makeCompound([face(g,12.1).extrude(V(0,0,-16.8)) for g in tab_shapes])
def distvol(s,o):
    vol=s.common(o).Volume if s.BoundBox.intersect(o.BoundBox) else 0
    return [round(s.distToShape(o)[0],5),round(vol,6)]
tabcheck={k:distvol(travel,d.getObject(k).Shape) for k in obstacle_names}
assert all(v[1]<.00001 for v in tabcheck.values()),tabcheck
antenna=box(-15.5,-22.2,2.5,-16.1)
res={'step_area_mm2':round(back.Volume/.8,5),'step_area_delta':round((board.Volume-baseline.Volume)/.8,5),
     'antenna_board_overlap_mm2':boardshape.intersection(antenna).area,'tab_to_antenna_xy_mm':added.distance(antenna),
     'tab_removal_16mm_distance_overlap':tabcheck,'nominal_body_collisions':[],'battery_side':[],'motion_clashes':[]}
installed_names=['Frame','Motor','MotorSeat','CrownBridge','Daughter','Retainer','Clamp1','Clamp2','Clamp3',
                 'Battery','Separator','SurfaceNTC','Tray','Speaker','Back','FrontRun','Crossing','RearRun','Stiffener','LidFlexNew',
                 'MotorWire0','MotorWire1','Lead0','Lead1','Lead2','NTCReturn','ALSPipe','ALSBaffle','Screen']
special={'J1':{'Lead0','Lead1','Lead2'},'J201':{'Stiffener','RearRun'},'MT1':{'MotorWire0','MotorWire1'},'MT2':{'MotorWire0','MotorWire1'}}
for p in parts.values():
    if p['board']!='main':continue
    ref=p['ref'];f=p['side']=='F'
    # The whole module is not 3.1 mm high over its antenna; use the R2 two-height solid.
    if ref=='U1':solid=d.ModuleParts.Shape.fuse(d.ModuleSubstrate.Shape)
    else:
        solid=face(p['physical'],12.1 if f else 11.3).extrude(V(0,0,p['height']*(1 if f else -1)))
    for k in installed_names:
        if k in special.get(ref,set()):continue
        o=d.getObject(k).Shape
        if solid.BoundBox.intersect(o.BoundBox):
            v=solid.common(o).Volume
            if v>.00001:res['nominal_body_collisions'].append([ref,k,round(v,5)])
    if not f:
        sep=d.Separator.Shape;ztest=solid.translated(V(0,0,-1.4))
        projection=box(sep.BoundBox.XMin,sep.BoundBox.YMin,sep.BoundBox.XMax,sep.BoundBox.YMax)
        if p['physical'].intersection(projection).area>.0001:
            crit=min((distvol(ztest,d.getObject(k).Shape)[0],k) for k in ['Separator','SurfaceNTC','Tray','Battery'])
            res['battery_side'].append([ref,round(p['height'],3),round(3.55-p['height'],3),round(2.15-p['height'],3),round(1.75-p['height'],3),*crit])
    if ref not in ['U1','J1','J201','J2','U16','MT1','MT2'] and not ref.startswith('K'):
        sweep=face(p['physical'],solid.BoundBox.ZMax).extrude(V(0,0,-16-solid.BoundBox.ZLength))
        for k in obstacle_names:
            o=d.getObject(k).Shape
            if sweep.BoundBox.intersect(o.BoundBox):
                v=sweep.common(o).Volume
                if v>.00001:res['motion_clashes'].append([ref,k,round(v,6)])
chip=Part.makeBox(5.6,3.3,1.55,V(-2.8,-1.65,-.53))
res['optical_chip_R2']={k:distvol(chip,d.getObject(k).Shape) for k in ['Baffle','PpgMount','Optic0','Optic1','Optic2']}
res['optical_bypass_pockets_mm3']={}
for ref in ('C301','C302','C303','C304'):
    p=parts[ref]
    body=face(p['physical'],1.02).extrude(V(0,0,-p['height']))
    res['optical_bypass_pockets_mm3'][ref]={k:round(body.common(d.getObject(k).Shape).Volume,5) for k in ['Baffle','PpgMount']}
res['zif_slot_to_inserted_flex_R2']=distvol(d.RearSocket.Shape,d.LidFlexNew.Shape)
res['mic_to_frame_mm']=round(face(parts['U9']['physical'],12.1).extrude(V(0,0,1.05)).distToShape(d.Frame.Shape)[0],3)
mag=face(parts['U14']['physical'],12.1).extrude(V(0,0,parts['U14']['height']))
res['magnetometer_body_distance_mm']={k:round(mag.distToShape(d.getObject(k).Shape)[0],3) for k in ['Motor','Speaker','Contact0','Contact1']}
(out/'mechanical-readback.json').write_text(json.dumps(res,ensure_ascii=False,indent=2),encoding='utf8')
print('delta',res['step_area_delta'],'sweep',tabcheck,'R2 collisions',res['nominal_body_collisions'],'motion',res['motion_clashes'],'battery min',sorted(res['battery_side'],key=lambda i:i[-2])[:3])
