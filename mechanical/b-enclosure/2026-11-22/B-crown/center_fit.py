"""Side-centred BMQ22, buffer around the existing pack, service from inside."""
from pathlib import Path
import sys,json
sys.path.insert(0,str(Path(__file__).resolve().parent))
from crown_fit import App,Part,V,HERE,SOURCE,block,prism,bend,union

PREVIOUS=HERE/'BMQ22-B/WristFlow-B-Crown.FCStd'
Y,Z=0.,8.
def shaft(x,length,r,y=0,z=8):
    return Part.makeCylinder(r,length,V(x,y,z),V(1,0,0))

def rounded_wire(x,w,h,z,r):
    blank=block(x,-w/2,z-h/2,1,w,h)
    solid=blank.makeFillet(r,[e for e in blank.Edges if e.BoundBox.XLength>.99])
    return next(f.OuterWire for f in solid.Faces if abs(f.BoundBox.XMax-x)<1e-6)

def run(dest):
    dest=Path(dest)
    source=App.openDocument(str(SOURCE));stock=source.Frame.Shape.copy()
    stockcap=source.Crown.Shape.copy()
    App.closeDocument(source.Name)
    doc=App.openDocument(str(PREVIOUS))
    changes={}
    def save(k,s,label=None):
        obj=doc.getObject(k);obj.Shape=s;changes[k]=s
        if label:obj.Label=label
    delta=V(4.7,-15.2,2.65)
    for key in ['EncoderBody','EncoderRotor','EncoderPins','EncoderPads','Daughter','SwitchFollower']:
        save(key,doc.getObject(key).Shape.translated(delta))
    # The outer wall follows the encoder cross section down to the concentric journal.
    pod=Part.makeLoft([rounded_wire(x,w,h,z,r) for x,w,h,z,r in
        [(18.30,11.,8.25,7.65,1.0),(20.45,10.7,8.0,7.65,1.2),
         (22.14,7.4,6.70,7.88,1.0),(22.94,5.8,5.5,8.,1.1)]],True,True)
    pod=pod.cut(block(17.70,-6,11.10,1.03,12,2.0))
    frame=stock.fuse(pod)
    entry=block(17.75,-3.62,4.49,1.10,7.24,5.73)
    # Leave two side pads at the face of the stiffener, plus the shell around the case.
    frame=frame.cut(entry).cut(block(18.84,-3.28,4.51,.36,6.56,5.57))
    frame=frame.cut(block(18.84,-2.52,5.73,3.31,5.04,4.17))
    frame=frame.cut(shaft(22.11,.85,1.18)).cut(shaft(22.89,.08,1.29))
    frame=frame.cut(block(17.70,-1.98,1.30,1.0,3.96,3.55))
    plate=block(17.87,-5.0,4.49,.44,10,5.80)
    frame=frame.cut(plate)
    # Two screws are accessible with the tray withdrawn. Their heads sit above pack height.
    scr=[]
    for y in (-4.42,4.42):
        frame=frame.cut(shaft(18.27,2.12,.43,y,8.72))
        plate=plate.cut(shaft(17.80,.65,.52,y,8.72))
        plate=plate.cut(shaft(17.84,.22,.74,y,8.72))
        scr.extend([shaft(18.06,1.80,.425,y,8.72),shaft(17.86,.20,.72,y,8.72)])
    # Cable passes under the edge of the plate.
    plate=plate.cut(block(17.82,-1.96,4.45,.55,3.92,.26))
    save('Frame',frame.removeSplitter(),'B 中框 · 右侧正中单冠，内侧装入')
    save('Retainer',plate,'小板内压片 · 从电池仓侧拆装')
    save('RetainerScrews',Part.makeCompound(scr),'内侧压片螺钉 · 两处名义包络')
    sleeve=union([shaft(22.90,.65,1.26),shaft(22.94,.13,1.50)]).cut(shaft(22.85,.76,.86))
    save('GuideSleeve',sleeve)
    drive=union([block(19.75,-.37,7.63,2.98,.74,.74),shaft(22.73,1.15,.80),
                 shaft(22.73,.15,1.07),shaft(23.88,.98,.60)])
    cap=shaft(23.32,1.76,3.30).cut(shaft(23.31,.57,1.68)).cut(shaft(23.87,1.10,.60))
    save('Crown',cap,'正中冠帽 · Ø6.6，背面避空，试装螺纹 M1.2')
    save('CrownShaft',drive,'表冠轴 · Ø1.60，0.74 方榫，冠帽端名义啮合 0.98')
    # One continuous rigid-flex ribbon. The first bend moves it below the tray.
    from shapely.geometry import LineString
    lower=[(17.61,0),(16.8,0),(16.1,.20),(15.55,.68),(15.20,1.45),(14.40,5.90),
           (13.6,11.75),(11.05,16.0),(9.8,17.75),(9.38,18.26),(8.93,18.5),(8.10,18.5)]
    outline=LineString(lower).buffer(1.75,cap_style=2,join_style=1)
    edge=[(x,y,1.18) for x,y in list(outline.exterior.coords)[:-1]]
    ribbon=union([block(18.31,-1.75,1.98,.10,3.50,2.65),bend(17.61,1.98,-1.75,.75,(1,-1)),
                  prism(edge,(0,0,.10)),bend(8.10,1.98,16.75,.75,(-1,-1)),
                  block(7.30,16.75,1.98,.1,3.5,8.05),bend(6.60,10.03,16.75,.75,(1,1)),
                  block(3.45,16.75,10.73,3.15,3.5,.1)])
    save('CrownFlex',ribbon,'编码器 FPC · 沿侧壁下行，经托板下方回至原 6P 座')
    doc.recompute()
    groups='Fixed Circuit Fasteners Cell Mated Lid Links Display Band CrownAssembly'.split()
    parts={o.Name:o.Shape for g in groups for o in doc.getObject(g).Group}
    hits={}
    for k in sorted(changes):
        s=parts[k]
        assert s.isValid() and s.Volume>0,k
        for j,t in parts.items():
            if j==k or (j in changes and j<k) or not s.BoundBox.intersect(t.BoundBox):continue
            v=s.common(t).Volume
            if k=='Frame' and j not in changes and stock.BoundBox.intersect(t.BoundBox):
                v-=stock.common(t).Volume
            if v>.00005:hits[k+'/'+j]=round(v,6)
    moving=('Crown','CrownShaft','SwitchFollower')
    for k in moving:
        s=parts[k].translated(V(-.25,0,0))
        for j,t in parts.items():
            if j in moving or not s.BoundBox.intersect(t.BoundBox):continue
            v=s.common(t).Volume
            if v>.00005:hits['pressed '+k+'/'+j]=round(v,6)
    battery=parts['Battery']
    metrics={'axis_y_mm':0.,'axis_z_mm':8.,'rest_crown_x_mm':round(cap.BoundBox.XMax,3),
             'old_crown_x_mm':round(stockcap.BoundBox.XMax,3),
             'original_case_half_width_mm':round(stock.BoundBox.XMax,3),
             'new_local_case_x_mm':round(frame.BoundBox.XMax,3),
             'nominal_battery_box_mm':[battery.BoundBox.XLength,battery.BoundBox.YLength,battery.BoundBox.ZLength],
             'sensor_or_seal_test':False,'hits_mm3':hits}
    pairs=[('EncoderBody','Battery'),('Daughter','Battery'),('Retainer','Battery'),('CrownFlex','Battery'),
           ('CrownFlex','Tray'),('CrownFlex','Back'),('CrownFlex','Antenna'),('Retainer','Antenna'),
           ('Retainer','Daughter'),('GuideSleeve','Frame'),('Retainer','Frame'),('CrownFlex','Daughter'),
           ('Daughter','Frame'),('CrownShaft','SwitchFollower'),('GoldFingers','ZIFContacts'),('ZIFContacts','Board')]
    metrics['distance_mm']={a+'/'+b:round(parts[a].distToShape(parts[b])[0],4) for a,b in pairs}
    metrics['pressed_clearances_mm']={k:round(cap.translated(V(-.25,0,0)).distToShape(parts[k])[0],4)
                                      for k in ('Frame','GuideSleeve')}
    (dest/'center-check.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2),encoding='utf8')
    assert not hits,hits
    assert len(frame.Solids)==len(ribbon.Solids)==len(cap.Solids)==1
    for k in ['CrownFlex/Daughter','Retainer/Daughter','Daughter/Frame','Retainer/Frame',
              'CrownShaft/SwitchFollower','GoldFingers/ZIFContacts','ZIFContacts/Board']:
        assert metrics['distance_mm'][k]<1e-5,(k,metrics)
    # Update section objects already present in the comparison study and add the pack.
    for name,group,prefix,slab in [
        ('表冠轴线 Y=0','CrownSection','AX',block(6.7,-.07,0,18.9,.14,13.1)),
        ('主板插座 Y=18.75','SocketSection','BX',block(.1,18.72,.14,14,.06,12.86))]:
        sub=doc.getObject(group);sub.Label='剖面 · '+name
        existing={o.Name[len(prefix):]:o for o in sub.Group}
        for key in set(existing)|set(['Battery','Tray','Frame','Retainer','CrownFlex','ModuleSubstrate','Antenna','Board']):
            s=parts[key].common(slab);o=existing.get(key)
            if s.Volume<1e-6:
                if o:doc.removeObject(o.Name)
                continue
            if not o:o=doc.addObject('Part::Feature',prefix+key);sub.addObject(o)
            o.Label='剖 · '+doc.getObject(key).Label;o.Shape=s
            o.ViewObject.ShapeColor=doc.getObject(key).ViewObject.ShapeColor;o.ViewObject.Visibility=False
        sub.Visibility=False
    # Open directly on the front elevation the wearer asked to compare.
    for n in ('ViewCut','Band','Lid','Cell','Jig','Meter','Tools','FreeHarness','Mated','SocketSection','CrownSection'):
        doc.getObject(n).Visibility=False
    doc.Frame.ViewObject.Transparency=0
    import FreeCADGui as Gui
    Gui.activeDocument().activeView().viewTop()
    Gui.activeDocument().activeView().fitAll()
    doc.Label='WristFlow B · 右侧正中单表冠 / 原 500 mAh'
    doc.recompute();target=dest/'WristFlow-B-Centered-Crown.FCStd'
    doc.saveAs(str(target));App.closeDocument(doc.Name)
    old=App.openDocument(str(PREVIOUS));rest=App.openDocument(str(target))
    for key in ('Battery','Board','Tray','Motor','CrownZIF','CN1'):
        a=old.getObject(key).Shape;b=rest.getObject(key).Shape
        assert abs(a.Volume-b.Volume)<1e-6 and abs(a.Volume-a.common(b).Volume)<1e-5,key
    for key in changes:
        o=rest.getObject(key);assert o.Shape.isValid() and abs(o.Shape.Volume-changes[key].Volume)<1e-5,key
    App.closeDocument(rest.Name);App.closeDocument(old.Name)
    (dest/'reopened.ok').write_text('ok')
