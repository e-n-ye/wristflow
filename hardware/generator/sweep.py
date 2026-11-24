import FreeCAD as App, Part, json, math
from shapely.geometry import Polygon, mapping
from shapely.ops import unary_union
from workspace import CAD, CAD_INPUTS
root=CAD
d=App.openDocument(str(root/'WristFlow-B-Review-R2.FCStd'))
shapes={k:d.getObject(k).Shape for k in ['Frame','Motor','MotorSeat','LugLongL','LugLongR','CrownBridge','Daughter','Retainer','ParkedMotorWire0','ParkedMotorWire1']}
shadows={}
for side,h,lo,hi in [('B',0,11.3-16-2,11.3)]+[('F',h,12.1-16,12.1+h) for h in [.75,1.25,1.7,3.3]]:
    polys=[]
    cut=Part.makeBox(45,50,hi-lo,App.Vector(-22.5,-25,lo))
    for s in shapes.values():
        piece=s.common(cut)
        if piece.Volume<1e-6:continue
        verts,tris=piece.tessellate(.025)
        faces=[Polygon([(verts[i].x,verts[i].y) for i in tr]) for tr in tris]
        polys+= [f for f in faces if f.area>1e-8]
    from shapely import union_all
    shadows[side+str(h)]=mapping(union_all(polys,grid_size=.0001).buffer(.02))
CAD_INPUTS.mkdir(parents=True, exist_ok=True)
with (CAD_INPUTS/'sweep.json').open('x', encoding='utf-8') as stream:
    json.dump(shadows, stream)
