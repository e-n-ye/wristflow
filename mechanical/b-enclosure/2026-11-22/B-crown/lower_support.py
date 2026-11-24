"""B chassis: support the lower board edge and common the encoder and button grounds."""
from pathlib import Path
import json
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from routing_review import App, Part, V, HERE, block, prism
from shared_edge import section
from layout_space import shadow

BASE = HERE / 'B-rear-FPC/WristFlow-B-Rear-FPC.FCStd'
FILE = 'WristFlow-B-Lower-Support.FCStd'
NETS = ('A', 'GND', 'B', 'KEY')
EPS = .00001


def rib_blank():
    # Widen only the root at the inner strap-end wall. The level bearing face
    # reaches the existing locating boss and shares its 12.10 mm datum.
    xy = [(4.08,-22.55),(6.22,-22.55),(6.03,-20.80),(6.03,-11.25),
          (5.80,-10.90),(4.50,-10.90),(4.27,-11.25),(4.27,-20.80)]
    yz = [(-22.55,12.10),(-10.90,12.10),(-10.90,12.75),(-14.80,13.05),
          (-20.55,13.55),(-21.70,13.92),(-22.55,14.20)]
    plan = prism([(x,y,12.10) for x,y in xy], (0,0,3))
    profile = prism([(3.50,y,z) for y,z in yz], (3.50,0,0))
    return plan.common(profile).removeSplitter()


def property_value(obj, typ, name, value):
    if name not in obj.PropertiesList:
        obj.addProperty(typ, name, 'Design')
    setattr(obj, name, value)


def four_contacts(d):
    # Reuse the working compressed profile, pad and solder fillet dimensions.
    leaves = [d.getObject(f'CrownLeaf{i}') for i in range(4)]
    pads = [d.getObject(f'CrownPad{i}') for i in range(4)]
    fillet = min(d.CrownJoints.Shape.Solids, key=lambda s:s.CenterOfMass.y)
    fillets = []
    for i, net in enumerate(NETS):
        center = (i - 1.5) * 1.90
        for obj, description in ((leaves[i], '主板弹片 · S7241-45R 压缩态'),
                                 (pads[i], '表冠水平小板镀金面')):
            obj.Shape = obj.Shape.translated(V(0,center-obj.Shape.BoundBox.Center.y,0))
            obj.Label = net + ' · ' + description
            property_value(obj, 'App::PropertyString', 'Net', net)
        fillets.append(fillet.translated(V(0,(i-1.5)*1.24-fillet.BoundBox.Center.y,0)))
    # Replace the reference cut at the true crown axis. The centred four-way
    # row has a gap at y=0, so neither an electrical pad nor a spring lies there.
    for name in ('AXCrownLeaf2','AXCrownPad2','CrownLeaf4','CrownPad4'):
        d.removeObject(name)
    d.CrownJoints.Shape = Part.makeCompound(fillets)
    d.CrownJoints.Label = '四路直角焊端 · A / GND / B / KEY'
    d.CrownBridge.Label = '表冠水平小板 · 四路对接'
    property_value(d.Daughter, 'App::PropertyStringList', 'ContactNets',
                   ['A = encoder A', 'GND = encoder common + switch terminal 1',
                    'B = encoder B', 'KEY = switch terminal 2 (pressed to GND)'])
    property_value(d.Daughter, 'App::PropertyString', 'KeyPolarity',
                   '低有效；若复用高有效 PA34 的硬件开关机，须在主板完成极性适配。')
    return leaves, pads


def slice_xy(shape, z):
    return shadow(section(shape,z))


def run(dest):
    dest = Path(dest)
    d = App.openDocument(str(BASE))
    old_frame = d.Frame.Shape.copy()
    frame_color = d.Frame.ViewObject.ShapeColor[:3]
    blank = rib_blank()
    rib = blank.cut(old_frame).removeSplitter()
    frame = old_frame.fuse(blank).removeSplitter()
    assert frame.isValid() and len(frame.Solids) == 1 and len(rib.Solids) == 1
    # Verify anchoring to both the lower wall and original screw boss.
    engagement = blank.common(old_frame)
    ends = [block(3,-23,12,5,2,3), block(3,-12.1,12,5,1.5,3)]
    roots = [engagement.common(end).Volume for end in ends]
    assert min(roots) > .35
    d.Frame.Shape = frame
    d.Frame.Label = '中框 · 表带端塑料托筋接原主板螺钉座'
    property_value(d.Frame, 'App::PropertyString', 'LowerSupport',
                   '试样假设中框该段为无导电填料、无金属镀层的工程塑料。托面与原座共面；禁止在托面放器件或露铜测试点。')
    amber = (.96,.66,.22)
    colors = []
    for facet in frame.Faces:
        p = facet.CenterOfMass
        local = 4.0 < p.x < 6.30 and -22.35 < p.y < -11.1 and 12.09 < p.z < 14.21
        colors.append(amber if local else frame_color)
    d.Frame.ViewObject.DiffuseColor = colors
    leaves, pads = four_contacts(d)
    board = d.Board.Shape
    land = slice_xy(rib,12.101).intersection(slice_xy(board,12.095))
    # The old integrated boss and new land meet. Measure overlap length directly on screw centreline.
    from shapely.geometry import LineString
    line = LineString([(5.15,-17),(5.15,-9.80)])
    bearing = land.intersection(line).length
    assert land.area > 5 and bearing > 3
    property_value(d.Board, 'App::PropertyString', 'LowerBearing',
                   '屏幕侧，托筋与板材实边接触；接触轮廓见中框或复核图。')

    groups = 'Fixed Circuit Fasteners Cell Mated Lid Links Display Band CrownAssembly'.split()
    fixed = [o for group in groups for o in d.getObject(group).Group if hasattr(o,'Shape') and o.Name!='Frame']
    for obj in fixed:
        if rib.BoundBox.intersect(obj.Shape.BoundBox):
            assert rib.common(obj.Shape).Volume < EPS, obj.Name
    for i,(leaf,pad) in enumerate(zip(leaves,pads)):
        for obj in fixed:
            if obj not in (leaf,pad):
                for sh in (leaf.Shape,pad.Shape):
                    if sh.BoundBox.intersect(obj.Shape.BoundBox):
                        assert sh.common(obj.Shape).Volume < EPS, (i,obj.Name)
        assert leaf.Shape.common(pad.Shape).Volume < EPS
        assert leaf.Shape.distToShape(pad.Shape)[0]<EPS and leaf.Shape.distToShape(board)[0]<EPS
        assert pad.Shape.distToShape(d.CrownBridge.Shape)[0]<EPS
        assert leaf.Shape.distToShape(frame)[0] > EPS and pad.Shape.distToShape(frame)[0] > EPS
        for j,other in enumerate(pads):
            if i!=j: assert leaf.Shape.distToShape(other.Shape)[0] > .10
    for joint in d.CrownJoints.Shape.Solids:
        for target in (d.Daughter,d.CrownBridge):
            assert joint.distToShape(target.Shape)[0]<EPS
    # Components and laminate travel toward the opened back. All board-side
    # components here are represented by increasingly conservative prisms.
    board_sweep = section(board,11.70).translated(V(0,0,.40)).extrude(V(0,0,-16))
    assert board_sweep.common(rib).Volume < EPS
    for obj in [o for o in d.Circuit.Group if o.Name!='Board']:
        bb = obj.Shape.BoundBox
        sweep = block(bb.XMin,bb.YMin,bb.ZMin-16,bb.XLength,bb.YLength,bb.ZLength+16)
        if sweep.BoundBox.intersect(rib.BoundBox):
            assert sweep.common(rib).Volume < EPS, obj.Name
    for o in leaves:
        bb = o.Shape.BoundBox
        sw = block(bb.XMin,bb.YMin,bb.ZMin-16,bb.XLength,bb.YLength,bb.ZLength+16)
        for obj in [d.Frame,d.Daughter,d.CrownBridge]:
            assert sw.common(obj.Shape).Volume < EPS
    antenna_xy = shadow(d.Antenna.Shape)
    assert shadow(rib).intersection(antenna_xy).area < EPS
    assert slice_xy(board,11.70).intersection(antenna_xy).area < EPS
    assert not any(o.Name.startswith('AXCrownLeaf') or o.Name.startswith('AXCrownPad')
                   for o in d.CrownSection.Group)
    assert len(d.EncoderPads.Shape.Solids)==7 and len(d.CrownJoints.Shape.Solids)==4
    gaps = {obj.Name:round(rib.distToShape(obj.Shape)[0],3)
            for obj in (d.Antenna,d.ModuleSubstrate,d.Motor,d.MotorSeat,d.Screen,d.FrontRun)}
    report = dict(revision='four crown nets and lower chassis rib', interface=list(NETS),
                  original_encoder_function_pads=5, encoder_mechanical_pads=2,
                  working_contact_pitch_mm=1.9, working_height_mm=1.0,
                  rib_new_volume_mm3=round(rib.Volume,3), rib_width_mm=1.76,
                  root_and_boss_fusion_overlap_mm3=[round(v,3) for v in roots],
                  additional_board_bearing_mm2=round(land.area,3),
                  supported_length_on_boss_centerline_mm=round(bearing,3),
                  distances_from_new_rib_mm=gaps, board_removal_stroke_mm=16,
                  new_fixed_collisions=False, antenna_projection_overlap_mm2=0,
                  pending=['polymer and molding tolerances','board deflection, motor vibration, drop',
                           'enclosed BLE RF','schematic layout and PA34 polarity if used'])
    for root in d.RootObjects:
        if hasattr(root,'Visibility'): root.Visibility=False
    display_names = {'Frame','Board','ModuleSubstrate','ModuleParts','Antenna','Motor','MotorSeat',
                     'MotorWire0','MotorWire1','Clamp1','Clamp2','Clamp3','Crown','CrownShaft',
                     'CrownBridge','Daughter','EncoderBody'}
    for group in ('Fixed','Circuit','Fasteners','CrownAssembly'):
        g=d.getObject(group);g.Visibility=True
        for obj in g.Group: obj.Visibility = obj.Name in display_names
    import FreeCADGui as Gui
    Gui.activeDocument().activeView().viewTop()
    Gui.activeDocument().activeView().fitAll()
    d.Label='WristFlow B · 下板边托筋与四触点表冠'
    d.recompute()
    path=dest/FILE
    d.saveAs(str(path))
    App.closeDocument(d.Name)
    check=App.openDocument(str(path))
    assert check.Frame.Shape.isValid() and len(check.Frame.Shape.Solids)==1
    assert abs(check.Frame.Shape.Volume-frame.Volume)<.0001
    for prefix,group in [('CrownLeaf',check.Circuit),('CrownPad',check.CrownAssembly)]:
        items=[o for o in group.Group if o.Name.startswith(prefix)]
        assert len(items)==4 and [o.Net for o in items]==list(NETS)
    assert len(check.LidFlexNew.Shape.Solids)==1 and check.Open6LidFlexNew.Shape.isValid()
    App.closeDocument(check.Name)
    (dest/'support-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2), encoding='utf-8')
    (dest/'reopened.ok').write_text('ok')
