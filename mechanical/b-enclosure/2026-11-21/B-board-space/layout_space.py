"""Calculate conservative double-sided placement envelopes from finished B service CAD (mm)."""
from pathlib import Path
import hashlib
import json
import math
import sys
import numpy as np
import shapely as sh
from shapely.geometry import box, Polygon, MultiPolygon
import FreeCAD as App
import FreeCADGui as Gui
import Part

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
sys.path.insert(0, str(ROOT.parent))
from workspace import WORKSPACE
PITCH=.25
Z_GAP=.20
XY_GAP=.20
EDGE=.30
CELL_EXTRA=.30
GRID_X,GRID_Y=-18.5,-21.75
PALETTE={'Front':['#c1dece','#7ab7a2','#368375','#254d64'],
         'Rear':['#d6e7f2','#a6cfe0','#6badc1','#3e819e','#375c80']}
HEIGHTS={'Front':[.4,.6,.7,.8],'Rear':[1.,2.,3.,4.,6.]}
def rgb(s):
    return tuple(int(s[i:i+2],16)/255 for i in (1,3,5))
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def shadow(s):
    """Project a solid clipped to the required Z interval, including its horizontal cut caps."""
    vertices,triangles=s.tessellate(.015)
    if not triangles:return Polygon()
    verts=np.array([[v.x,v.y] for v in vertices])
    xy=verts[np.asarray(triangles)]
    a=xy[:,1]-xy[:,0];b=xy[:,2]-xy[:,0]
    polys=sh.polygons(xy[np.abs(a[:,0]*b[:,1]-a[:,1]*b[:,0])>1e-9])
    return sh.union_all(polys,grid_size=.0001) if len(polys) else Polygon()
def group_union(geom):
    return sh.union_all([g for g in geom if not g.is_empty],grid_size=.0001)
def ranges(row):
    inds=np.where(np.diff(np.r_[False,row,False]))[0]
    return list(zip(inds[::2],inds[1::2]))
def footprint(mask):
    rows=[]
    for j,row in enumerate(mask):
        for a,b in ranges(row):
            rows.append(box(GRID_X+a*PITCH,GRID_Y+j*PITCH,GRID_X+b*PITCH,GRID_Y+(j+1)*PITCH))
    return group_union(rows).simplify(.00001,preserve_topology=True)
def largest_rectangle(mask):
    counts=np.zeros(mask.shape[1]+1,dtype=int);best=(0,None)
    for row,line in enumerate(mask):
        counts[:-1]=np.where(line,counts[:-1]+1,0)
        stack=[]
        for col,h in enumerate(counts):
            start=col
            while stack and h<stack[-1][1]:
                left,height=stack.pop(); area=height*(col-left);start=left
                if area>best[0]:
                    best=(area,(left,row+1-height,col,row+1))
            if h and (not stack or h>stack[-1][1]):stack.append((start,h))
    if best[1] is None:return None
    x0,y0,x1,y1=best[1]
    return [float(GRID_X+x0*PITCH),float(GRID_Y+y0*PITCH),float((x1-x0)*PITCH),float((y1-y0)*PITCH)]
def pieces(g):
    if g.is_empty:return []
    return list(g.geoms) if isinstance(g,MultiPolygon) else [g]
def native(g,z,t):
    solids=[]
    for p in pieces(g):
        loops=[Part.makePolygon([App.Vector(x,y,z) for x,y in r.coords]) for r in [p.exterior,*p.interiors]]
        face=Part.makeFace(loops,'Part::FaceMakerBullseye')
        solids.append(face.extrude(App.Vector(0,0,t)))
    result=Part.makeCompound(solids)
    assert result.isValid() and abs(result.Volume-abs(t)*g.area)<.001
    return result
def launch(dest):
    import subprocess
    dest.mkdir(parents=True,exist_ok=False)
    macro=dest/'build.FCMacro'
    macro.write_text("from pathlib import Path\nimport traceback,FreeCADGui as Gui\nfrom PySide import QtCore\nGui.getMainWindow().hide()\n"
        +f"src=Path({str(Path(__file__).resolve())!r})\ndst=Path({str(dest)!r})\n"
        +"try:\n ns={'__file__':str(src),'__name__':'builder'}\n exec(compile(src.read_text(encoding='utf8'),str(src),'exec'),ns)\n ns['run'](dst)\nexcept Exception:\n (dst/'error.txt').write_text(traceback.format_exc(),encoding='utf8')\nfinally:\n QtCore.QTimer.singleShot(0,QtCore.QCoreApplication.quit)\n",encoding='utf8')
    si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
    with (dest/'freecad.log').open('w',encoding='utf8') as out:
        subprocess.run([sys.executable.replace('python.exe','FreeCAD.exe'),'-u',str(dest/'session.cfg'),str(macro)],stdout=out,stderr=out,startupinfo=si,timeout=230)
    if not (dest/'pass.json').exists():
        err=dest/'error.txt';raise RuntimeError(err.read_text(encoding='utf8') if err.exists() else 'Output was not verified')

def run(dest):
    dest=Path(dest);Gui.activateWorkbench('PartWorkbench')
    folder=WORKSPACE/ROOT.name/'B-service-section';inputfile=folder/'WristFlow-B-Open.FCStd'
    prior=json.loads((folder/'geometry.json').read_text(encoding='utf8'))
    assert digest(inputfile)==prior['open_fcstd_sha256']
    src=App.openDocument(str(inputfile));board=src.Board.Shape.copy()
    top=board.BoundBox.ZMax;bottom=board.BoundBox.ZMin
    exact=board.Volume/(top-bottom)
    assert all(abs(f.Area-exact)<1e-5 for f in board.Faces if f.BoundBox.ZLength<1e-5)
    assert abs(Part.read(str(folder/'B-board-outline.step')).Volume-board.Volume)<1e-5
    shapes={o.Name:o.Shape.copy() for o in src.Objects if o.TypeId=='Part::Feature'}
    def names(*groups): return [o.Name for k in groups for o in src.getObject(k).Group]
    fixed=names('Fixed')
    service=names('Tools','FreeHarness','Jig','Meter','Mated','Links')
    rear=names('Lid','Cell','Fasteners','Circuit')+service
    rear=[k for k in rear if k not in ('Board','ScreenSocket')]
    front=['ScreenSocket']
    cache={}
    def clip(k,lo,hi):
        s=shapes[k];bb=s.BoundBox
        lo=max(lo,bb.ZMin);hi=min(hi,bb.ZMax)
        if hi-lo<.00001:return Polygon()
        key=(k,round(lo,6),round(hi,6))
        if key not in cache:
            if lo>bb.ZMin+.00001 or hi<bb.ZMax-.00001:
                s=s.common(Part.makeBox(100,110,hi-lo,App.Vector(-50,-55,lo)))
            cache[key]=shadow(s)
        return cache[key]
    real=shadow(board)
    usable=real.buffer(-EDGE)
    assert abs(real.area-exact)<.25
    antenna=shapes['Antenna'].BoundBox
    rf_hold=box(-22,antenna.YMin,22,24)
    def rect_bounds(keys):
        q=group_union([shadow(shapes[k]) for k in keys])
        return q.envelope
    land=group_union([rect_bounds([k for k in names('Circuit') if k.startswith(p)]) for p in ['LPad','DBG','Sleeve']])
    mod=rect_bounds(['ModuleParts','ModuleSubstrate'])
    # Additional rear tool round sections model the specified service actions. Their screws are removed first.
    screws=group_union([sh.Point(x,y).buffer(1.30) for x,y in prior['board_supports_xy']])
    wirehold=rect_bounds(['Lead0','Lead1','Lead2','FreeLead0','FreeLead1','FreeLead2'])
    # Keep the FPC bend/service strip clear on its own mounting side until the actual tail is known.
    fpc_hold=rect_bounds(['ScreenFlex','ScreenTail','ScreenSocket'])
    X,Y=np.meshgrid(GRID_X+PITCH*np.arange(148),GRID_Y+PITCH*np.arange(174))
    tiles=sh.box(X,Y,X+PITCH,Y+PITCH)
    masks={};results={}
    for face,hs in HEIGHTS.items():
        results[face]=[];masks[face]=[]
        for h in hs:
            blockers=[]
            # During rearward board withdrawal, fixed frame parts below the initial chip top also matter.
            end=top+h+Z_GAP if face=='Front' else bottom
            blockers.extend(clip(k,-20,end) for k in fixed)
            if face=='Rear':
                blockers.extend([land,mod,screws,wirehold])
                for k in rear:
                    lo=bottom-h-Z_GAP-(CELL_EXTRA if k=='Battery' else 0)
                    blockers.append(clip(k,lo,bottom+.01))
            else:
                blockers.append(fpc_hold)
                blockers.extend(clip(k,top-.01,top+h+Z_GAP) for k in front)
            solid_mask=group_union(blockers).buffer(XY_GAP+.02)
            allowed=usable.difference(solid_mask)
            plan=allowed.difference(rf_hold)
            sh.prepare(allowed);sh.prepare(plan)
            mechanical=sh.covers(allowed,tiles);mask=sh.covers(plan,tiles)
            if masks[face]:assert not np.any(mask & ~masks[face][-1])
            masks[face].append(mask)
            rect=largest_rectangle(mask)
            if rect:
                x,y,w,l=rect
                assert plan.buffer(1e-8).covers(box(x,y,x+w,y+l))
            results[face].append({'height':h,'area':float(mask.sum()*PITCH**2),
                    'before_rf_hold':float(mechanical.sum()*PITCH**2),'rectangle_xywh':rect})
    doc=App.newDocument('BoardSpace');doc.Label='B 版主板双面器件空间'
    def group(key,label):
        obj=doc.addObject('App::DocumentObjectGroup',key);obj.Label=label;return obj
    main=group('Outline','主板与已占接口');fg=group('Front','屏幕侧 · 器件高度');rg=group('Rear','电池侧 · 器件高度')
    reserve=group('Review','天线端 · 待硬件复核');ref=group('Reference','可选参照 · 电池、屏幕、喇叭、充电触点')
    def add(g,k,label,s,color):
        o=doc.addObject('Part::Feature',k);o.Label=label;o.Shape=s
        g.addObject(o);o.ViewObject.ShapeColor=rgb(color);o.ViewObject.LineColor=(.15,.20,.23);o.ViewObject.LineWidth=1.
        return o
    plate=add(main,'PCB','主板实体 · 1276.83 mm²',board,'#a2a6a9')
    for k in ['ModuleParts','ModuleSubstrate','ScreenSocket','BatHeader','BatPlug','KeyBody',*[f'Clamp{i}' for i in (1,2,3)]]:
        add(main,k,src.getObject(k).Label,shapes[k],'#a48664' if 'Module' in k else '#c29c60')
    for face,hs in HEIGHTS.items():
        side=fg if face=='Front' else rg
        for i,h in enumerate(hs):
            mask=masks[face][i].copy()
            if i+1<len(hs):mask &= ~masks[face][i+1]
            if not np.any(mask):continue
            poly=footprint(mask)
            s=native(poly,top if face=='Front' else bottom,h if face=='Front' else -h)
            o=add(side,face+'Zone'+str(i),('屏侧' if face=='Front' else '电池侧')+'　'+str(h)+' mm 高度包络',s,PALETTE[face][i])
            o.addProperty('App::PropertyLength','BodyHeight','Space').BodyHeight=h
            o.addProperty('App::PropertyArea','BandArea','Space').BandArea=poly.area
            o.addProperty('App::PropertyArea','AreaAtLeast','Space').AreaAtLeast=results[face][i]['area']
    rf_board=rf_hold.intersection(real)
    add(reserve,'RFHold','板端暂缓布件区 · 射频工程待定',native(rf_board,top+.005,.035),'#c78167')
    add(reserve,'RFUnderside','板端暂缓布件区 · 后侧示意',native(rf_board,bottom-.005,-.035),'#c78167')
    add(reserve,'Radiator','模组天线 · 18 × 6.1 mm',shapes['Antenna'],'#bc6956')
    for k in ['Screen','Battery','Speaker','PullAccess','Fixture','PpgBoard','LidFlex','LidTower','Motor','Frame']:
        obj=add(ref,'Ref'+k,src.getObject(k).Label,shapes[k],'#8e9696');obj.ViewObject.Transparency=78
    for i,x in enumerate([-1.75,1.75]):
        p=add(ref,'ProvisionalCharge'+str(i),'后盖触点 · 中心距约 3.5 mm，照片估读',
              Part.makeCylinder(1.05,.89,App.Vector(x,-15,0)),'#bb9d69')
        p.addProperty('App::PropertyString','Evidence','Space').Evidence='实物带尺照片估读；电极方向、磁极及行程未确认。触点直径及中心位置沿用概念值。'
    ex=Part.makeBox(8,6,2,App.Vector(-7,-18,bottom-2))
    assert footprint(masks['Rear'][1]).covers(box(-7,-18,1,-12))
    for k in set(fixed+rear):
        if ex.BoundBox.intersect(shapes[k].BoundBox):
            assert ex.common(shapes[k]).Volume<1e-6,k
    add(ref,'Example','独立核验样块 8 × 6 × 2 mm · 电池侧',ex,'#eea333')
    meta=doc.addObject('App::FeaturePython','Basis');meta.Label='计算依据与未定项'
    for k,v in {'Grid':'0.25 mm，整格可用才计入','Datum':'与 B 装配相同。前板面 Z=12.10，后板面 Z=11.30；Y 正向朝维修触点端。','Allowance':'边缘 0.30 mm；障碍周围 XY 0.20 + 网格投影补偿 0.02 mm；Z 0.20 mm；电芯额外 Z 0.30 mm。','RF':'Y ≥ 14.55 mm 全端暂缓新增器件；为本轮评估假设，实际依 DS5203 §8.4 审核。','Scope':'色块表示任选器件的高度档。焊盘/走线、贴装、公差、电磁兼容需硬件设计复核。预排 IC 未固定，需算在色块内。'}.items():
        meta.addProperty('App::PropertyString',k,'Review');setattr(meta,k,v)
    ref.ViewObject.Visibility=False
    for q in ref.Group:q.ViewObject.Visibility=False
    srcname=src.Name;App.closeDocument(srcname);App.setActiveDocument(doc.Name)
    def record_view(key,normal):
        doc.recompute();v=Gui.getDocument(doc.Name).activeView();v.setCameraType('Orthographic')
        if hasattr(v,'setAnimationEnabled'):v.setAnimationEnabled(False)
        v.setCameraOrientation(App.Rotation(App.Vector(1,0,0),App.Vector(0,1,0),App.Vector(*normal),'ZYX').Q)
        v.fitAll();Gui.updateGui();v.saveImage(str(dest/(key+'.png')),1050,860,'White')
    record_view('front',(.37,-.40,1));record_view('rear',(.38,-.40,-1))
    name=dest/'WristFlow-B-Board-Space.FCStd';doc.saveAs(str(name))
    proof={'base_sha256':digest(inputfile),'board_exact_mm2':exact,'height_bins':results,
           'grid':PITCH,'clearance_xy_z':[XY_GAP,Z_GAP],'edge':EDGE,'battery_extra_z':CELL_EXTRA,
           'rf_hold_y_from':antenna.YMin,'maps':{},'board_shape':sh.to_wkt(real,rounding_precision=4),'rf_shape':sh.to_wkt(rf_board,rounding_precision=4)}
    for f,m in masks.items():
        proof['maps'][f]=[sh.to_wkt(footprint(x),rounding_precision=3) for x in m]
    recheck={o.Name:o.Shape.Volume for o in doc.Objects if o.TypeId=='Part::Feature'}
    App.closeDocument(doc.Name);d=App.openDocument(str(name));d.recompute()
    for k,vol in recheck.items():
        s=d.getObject(k).Shape;assert s.isValid() and abs(s.Volume-vol)<1e-5,k
    for f,hs in HEIGHTS.items():
        groupobj=d.getObject(f);vol=sum(o.Shape.Volume for o in groupobj.Group)
        want=sum((h-(hs[i-1] if i else 0))*results[f][i]['area'] for i,h in enumerate(hs))
        assert abs(vol-want)<.005
    proof.update({'reopened_solids_objects':len(recheck),'fcstd_sha256':digest(name)})
    (dest/'pass.json').write_text(json.dumps(proof,ensure_ascii=False),encoding='utf8')
    App.closeDocument(d.Name)
if __name__=='__main__':launch(Path(sys.argv[1]).resolve())
