"""Implement the eight mechanical review items against the saved four-contact prototype."""
import json, math, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from lower_support import App,Part,V,HERE,FILE as OLD_FILE,section,block,prism,shadow,property_value,rib_blank
from shared_edge import section_lift,routed_lead
from center_fit import shaft
from routing_review import cylinder
from shapely.geometry import Polygon, Point

BASE=HERE/'B-lower-support'/OLD_FILE
FILE='WristFlow-B-Review-R2.FCStd'
DZ=.45
RIM=.15
PORT_FACE=-17.82
GROUPS='Fixed Circuit Fasteners Cell Mated Lid Links Display Band CrownAssembly'.split()
EPS=.00005
def vec(z):return V(0,0,z)
def shifted(s,z):return s.translated(vec(z))
def hit(a,b):
    return max(0.,a.common(b).Volume) if a.BoundBox.intersect(b.BoundBox) else 0.
def gap(a,b):return a.distToShape(b)[0]
def clean(s):
    pieces=s.Solids
    return pieces[0] if len(pieces)==1 else Part.makeCompound(pieces)
def round_z(s,r):
    return s.makeFillet(r,[e for e in s.Edges if e.BoundBox.ZLength>.7])
def flat_section(s,z):
    faces=section(s,z).Faces
    return shadow(Part.makeCompound(faces))
def round_numbers(x):
    if isinstance(x,float):return round(x,5)
    if isinstance(x,dict):return {k:round_numbers(v) for k,v in x.items()}
    if isinstance(x,(tuple,list)):return [round_numbers(a) for a in x]
    return x
def upper_frame(s):return section_lift(section_lift(s,17.91,RIM),15.5,DZ)
def tool_cylinder(start,end,r):
    a,b=V(*start),V(*end);u=b-a
    return Part.makeCylinder(r,u.Length,a,u.normalize())
def rear_tool():
    # Thin straight unlocking blade from outside the 6 mm service gap to the back of the socket.
    u=V(0,17.74-23.89,10.245+4.6).normalize()
    n=V(0,u.z,-u.y)*.15
    a=V(3.2,23.89,-4.60)-u*4
    b=V(3.2,17.74,10.245)
    p=[a+V(-.60,0,0)+n,a+V(.60,0,0)+n,a+V(.60,0,0)-n,a+V(-.60,0,0)-n]
    return prism([tuple(v) for v in p],tuple(b-a))

def check_latch_space(obstacles):
    # Trial volume below the rear-opening actuator at the 6 mm service position.
    # No proprietary hinge geometry is assumed in this clearance test.
    envelope=block(-.50,16.70,6.48,7.40,1.95,3.80)
    for k,t in obstacles.items():
        if k!='RearSocket':assert hit(envelope,t)<EPS,('latch space',k)
    return {'trial_actuator_space_mm':[7.40,1.95,3.80],
            'actuator_space_to_open_FPC_mm':gap(envelope,obstacles['LidFlexNew'])}
def layout_geometry(s,report):
    orig=dict(s);frame=s['Frame']
    # Increase the retreat on all three nearby faces of the antenna recess.
    pocket=round_z(block(-15.86,-22.56,.89,18.72,3.68,12.40),.36)
    frame=frame.cut(pocket)
    frame=frame.cut(round_z(block(-15.86,-22.56,12.06,18.72,28.62,1.24),.36))
    # Crop only the inner nose of the left lug, preserving its spring-bar bore.
    s['LugLongL']=clean(s['LugLongL'].cut(block(-16,-22.58,2.1,6,1,5.15)))
    # Retain both original axes. Restore the missing rim at the counterbore.
    ret=s['Retainer']
    for cy in (-4.42,4.42):
        ear=shaft(15.47,.44,1.12,cy,8.72)
        ear=ear.cut(shaft(15.45,.50,.52,cy,8.72)).cut(shaft(15.45,.21,.74,cy,8.72))
        ret=ret.fuse(ear)
        frame=frame.cut(shaft(15.45,.46,1.18,cy,8.72))
    s['Retainer']=ret.removeSplitter()
    # Extend the moulded ledge over the spring noses to shorten the bending arm.
    ledge=block(14.13,-3.75,13.725,2.65,7.50,.45)
    frame=frame.fuse(ledge).removeSplitter()
    s['Frame']=upper_frame(frame)
    for k in ('Screen','ActiveArea','GlassTape','FrontRun'):s[k]=shifted(s[k],DZ)
    s['Crossing']=section_lift(s['Crossing'],11.68,DZ)
    for k in ('ALSPipe','ALSBaffle'):s[k]=upper_frame(s[k])
    for k in ('ALSLens','ALSSeal'):s[k]=shifted(s[k],DZ+RIM)
    # Move the motor in X, then restore FR4 in the liberated corner and protect the locating bore.
    s['Motor']=s['Motor'].translated(V(1,0,0))
    seat_blank=block(7.865,-21.2,10.4,12.05,10.40,.55)
    # The cutter crosses the curved outer corner of the case; use only the inboard connected volume.
    s['MotorSeat']=max(seat_blank.cut(s['Frame']).Solids,key=lambda q:q.Volume).removeSplitter()
    extra=block(6.20,-14.95,11.3,1.60,4.75,.80)
    arc=block(7.8,-10.90,11.30,.5,.5,.8).cut(cylinder(.50,.81,(8.30,-10.9,11.30)))
    board=s['Board'].fuse(extra).fuse(arc).cut(cylinder(1.24,1.50,(5.15,-9.80,11.05)))
    notch=round_z(block(12.20,-10.9,11.,1.8,1.15,2.0),.25)
    s['Board']=board.cut(notch).removeSplitter()
    parking=[];tools=[]
    for i,(ex,ey) in enumerate(((16.60,-7.40),(17.30,-9.10))):
        x=12.75+i*.70
        s['MotorLand'+str(i)]=block(ex-.38,ey-.50,11.24,.76,1,.06)
        points=[(x,-11.016,12.99),(x,-10.40,12.99),(x,-10.40,10.20),(ex,ey,10.20),(ex,ey,11.24)]
        active=routed_lead(points);s['MotorWire'+str(i)]=active
        def parked(end):return routed_lead([(x,-11.016,12.99),(x,-10.40,12.99),(x,-10.40,9.50),(x,end,9.50)])
        # The volume of the fixed-section lead is proportional to its developed length.
        end=-15.;delta=(active.Volume-parked(end).Volume)/(math.pi*.19**2)
        parked_wire=parked(end-delta)
        assert abs(parked_wire.Volume-active.Volume)<1e-5
        parking.append(parked_wire);tools.append(tool_cylinder((ex,ey,-1.6),(ex,ey,11.24),.65))
    def pcb_metrics(board):
        geom=flat_section(board,11.70)
        if geom.geom_type=='MultiPolygon':geom=max(geom.geoms,key=lambda q:q.area)
        axis=Point(5.15,-9.80)
        center_to_edge=geom.exterior.distance(axis)
        return {'hole_web':center_to_edge-1.24,'head_edge':center_to_edge-1.52,
                'bearing_area':section(board,11.30+.005).translated(vec(-.005)).common(
                    Part.Face(Part.Wire([Part.makeCircle(1.52,V(5.15,-9.80,11.30))]))).Area}
    report['board']={'old':pcb_metrics(orig['Board']),'new':pcb_metrics(s['Board']),
                    'net_area_gain':(s['Board'].Volume-orig['Board'].Volume)/.8}
    report['crown']={'hole_axes_y':[-4.42,4.42],'nominal_shank_diameter':.85,'frame_pilot_diameter':.86,
        'plate_width_at_fasteners_old_new':[10,11.08],'hole_to_outer_edge_old_new':[.06,1.12-.52],
        'counterbore_to_outer_edge_old_new':[-.16,1.12-.74],
        'head_seating_annulus_mm2':math.pi*(.72**2-.52**2),
        'seat_edge_to_spring_nose_old_new':[15.78-14.63,14.13-14.42],
        'radial_guide_play':.86-.80,'shaft_pull_take_up':gap(s['CrownShaft'],s['GuideSleeve']),
        'plate_battery_gap':gap(s['Retainer'],s['Battery']),'screws_battery_gap':gap(s['RetainerScrews'],s['Battery'])}
    outside_corner=next(f for f in orig['Frame'].Faces if f.Surface.__class__.__name__=='Cylinder'
        and f.BoundBox.XMin<-20.5 and f.BoundBox.YMin<-23.7)
    report['radio']={'installed_to_frame':[gap(orig['Antenna'],orig['Frame']),gap(s['Antenna'],s['Frame'])],
        'board_minus_5p9_to_lug':[gap(shifted(orig['ModuleSubstrate'],-5.9),orig['LugLongL']),
                                   gap(shifted(s['ModuleSubstrate'],-5.9),s['LugLongL'])],
        'lug_to_bore_ligament_at_shortened_end':(-22.58)-(-25.51),
        'lug_frame_root_overlap_mm3':[hit(orig['LugLongL'],orig['Frame']),hit(s['LugLongL'],s['Frame'])],
        'tip_to_outside_flat_wall':23.75-22.56,'corner_wall_old_new':[
            gap(outside_corner,Part.Vertex(V(-15.66,-22.36,7.))),gap(outside_corner,section(pocket,7.))],
        'original_antenna_under_host_projection_mm2':flat_section(orig['Board'],11.70).intersection(shadow(orig['Antenna'])).area,
        'new_antenna_under_host_projection_mm2':flat_section(s['Board'],11.70).intersection(shadow(s['Antenna'])).area}
    report['screen']={}
    for name,items in [('old',orig),('new',s)]:
        bounds=lambda key:items[key].BoundBox
        report['screen'][name]={'pcb_to_display':bounds('Screen').ZMin-bounds('Board').ZMax,
             'module_to_screen_flex':gap(items['ModuleParts'],items['FrontRun']),
             'glass_to_rim':bounds('Frame').ZMax-bounds('Screen').ZMax,
             'active_surface_to_rim':bounds('Frame').ZMax-bounds('ActiveArea').ZMax,
             'tape_thickness':bounds('GlassTape').ZLength,
             'seat_depth':bounds('Frame').ZMax-bounds('GlassTape').ZMin,
             'shell_through_optic_back':bounds('Frame').ZMax-bounds('Baffle').ZMin,
             'housing_rim_to_back_plate':bounds('Frame').ZMax-bounds('Back').ZMin,
             'ALS_sensor_to_guide':gap(items['ALSPipe'],items['AmbientALS'])}
    return orig,parking,tools


def rear_geometry(s):
    # Keep both frame shelf and tray referenced to the shifted lower fastener.
    x=12.50; y0=-21.25; y=-21.45
    def ring_at(y,z,r,h):return cylinder(r,h,(x,y,z))
    lower=ring_at(y0,1.90,.60,.40)
    tray=s['Tray'].cut(ring_at(y0,2.30,.821,.36)).fuse(lower)
    tray=tray.fuse(ring_at(y,1.90,.98,.40)).fuse(ring_at(y,2.30,.82,.35))
    trim=[(-22.60,2.04),(-22.43,2.04),(-22.145,2.13),(-22.145,2.651),(-22.60,2.651)]
    tray=tray.cut(prism([(10.70,a,b) for a,b in trim],(3.60,0,0))).cut(ring_at(y,1.85,.60,1))
    # Increase the adhesive allocation without raising the cell, separator or thermistor.
    tray=tray.cut(block(-17.11,-20.51,2.25,32.22,30.72,.055))
    s['Tray']=tray.removeSplitter()
    # Relieve just the bottom of the opposite lug where the shifted tray ear passes it.
    bevel=[(-22.425,2.10),(-22.425,2.49),(-22.225,2.90),(-21.9,2.90),(-21.9,2.10)]
    s['LugLongR']=s['LugLongR'].cut(prism([(11.1,a,b) for a,b in bevel],(3.5,0,0))).removeSplitter()
    s['CellTape']=Part.makeCompound([block(a,-14.5,2.25,3.0,19,.10) for a in (-12.5,7.)])
    s['TrayScrew1']=s['TrayScrew1'].translated(V(0,-.20,0))
    frame=s['Frame'].fuse(ring_at(y0,2.65,.60,1.05))
    frame=frame.cut(ring_at(y,.90,1.09,1.75)).cut(ring_at(y,2.55,.60,2.00))
    for k in ('Lead0','Lead2','NTCReturn'):
        s[k]=s[k].cut(s['Battery']).removeSplitter()
    # Use a straight, side-accessible frame duct and shift the two open ends
    # of the removable soft gasket relative to each other. The airway clears
    # the fastener and preserves its nominal area and the original exterior hole.
    acoustic_air=block(-19.98,14.97,3.387,2.21,3.10,.616)
    undercut=block(PORT_FACE-.14,13.92,.88,.62,4.62,2.32).cut(cylinder(1.13,4,(-19,14,.5)))
    frame=frame.cut(undercut)
    collar=block(-19.96,13.80,3.03,PORT_FACE+19.96,4.99,1.31).cut(cylinder(.75,3.54,(-19,14,0)))
    ramp=prism([(PORT_FACE-.21,13.7,3.02),(PORT_FACE+.01,13.7,3.02),(PORT_FACE+.01,13.7,3.20)],(0,5.2,0))
    s['Frame']=frame.fuse(collar.cut(ramp)).cut(acoustic_air).removeSplitter()
    def loft_port(stations):
        return Part.makeLoft([Part.makePolygon([V(x,y,z) for y,z in
            [(a,c),(b,c),(b,e),(a,e),(a,c)]]) for x,a,b,c,e in stations],True,True)
    jacket=loft_port([(-16.4,14.025,17.975,3.22,4.18),(PORT_FACE,14.67,18.37,3.20,4.19)])
    throat=loft_port([(-16.4,14.325,17.675,3.41,3.98),(PORT_FACE,14.97,18.07,3.387,4.003)])
    s['Outlet']=jacket.cut(throat).removeSplitter()
    relief=round_z(block(-21.,13.40,11,3.42,5.80,2),.35)
    s['Board']=s['Board'].cut(relief).removeSplitter()


def motor_seat(s):
    blank=s['MotorSeat']
    ribs=[]
    for y in (-16.10,-18.45):
        prof=[(15.55,10.40),(19.25,10.40),(19.25,9.62),(18.30,9.62),(15.55,10.18)]
        ribs.append(prism([(a,y,b) for a,b in prof],(0,.78,0)))
    seat=blank.multiFuse(ribs).cut(s['Frame']).removeSplitter()
    s['MotorSeat']=seat.cut(cylinder(4.55,.11,(13.10,-16,10.85))).removeSplitter()
    s['MotorTape']=cylinder(4.50,.10,(13.10,-16,10.85))


def evaluate(s,old,parking,tools,r):
    for k,sh in s.items():assert sh.isValid() and len(sh.Solids)>0,k
    def dist(a,b):return gap(s[a],s[b])
    # Compare only changed installed members; the two band roots have deliberate unions with the frame.
    changed=[k for k,sh in s.items() if k not in old or sh is not old[k]]
    issues={}
    for k in changed:
        for j in s:
            if k==j or j in changed and j<k or not s[k].BoundBox.intersect(s[j].BoundBox):continue
            v=hit(s[k],s[j])-(hit(old[k],old[j]) if k in old and j in old else 0)
            if v>EPS and {k,j} not in ({'Motor','MotorWire0'},{'Motor','MotorWire1'}):issues[k+'/'+j]=v
    assert not issues,issues
    assert all(dist(a,b)<1e-6 for a,b in [('CrownBridge','Frame'),('Retainer','Daughter'),
        ('RetainerScrews','Retainer'),('GlassTape','Frame'),('GlassTape','Screen'),('FrontRun','Crossing'),
        ('Crossing','RearRun'),('ALSPipe','ALSLens'),('ALSSeal','Frame'),('MotorTape','MotorSeat'),
        ('MotorTape','Motor'),('CellTape','Battery'),('CellTape','Tray'),('Outlet','Speaker')]),'missing seat contact'
    for i in range(4):
        assert dist('CrownLeaf'+str(i),'CrownPad'+str(i))<EPS
    # Spring noses project onto a coplanar frame surface directly behind the contact board.
    shelf=flat_section(s['Frame'],13.727)
    contact=0
    for i in range(4):
        nose=flat_section(s['CrownLeaf'+str(i)],13.099)
        assert nose.difference(shelf).area<EPS
        contact+=nose.area
    r['crown']['nose_support_area_mm2']=contact
    # Test the two lateral straight driver entries. Start 12 mm inside the crown head plane.
    for cy in (-4.42,4.42):
        driver=shaft(3.46,12.,.8,cy,8.72)
        assert all(hit(driver,s[k])<EPS for k in s if k!='RetainerScrews'),cy
    r['crown']['driver_envelope_diameter_mm']=1.60
    r['crown']['cap_to_sleeve_after_0p25_press']=gap(s['Crown'].translated(V(-.25,0,0)),s['GuideSleeve'])
    r['crown']['cap_first_guide_stop_approx_mm']=.33
    for k in ['Crown','CrownShaft','SwitchFollower']:
        pressed=s[k].translated(V(-.25,0,0))
        for j,t in s.items():
            if j not in ('Crown','CrownShaft','SwitchFollower'):assert hit(pressed,t)<EPS,(k,j)
    sweep=section(s['Board'],11.70).translated(vec(.4)).extrude(vec(-16))
    for k in ['Frame','MotorSeat','Motor','LugLongL','CrownBridge','Retainer','Daughter']:
        assert hit(sweep,s[k])<EPS,k
    for i,p in enumerate(parking):
        assert hit(p,sweep)<EPS and gap(p,sweep)>.20
        for k,t in s.items():
            if k not in ('Motor','MotorWire0','MotorWire1'):assert hit(t,p)<EPS,('park',k)
    assert gap(*parking)>0
    # Full continuous 16 mm removal for the loaded PCB, including pads and module, with wires parked.
    obstacles=[s[k] for k in ('Frame','Motor','MotorSeat','LugLongL','LugLongR','CrownBridge','Daughter','Retainer')]+parking
    for k in ('ModuleParts','ModuleSubstrate','Antenna','MotorLand0','MotorLand1','AmbientALS','RearSocket',
              'CrownLeaf0','CrownLeaf1','CrownLeaf2','CrownLeaf3'):
        bb=s[k].BoundBox
        env=block(bb.XMin,bb.YMin,bb.ZMin-16,bb.XLength,bb.YLength,bb.ZLength+16)
        for o in obstacles:assert hit(env,o)<EPS,('removal',k,str(o.BoundBox))
    for i,t in enumerate(tools):
        exclude={'MotorLand'+str(i),'MotorWire'+str(i),'Back','Baffle','Optic0','Optic1','Optic2',
                 'PpgMount','LidFlexNew','LidBond','SpeakerBed','PpgStiffener','PpgStiffAdhesive','OpticalDice'}
        for k in s.keys()-exclude:assert hit(s[k],t)<EPS,('tool',i,k)
    ruler=lambda k:min(gap(t,s[k]) for t in tools)
    r['motor']={'center':[13.1,-16.0],'displacement_x':1.0,'motor_to_right_wall_excluding_integral_seat':dist('Motor','Frame'),
        'motor_to_rib':gap(s['Motor'],rib_blank()),'motor_to_board':dist('Motor','Board'),
        'motor_to_antenna':dist('Motor','Antenna'),'seat_to_board':dist('MotorSeat','Board'),
        'tool_to_tray':ruler('Tray'),'tool_to_rear_FPC_closed':ruler('LidFlexNew'),
        'wire_to_wire':dist('MotorWire0','MotorWire1'),'parked_wires_to_removal':min(gap(p,sweep) for p in parking),
        'tape_thickness':.10,'tape_recess_depth':.10,'frame_integral_seat':True}
    alt=s['Motor'].copy()
    alt.rotate(V(13.1,-16,12.3),V(0,1,0),180)
    low=shifted(alt,-3.25);alt=shifted(alt,-2.70)
    upper=max(block(7.865,-21.2,10.95,12.05,10.40,.55).cut(s['Frame']).Solids,key=lambda q:q.Volume)
    bottom=shifted(upper,-3.25)
    r['motor_flip_trial']={'trial_z':[alt.BoundBox.ZMin,alt.BoundBox.ZMax],'thickness_delta':0,
        'direct_flip_insulation_intersection_mm3':hit(low,s['Separator']),
        'low_under_motor_bracket_insulation_intersection_mm3':hit(bottom,s['Separator']),
        'upper_trial_bracket_to_board_removal':gap(upper,sweep),'upper_trial_bracket_to_frame':gap(upper,s['Frame']),
        'motor_to_cell_insulation':gap(alt,s['Separator']),'motor_to_rear_FPC':gap(alt,s['LidFlexNew'])}
    for k,t in s.items():
        if k in ('Motor','MotorSeat','MotorTape','MotorWire0','MotorWire1'):continue
        assert hit(alt,t)<EPS and hit(upper,t)<EPS,('alternate',k)
    # Volumes of the two ears above the tray deck; the flat adhesive surface has a separate measurement.
    def ears(parts):
        return [gap(parts['Battery'],clean(parts['Tray'].common(b))) for b in (
            block(10,-23,2.301,4.8,2.6,.45),block(-19.2,9,2.301,2.6,3.3,.45))]
    def floor_gap(parts):
        centre=parts['Tray'].common(block(-.1,-.1,1.5,.2,.2,1.0))
        return parts['Battery'].BoundBox.ZMin-centre.BoundBox.ZMax
    r['battery']={'ear_gaps_old':ears(old),'ear_gaps_new':ears(s),'body_to_tray_floor_old_new':[
        floor_gap(old),floor_gap(s)],'min_all_tray_surfaces_old_new':[gap(old['Tray'],old['Battery']),dist('Tray','Battery')],
        'body_to_lower_screw_old_new':[gap(old['TrayScrew1'],old['Battery']),dist('TrayScrew1','Battery')],
        'body_to_frame':dist('Battery','Frame'),'tray_to_lug':dist('Tray','LugLongR'),
        'NTC_to_board':dist('SurfaceNTC','Board'),'tray_to_stiffener':dist('Tray','PpgStiffener'),
        'cable_to_battery_intrusion_mm3':{k:hit(s[k],s['Battery']) for k in ('Lead0','Lead2','NTCReturn')}}
    for j in range(13):
        tray=shifted(s['Tray'],-j/2)
        for k in ('LugLongR','Frame'):assert hit(tray,s[k])<EPS,('tray withdrawal',k,j)
    # Extrude the planar outline of the outlet across the entire opening trajectory.
    outlet=s['Outlet']
    def mouth_plane(x):
        return Part.Face(Part.makePolygon([V(x,y,z) for y,z in
            [(12.5,2.25),(19.5,2.25),(19.5,5.25),(12.5,5.25),(12.5,2.25)]]))
    seal_plane=mouth_plane(PORT_FACE)
    seal_area=outlet.common(seal_plane).common(s['Frame'].common(seal_plane)).Area
    assert abs(seal_area-(3.70*.99-3.10*.616))<EPS
    for x in [-19.95,-19.80,-19.50,-19.25,-19.,-18.75,-18.50,-18.25,-18.,PORT_FACE-.01]:
        channel=[q for q in mouth_plane(x).cut(s['Frame']).Faces if q.isInside(V(x,16,3.70),1e-7,True)]
        assert len(channel)==1 and abs(channel[0].Area-3.10*.616)<EPS,x
    # Outlet has multiple wall cross sections. Sweep each of the actual solid's XY projection polygons.
    outline=shadow(outlet)
    swept=prism([(x,y,-2.80) for x,y in list(outline.exterior.coords)[:-1]],(0,0,6.99))
    assert hit(swept,s['Frame'])<EPS
    rearprobe=rear_tool()
    obstacles={k:t for k,t in s.items() if k not in REAR and not k.startswith('CoverScrew')}
    for k in REAR:
        if k.startswith('CoverScrew'):continue
        if k=='LidFlexNew':obstacles[k]=OPEN_FLEX
        elif k in ('RearFingers','LidEnd'):obstacles[k]=s[k]
        else:obstacles[k]=shifted(s[k],-6)
    for k,t in obstacles.items():
        if k!='RearSocket':assert hit(t,rearprobe)<EPS,('zif approach',k,hit(t,rearprobe))
    r['sound']={'closed_overlap_old_new':[hit(old['Outlet'],old['Frame']),hit(outlet,s['Frame'])],
         'closed_gap_old_new':[gap(old['Outlet'],old['Frame']),gap(outlet,s['Frame'])],
         'open_1_overlap_old_new':[hit(shifted(old['Outlet'],-1),old['Frame']),hit(shifted(outlet,-1),s['Frame'])],
         'open_1_new_gap':gap(shifted(outlet,-1),s['Frame']),'service_sweep_0_to_6_overlap':hit(swept,s['Frame']),
         'cover_screw_closed_overlap_old_new':[hit(old['Outlet'],old['CoverScrew1']),hit(outlet,s['CoverScrew1'])],
         'cover_screw_gap':dist('Outlet','CoverScrew1'),'closed_mating_ring_mm2':seal_area,
         'speaker_end_aperture_mm':[3.35,.57],'fixed_straight_duct_x_section_mm':[3.10,.616],
         'installed_gasket_length_mm':outlet.BoundBox.XLength,
         'open_6_blade_mm':[1.2,.30],
         'blade_to_frame':gap(rearprobe,s['Frame']),'blade_to_lid':gap(rearprobe,obstacles['Back']),
         'blade_to_socket':gap(rearprobe,s['RearSocket']),'blade_to_fold':gap(rearprobe,OPEN_FLEX)}
    r['sound'].update(check_latch_space(obstacles))
    return alt,upper,rearprobe


def make_geometry(d):
    global REAR,OPEN_FLEX
    REAR={o.Name for o in d.Lid.Group}
    OPEN_FLEX=d.Open6LidFlexNew.Shape
    s={o.Name:o.Shape.copy() for name in GROUPS+['IsolationStudy'] for o in d.getObject(name).Group}
    report={}
    old,parking,tools=layout_geometry(s,report)
    board_area_before_port=s['Board'].Volume/.8
    rear_geometry(s)
    report['board']['port_relief_mm2']=board_area_before_port-s['Board'].Volume/.8
    report['board']['net_area_gain']=(s['Board'].Volume-old['Board'].Volume)/.8
    motor_seat(s)
    alt,upper,probe=evaluate(s,old,parking,tools,report)
    return s,old,report,parking,alt,upper,probe


def run(dest):
    dest=Path(dest);d=App.openDocument(str(BASE))
    App.ParamGet('User parameter:BaseApp/Preferences/Document').SetInt('CountBackupFiles',0)
    s,old,report,parking,alt,upper,probe=make_geometry(d)
    def put(name,label,shape,group,color=None):
        o=d.getObject(name)
        if o is None:
            o=d.addObject('Part::Feature',name);group.addObject(o)
        o.Shape=shape
        if label:o.Label=label
        if color:o.ViewObject.ShapeColor=color
        return o
    def group(name,label):
        o=d.addObject('App::DocumentObjectGroup',name);o.Label=label;return o
    for k in old:
        if s[k] is not old[k]:put(k,None,s[k],None)
    mount=s['Frame'].fuse(s['MotorSeat']).removeSplitter()
    assert len(mount.Solids)==1 and mount.isValid()
    d.Frame.Shape=mount
    d.Frame.Label='R2 中框 · 承屏台阶、表冠承台、下板边托筋及一体电机托架'
    d.Board.Label='R2 主板 · 下固定孔补板、后盖侧电机焊盘'
    d.Retainer.Label='表冠压片 · 两端完整沉孔承压耳'
    d.Tray.Label='电芯托板 · 安装耳移位、0.10 mm 胶位'
    put('CellTape','电池托板胶 · 两条，厚 0.10 mm 试放',s['CellTape'],d.Cell,(.75,.69,.43))
    put('MotorTape','电机固定胶 · Ø9 × 0.10 mm，嵌入中框托架',s['MotorTape'],d.Fixed,(.82,.72,.50))
    for i in range(2):
        d.getObject('MotorWire'+str(i)).Label='电机引线 '+str(i+1)+' · 绕缺口至后盖侧'
        d.getObject('MotorLand'+str(i)).Label='主板后盖侧 · 电机焊盘 '+str(i+1)
    detail=group('ReviewGeometry','分析用局部 · 单独查看')
    d.Fixed.removeObject(d.MotorSeat);detail.addObject(d.MotorSeat)
    d.MotorSeat.Label='局部显示 · 已并入中框的电机托架'
    for i,wire in enumerate(parking):put('ParkedMotorWire'+str(i),'拆板 · 脱焊并将等长引线停到托架下',wire,detail,(.70,.47,.26))
    put('ZIFProbe','开盖 6 mm · 锁扣入口薄片试具',probe,detail,(.41,.56,.65))
    trial=group('RearMotorTrial','比较方案 · 电机本体翻向电池（单独显示整组）')
    put('TrialRearMotor','电池侧电机 · 上抬后的比较位置',alt,trial,(.60,.46,.70))
    put('TrialUpperMotorSeat','比较方案 · 上托架占位，需重新固定',upper,trial,(.65,.59,.70))
    for k in ('Frame','Board','Battery','Separator','Tray'):
        put('Trial'+k,'比较方案 · '+d.getObject(k).Label,s[k],trial,d.getObject(k).ViewObject.ShapeColor[:3])
    color=d.Frame.ViewObject.ShapeColor[:3]
    def frame_surface(f):
        b=f.BoundBox
        if b.XMin>7.7 and b.XMax<18.85 and b.YMax<-10.7 and b.ZMin>9.60 and b.ZMax<10.96:return (.93,.61,.23)
        if b.XMin>4.0 and b.XMax<6.3 and b.YMax<-10.8 and b.ZMin>12.09 and b.ZMax<14.25:return (.96,.68,.33)
        return color
    d.Frame.ViewObject.DiffuseColor=[frame_surface(f) for f in mount.Faces]
    for obj,txt in [(d.Tray,'LP503030 32 × 30.5 × 5.3 mm。固定胶按压合厚度建模，包体膨胀、出线、胶层公差待实物。'),
        (d.Outlet,'框侧孔下缘局部退让。导音软垫的实际压缩密封、沿盖拆出和复位待试件。'),
        (d.CrownBridge,'四个弹片触顶投影均有上方中框承台。名义压合和实际各路稳定导通分别验收。'),
        (d.Frame,'屏支承向外抬高 0.45 mm，外沿另加 0.15 mm。下端托筋及电机托架均属中框实体。')]:
        property_value(obj,'App::PropertyString','RevisionNote',txt)
    s['Frame']=mount
    # Rebuild snapshot cuts inherited from earlier models. Source labels follow the current assembly.
    slabs=[(d.ViewCut,'Cut',block(-4.225,-40,-4,.45,80,30)),
           (d.CrownSection,'AX',block(-24,-.07,-4,50,.14,30)),
           (d.SocketSection,'BX',block(-.30,15.89,-4,7,.06,30))]
    for grp,prefix,slab in slabs:
        names={o.Name[len(prefix):] for o in grp.Group}|{'Frame','Board','LidFlexNew','Screen','CellTape'}
        for k in names:
            sh=s[k].common(slab) if k in s else None
            o=d.getObject(prefix+k)
            if sh is None or not sh.Solids or sh.Volume<.000001:
                if o: d.removeObject(o.Name)
            else:
                put(prefix+k,'剖 · '+d.getObject(k).Label,clean(sh),grp,d.getObject(k).ViewObject.ShapeColor[:3])
    put('Open6Outlet','开盖 6 mm · 导音软垫',shifted(s['Outlet'],-6),d.LidService6,(.86,.61,.33))
    d.LidService6.Visibility=False;detail.Visibility=False;trial.Visibility=False
    for gp,_,_ in slabs:gp.Visibility=False
    for r in d.RootObjects:
        if hasattr(r,'Visibility'):r.Visibility=False
    shown={'Frame','ModuleSubstrate','ModuleParts','Antenna','Board','Clamp1','Clamp2','Clamp3','Motor','MotorWire0',
           'MotorWire1','Crown','Daughter','CrownBridge','EncoderBody','LugLongL','LugLongR','LugShortL','LugShortR'}
    for name in ('Fixed','Circuit','Fasteners','CrownAssembly','Band'):
        gp=d.getObject(name);gp.Visibility=True
        for obj in gp.Group:obj.Visibility=obj.Name in shown
    import FreeCADGui as Gui
    Gui.activeDocument().activeView().viewTop();Gui.activeDocument().activeView().fitAll()
    d.Label='WristFlow B · 审查修订 R2'
    d.recompute();fp=dest/FILE;d.saveAs(str(fp));App.closeDocument(d.Name)
    d=App.openDocument(str(fp))
    for k in ['Frame','Board','Retainer','Tray','Outlet','LugLongL','LidFlexNew','MotorSeat','TrialRearMotor']:
        assert d.getObject(k).Shape.isValid() and len(d.getObject(k).Shape.Solids)==1,k
    assert abs(d.Frame.Shape.Volume-mount.Volume)<EPS and d.TrialRearMotor.Visibility==False
    assert len([o for o in d.Circuit.Group if o.Name.startswith('CrownLeaf')])==4
    assert len([o for o in d.CrownAssembly.Group if o.Name.startswith('CrownPad')])==4
    d.Board.Shape.exportStep(str(dest/'WristFlow-B-R2-PCB.step'))
    # Native FreeCAD screenshot of the cropped loaded assembly. The cut removes
    # only display occlusion so the lower clamp, motor and new edge can be read together.
    crop=block(2.9,-23.6,9.5,17.65,16.60,5.27)
    native_parts={}
    for key in ('Frame','Board','Motor','MotorWire0','MotorWire1','Clamp1','MotorLand0','MotorLand1'):
        source=d.getObject(key);chunk=source.Shape.common(crop)
        if chunk.Volume>1e-7:native_parts[key]=(clean(chunk),source.ViewObject.ShapeColor[:3])
    App.closeDocument(d.Name);Gui.updateGui()
    local=App.newDocument('MotorInspection')
    for key,(chunk,col) in native_parts.items():
        o=local.addObject('Part::Feature','Detail'+key);o.Shape=chunk
        o.ViewObject.ShapeColor=col;o.ViewObject.DisplayMode='Flat Lines'
    local.recompute();App.setActiveDocument(local.Name);Gui.updateGui()
    view=Gui.getDocument(local.Name).activeView()
    view.setCameraOrientation(App.Rotation(V(1,0,0),-19).multiply(App.Rotation(V(0,0,1),-10)),False)
    view.fitAll();Gui.updateGui();view.saveImage(str(dest/'motor-native.png'),1040,800,'White')
    App.closeDocument(local.Name)
    (dest/'review-check.json').write_text(json.dumps(round_numbers(report),ensure_ascii=False,indent=2),encoding='utf8')
    (dest/'reopened.ok').write_text('ok')
