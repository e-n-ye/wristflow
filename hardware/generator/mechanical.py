"""Extract the STEP outline and R2 mechanical reservations in the assembly's XY frame."""
import json
import FreeCAD as App
import Part
from shapely.geometry import Polygon, mapping
from shapely.ops import unary_union
from workspace import CAD, CAD_INPUTS

source = CAD
doc = App.openDocument(str(source / "WristFlow-B-Review-R2.FCStd"))
board = Part.read(str(source / "WristFlow-B-R2-PCB.step"))
assert board.isValid() and len(board.Solids) == 1
assert abs(board.Volume - doc.Board.Shape.Volume) < 0.001
top = max((f for f in board.Faces if f.BoundBox.ZLength < 0.00001), key=lambda f: f.CenterOfMass.z)
def points(wire):
    return [(round(v.x, 6), round(v.y, 6)) for v in wire.discretize(Deflection=0.003)]
outline = Polygon(points(top.OuterWire), [points(w) for w in top.Wires if not w.isSame(top.OuterWire)])
assert abs(outline.area-top.Area) < .25 and outline.is_valid

def projection(solid):
    vertices, triangles = solid.tessellate(.035)
    faces = [Polygon([(vertices[i].x, vertices[i].y) for i in face]) for face in triangles]
    return unary_union([f for f in faces if f.area > .0000001]).buffer(.015)

groups = ['Fixed','Display','Cell','Mated','Fasteners','CrownAssembly','Lid','Tools']
obstacles = {o.Name:o.Shape for g in groups for o in doc.getObject(g).Group if hasattr(o,'Shape')}
obstacles.update({k:doc.getObject(k).Shape for k in ['Separator','MotorSeat','ParkedMotorWire0','ParkedMotorWire1']})
bb = {}
for o in doc.Circuit.Group + doc.CrownAssembly.Group + doc.Lid.Group:
    if hasattr(o,'Shape'):
        b = o.Shape.BoundBox
        bb[o.Name] = [b.XMin,b.YMin,b.ZMin,b.XMax,b.YMax,b.ZMax]
bands = {}
for face,z,direction in [('F',12.1,1),('B',11.3,-1)]:
    for h in [.1,.75,1.25,1.7,2.2,3.3]:
        lo,hi=sorted([z+direction*.005,z+direction*h])
        cutter=Part.makeBox(48,52,hi-lo,App.Vector(-24,-26,lo))
        shadows={}
        for name,s in obstacles.items():
            if s.BoundBox.ZMax <= lo or s.BoundBox.ZMin >= hi:continue
            piece=s.common(cutter)
            if piece.Volume>1e-6:shadows[name]=mapping(projection(piece))
        bands[face+str(h)]=shadows
result=dict(outline=mapping(outline),exact_area=top.Area,boxes=bb,bands=bands)
CAD_INPUTS.mkdir(parents=True, exist_ok=True)
with (CAD_INPUTS/'constraints.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream)
print('R2',top.Area,outline.area,len(top.Wires),list(bands))
