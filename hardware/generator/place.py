from library import *
from shapely.geometry import shape, mapping
from shapely.prepared import prep
from functools import lru_cache
import itertools
from collections import Counter
from workspace import CAD_INPUTS
mechanical=json.loads((CAD_INPUTS/'constraints.json').read_text(encoding='utf-8'))
boardshape=shape(mechanical['outline'])
constraints={k:{n:shape(v) for n,v in row.items()} for k,row in mechanical['bands'].items()}
layers={side:[] for side in ('F','B')}
placed={}
def position(ref,side,cx,cy,rot,from_body=False):
    p=parts[ref]
    center=transform(p['body'],0,0,rot,side).centroid if from_body else Point(0,0)
    x=cx-center.x;y=cy-center.y
    p.update(side=side,x=x,y=y,rot=rot)
    p['bounds']=transform(p['env'],x,y,rot,side).envelope
    p['physical']=transform(p['body'],x,y,rot,side)
    placed[ref]=p
    if p['board']=='main':layers[side].append(p['bounds'])

position('U1','F',-6.5,-8.25,180,True)
reference_outline=boardshape
u=parts['U1']
tab_shapes=[transform(g,u['x'],u['y'],u['rot'],'F').buffer(.525,quad_segs=12)
            for pin,g in u['padshapes'] if pin in ('1','48')]
boardshape=unary_union([boardshape,*tab_shapes])
added=boardshape.difference(reference_outline)
assert added.bounds[1]>-16.1
position('U16','F',8.25,20.25,90,True)
position('J201','B',10.545,5.135,90,True)
position('J1','B',-9,15.45,180)
position('J2','B',3.2,16.55,180,True)
for i,y in enumerate((-2.85,-.95,.95,2.85)):position('K'+str(i+1),'F',13.5,y,0)
position('MT1','B',16.6,-7.4,0)
position('MT2','B',17.3,-9.1,0)
for i in range(5):position('TP'+str(11+i),'B',-3-i*2,20,0)
for ref,y in [('JP1',14.38),('R49',16.16),('R50',17.94)]:position(ref,'B',-15,y,0)
height_bands=[.1,.75,1.25,1.7,2.2,3.3]
@lru_cache(None)
def shadow(side,h):
    level=next(v for v in height_bands if v>=h)
    return unary_union(list(constraints[side+str(level)].values()))
# Reserve working areas throughout placement, including connector actuation with cover open.
reserve={'F':unary_union([box(11.7,-4.8,16.6,4.8),box(-15.55,-22.25,2.55,-16.05)]),
         'B':unary_union([box(-.55,14.10,6.95,18.85),box(-12.3,12.25,-5.7,18.65),
                          box(-17,13.4,-13.2,18.95),box(-12,19.1,-2.1,20.9),box(7.945,.935,13.145,9.335),
                          box(15.8,-9.95,18.2,-6.35),box(-15.55,-22.25,2.55,-16.05)])}
@lru_cache(None)
def allowed(side,h):
    motion=json.loads((CAD_INPUTS/'sweep.json').read_text(encoding='utf-8'))
    sweepname='B0' if side=='B' else 'F'+str(next(i for i in [.75,1.25,1.7,3.3] if i>=h))
    return prep(boardshape.buffer(-.51).difference(shadow(side,h).buffer(.15)).difference(reserve[side]).difference(shape(motion[sweepname]).buffer(.12)))
centers={'Module':('B',-8,1),'Charger':('B',-6,11.3),'Rails':('B',-10,-6),
         'AMOLED':('B',6,6),'Audio':('F',-8,15),'Mic':('F',16,14),
         'Motion light':('F',6,-5),'Crown motor':('F',9,-6),'Rear interface':('F',1,14),'Service':('B',-8,11)}
anchors={
 'U3':('B',-5,12),'U17':('B',-.5,13),'D19':('B',1,12),'U19':('B',-8,4),
 'U4':('B',-11,-5),'U15':('B',-7,-4),'U2':('B',-10,-1),
 'U13':('F',5,-2),'U14':('F',-14,14),'U6':('F',-6,15),'U9':('F',15.8,15.6),
 'Q3':('F',9,1),'Q2':('F',12,-7),'U21':('F',2,13),
 'C29':('F',4,-3),'C30':('F',5,-3),'C31':('F',-14,14),'C35':('F',7.3,18.6),
 'C1':('B',-10,4),'C38':('B',-10,4),'R11':('B',-6,18),'R12':('B',-5,18),
 'R61':('F',9,1),'R62':('F',9.5,3),'R63':('F',10,3),'R60':('B',-5,12),'R64':('B',-5,12),
 'C201':('B',9,9.3),'C202':('B',8.5,9.3),'C203':('B',8,9.3)}
anchors.update({'U3':('F',1.0,15.5),'U6':('F',-9,15.5),'U14':('F',16.5,7.1),'U21':('B',2,3),'C1':('B',-9.25,5.28),'C38':('B',-9.25,5.28)})
for ref,num in [('R16','40'),('R17','39'),('R18','43'),('R19','42'),('R20','41')]:
    g=next(g for n,g in u['padshapes'] if n==num)
    xy=transform(g,u['x'],u['y'],u['rot'],'F').centroid
    anchors[ref]=('B',xy.x,xy.y)
parents={'C3':'U3','C6':'U3','C7':'U3','C8':'U3','R60':'U3','R64':'U3',
         'C2':'U17','R4':'U17','C18':'U6','C19':'U6','C24':'U6','R27':'U6','R28':'U6','C20':'U6','C21':'U6',
         'C39':'U21','C40':'U21','C4':'U2','C5':'U2','C9':'U4','C10':'U4','C33':'U15','C34':'U15',
         'C26':'U9','C27':'U9','R30':'U9','L4':'U9','C29':'U13','C30':'U13','C31':'U14','C35':'U16',
         'C28':'Q2','R37':'Q2','R38':'Q2','D16':'Q2','R61':'Q3','U19':'C1'}
priority='U3 C7 C6 C8 C3 U6 C19 C18 C24 U21 C40 C39 C1 C38 U19 U13 U14 U9 C26 C27 U17 C2 U4 C10 C9 U15 C34 C33 U2 C4 C5 R16 R17 R18 R19 R20 Q3 Q2 C28 D16'.split()
order=sorted((p for p in parts.values() if p['board']=='main' and p['ref'] not in placed),
             key=lambda p:(priority.index(p['ref']) if p['ref'] in priority else 100 if p['ref'] in parents else 110,
                           p['ref'] not in anchors,-p['env'].area))
for p in order:
    ref=p['ref']
    side,cx,cy=anchors.get(ref,centers[p['group']])
    if parents.get(ref) in placed:
        parent=placed[parents[ref]]
        side,cx,cy=parent['side'],parent['x'],parent['y']
    occupied=prep(unary_union(layers[side]).buffer(1.00,join_style=2))
    candidates=[]
    for rot in (0,90):
        b=transform(p['env'],0,0,rot,side).bounds
        for dx,dy in itertools.product(range(-36,37),range(-42,43)):
            x,y=dx*.5,dy*.5
            cost=(x-cx)**2+(y-cy)**2+rot/1000
            candidates.append((cost,x,y,rot,b))
    for cost,x,y,rot,b in sorted(candidates):
        bounds=box(x+b[0],y+b[1],x+b[2],y+b[3])
        if allowed(side,p['height']).covers(bounds) and not occupied.intersects(bounds):
            position(ref,side,x,y,rot)
            break
    else:raise ValueError('No placement '+ref)
# Flat rear layout is a terminal-location study. The fold length is not a fabrication contour.
for ref,x,y,side,rot in [('U301',0,0,'F',90),('C301',-3.75,2.65,'F',0),('C302',-.95,2.85,'F',0),
                          ('C303',1.5,3.05,'F',0),('C304',4.25,2.65,'F',90),('JF1',3.2,31,'F',0),
                          ('CH1',-1.75,-15,'F',0),('CH2',1.75,-15,'F',0),('SP1',-1.45,14.8,'F',90),('SP2',-1.45,13.2,'F',90)]:
    position(ref,side,x,y,rot)
position('ENC1','F',0,0,0)
for i in range(4):position('JC'+str(i+1),'F',(i-1.5)*1.1,4.55,0)
for i in range(4):
    position('P'+str(i+1),'B',.03,(i-1.5)*1.9,0)
    position('JC'+str(i+11),'F',2.45,(i-1.5)*1.1,90)
deck={'main':boardshape,'crown':box(-3.5,-3.37,3.5,5.125),
      'bridge':box(-1.73,-4.75,2.95,4.75),
      'rear':unary_union([box(-5.4,-4.25,5.4,4.25),box(-2.6,-16.1,2.6,-3.75),
                          box(-2.5,4,0,16),box(.45,3.5,5.95,32.25)])}
assert set(placed)==set(parts)
print('PLACED',Counter((p['board'],p['side']) for p in parts.values()))
