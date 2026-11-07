"""FreeCAD-native A0 study. Run through run.py with an installed FreeCADCmd.

Imported community geometry stays in local outputs. Native Part/Spreadsheet
features keep their dependency graph without a custom Python proxy on reopening.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import traceback
import builtins
from zipfile import ZipFile

import FreeCAD as App
import Mesh
import Part


VISIBLE = []


def native(doc, kind, name, label=None):
    obj = doc.addObject(kind, name)
    obj.Label = label or name
    return obj


def note(obj, text):
    obj.addProperty('App::PropertyString', 'EvidenceNote', 'Evidence')
    obj.EvidenceNote = text


def box(doc, name, lx, ly, hz, x, y, z):
    obj = native(doc, 'Part::Box', name)
    for prop, value in [('Length', lx), ('Width', ly), ('Height', hz),
                        ('Placement.Base.x', x), ('Placement.Base.y', y),
                        ('Placement.Base.z', z)]:
        obj.setExpression(prop, value)
    return obj


def cylinder(doc, name, radius, height, x, y, z, along_x=False):
    obj = native(doc, 'Part::Cylinder', name)
    for prop, value in [('Radius', radius), ('Height', height),
                        ('Placement.Base.x', x), ('Placement.Base.y', y),
                        ('Placement.Base.z', z)]:
        obj.setExpression(prop, value)
    if along_x:
        obj.Placement.Rotation = App.Rotation(App.Vector(0, 0, 1), App.Vector(1, 0, 0))
    return obj


def fuse(doc, name, shapes):
    obj = native(doc, 'Part::MultiFuse', name)
    obj.Shapes = shapes
    obj.Refine = True
    return obj


def cut(doc, name, base, tool):
    obj = native(doc, 'Part::Cut', name)
    obj.Base, obj.Tool = base, tool
    obj.Refine = True
    return obj


def rounded(doc, name, w, l, r, h, cx, cy, z):
    """Analytic rounded rectangle from native, expression-driven primitives."""
    members = [
        box(doc, name+'BarX', f'({w})-2*({r})', l, h,
            f'({cx})-({w})/2+({r})', f'({cy})-({l})/2', z),
        box(doc, name+'BarY', w, f'({l})-2*({r})', h,
            f'({cx})-({w})/2', f'({cy})-({l})/2+({r})', z),
    ]
    for i, (sx, sy) in enumerate([(-1,-1),(-1,1),(1,-1),(1,1)]):
        members.append(cylinder(doc, name+'Corner'+str(i), r, h,
            f'({cx})+{sx}*(({w})/2-({r}))',
            f'({cy})+{sy}*(({l})/2-({r}))', z))
    return fuse(doc, name, members)


def frame(doc, name, w, l, cx, cy, top_z):
    outer = rounded(doc, name+'Outer', f'({w})+2*(P.FitXY+P.FrameWall)',
                    f'({l})+2*(P.FitXY+P.FrameWall)', '1.2 mm',
                    'P.FrameHeight', cx, cy, f'({top_z})-P.FrameHeight')
    inner = rounded(doc, name+'Void', f'({w})+2*P.FitXY', f'({l})+2*P.FitXY',
                    '0.5 mm', 'P.FrameHeight+2 mm', cx, cy,
                    f'({top_z})-P.FrameHeight-1 mm')
    return cut(doc, name, outer, inner)


def outline_face(data, tx, ty):
    edges, start = [], None
    for command in data:
        if command[0] == 'M':
            start = App.Vector(command[1]*.0254+tx, command[2]*.0254+ty, 0)
            continue
        end = App.Vector(command[-2]*.0254+tx, command[-1]*.0254+ty, 0)
        if command[0] == 'L':
            edges.append(Part.makeLine(start, end))
        else:
            sweep = math.radians(command[1])
            delta = end-start
            chord = delta.Length
            centre = (start+end)*.5 + App.Vector(-delta.y, delta.x, 0)*(1/(2*math.tan(sweep/2)))
            v = start-centre
            mid = centre + App.Vector(v.x*math.cos(sweep/2)-v.y*math.sin(sweep/2),
                                      v.x*math.sin(sweep/2)+v.y*math.cos(sweep/2), 0)
            edges.append(Part.Arc(start, mid, end).toShape())
        start = end
    face = Part.Face(Part.Wire(edges))
    if not face.isValid():
        raise ValueError('Invalid official V1.2 outline reconstruction')
    return face


def add_board_references(doc, cfg):
    p = cfg['parameters']
    tx, ty = p['RigidFitTX'][0], p['RigidFitTY'][0]
    face = outline_face(cfg['board_reference']['outline'], tx, ty)
    core_face = face.copy()
    for x, y in cfg['board_reference']['screen_mounts_mm']:
        hole = Part.Face(Part.Wire([Part.makeCircle(p['ReferenceCoreHoleR'][0], App.Vector(x+tx,y+ty,0))]))
        core_face = core_face.cut(hole)
    # Core TOP faces the rear: the physical flip reverses X in this view.
    # The symmetric hole pattern alone cannot determine this orientation.
    mirror = App.Matrix()
    mirror.A11 = -1
    mirror.A14 = 2*(17.555+tx)
    core_face = core_face.transformGeometry(mirror)
    corebase = native(doc, 'Part::Feature', 'CoreOutlineReference')
    corebase.Shape = core_face
    core = native(doc, 'Part::Extrusion', 'CorePCBReference', 'V1.2主板参照（非V1.1实测）')
    core.Base, core.Solid = corebase, True
    core.setExpression('Dir.z', 'P.CorePCBThickness')
    core.setExpression('Placement.Base.z', 'P.PCBRearZ')
    screenbase = native(doc, 'Part::Feature', 'ScreenOutlineReference')
    screenbase.Shape = face
    screen = native(doc, 'Part::Extrusion', 'ScreenPCBReference', 'V1.2屏幕板参照（孔未当实测钻孔）')
    screen.Base, screen.Solid = screenbase, True
    screen.setExpression('Dir.z', 'P.ScreenPCBThickness')
    screen.setExpression('Placement.Base.z', 'P.PCBRearZ+P.CorePCBThickness+P.BoardGap')
    note(core, 'Official V1.2 geometry, rigid translation only; retained RMS0.240279mm mismatch to community axes.')
    note(screen, 'User confirmed screen-board fixing role. Hole/contact Z of actual assembly still unverified.')
    columns=[]
    for i, (x,y) in enumerate(cfg['board_reference']['screen_mounts_mm']):
        col=cylinder(doc, 'InterboardSupport'+str(i), 'P.PostOD/2', 'P.BoardGap',
                     f'{x+tx} mm', f'{y+ty} mm', 'P.PCBRearZ+P.CorePCBThickness')
        note(col,'Measured board gap with reference-only OD4.35; no outward golden post in enclosure assembly.')
        columns.append(col)
    return core, screen, columns


def core_world_xy(cfg, x, y):
    """V1.2 core TOP coordinates -> screen/STL registration, without scaling."""
    p = cfg['parameters']
    return (2*17.555-(x-.127)+p['RigidFitTX'][0],
            y+.127+p['RigidFitTY'][0])


def add_sheet(doc, cfg, variant):
    sheet = native(doc, 'Spreadsheet::Sheet', 'P', '参数：来源／假设分开，草案禁止打印')
    sheet.set('A1','Parameter'); sheet.set('B1','Value (mm)')
    sheet.set('C1','Evidence class'); sheet.set('D1','Note')
    values = dict(cfg['parameters'])
    v=cfg['variants'][variant]
    for key in ['CaseWidth','CaseLength','BossOffsetX','BossY']:
        values[key]=[v[key],'design proposal',f'Variant {variant}']
    values.update({
        'BatteryCX':[values['CaseCenterX'][0],'derived layout','Battery centre'],
        'BatteryCY':[0,'expression','Top edge minus half width'],
        'SpeakerCX':[0,'expression','Variant dependent'],
        'SpeakerCY':[0,'expression','Below battery with gap'],
        'MotorCX':[0,'expression','Variant dependent'],
        'MotorCY':[0,'expression','Variant dependent'],
        'ComponentFrontZ':[0,'expression','Preserved original bulkhead plus provisional pad'],
        'MotorFrontZ':[0,'expression','Independent motor mounting level'],
        'MotorCarrierTopZ':[0,'expression','Stacked variant only'],
        'OpeningZ':[0,'expression','Open rear face before removable lid'],
        'GlassDatumZ':[0,'expression / assumption','PCBRearZ candidate plus measured A; not physical source-mesh glass registration'],
        'VentZ':[0,'expression','Sound path proposal; acoustics not validated']
    })
    rows={}
    for row,(key,(value,kind,description)) in enumerate(values.items(),2):
        rows[key]=row
        sheet.set('A'+str(row),key); sheet.set('B'+str(row),'='+str(value)+' mm')
        sheet.setAlias('B'+str(row),key)
        sheet.set('C'+str(row),kind); sheet.set('D'+str(row),description)
    expressions={
        'BossOffsetX':'=CaseWidth/2-Wall-BossRadius/2',
        'BatteryCY':'=BatteryTopY-BatteryW/2',
        'SpeakerCY':'=BatteryTopY-BatteryW-PartGap-SpeakerW/2',
        'SpeakerCX':'=CaseCenterX-(MotorD+PartGap)/2' if variant=='A' else '=CaseCenterX',
        'MotorCX':'=SpeakerCX+SpeakerL/2+PartGap+MotorD/2' if variant=='A' else '=SpeakerCX+8 mm',
        'MotorCY':'=SpeakerCY' if variant=='A' else '=SpeakerCY+3.5 mm',
        'ComponentFrontZ':'=OriginalFloorOuterZ-PadThickness',
        'MotorCarrierTopZ':'=ComponentFrontZ-SpeakerH-MotorGap',
        'MotorFrontZ':'=ComponentFrontZ' if variant=='A' else '=MotorCarrierTopZ-MotorCarrier-0.3 mm',
        'OpeningZ':'=min(ComponentFrontZ-BatteryH;ComponentFrontZ-SpeakerH;MotorFrontZ-MotorH)-CoverGap',
        'GlassDatumZ':'=PCBRearZ+StackA',
        'VentZ':'=ComponentFrontZ-SpeakerH-0.4 mm' if variant=='B' else '=(ComponentFrontZ-SpeakerH+OpeningZ)/2',
    }
    for key,expr in expressions.items(): sheet.set('B'+str(rows[key]),expr)
    sheet.setColumnWidth('A',205); sheet.setColumnWidth('B',110)
    sheet.setColumnWidth('C',170); sheet.setColumnWidth('D',650)
    doc.recompute()
    return sheet


def build_variant(cfg, cap_shape, mesh_paths, out, variant):
    global VISIBLE
    doc=App.newDocument('WristFlow_A0_'+variant)
    doc.Label='WristFlow A0 '+variant+' — 草案／禁止直接打印'
    meta=native(doc,'App::FeaturePython','DraftEvidence','装配候选；不是实物验收')
    for prop,value in {
        'ReleaseState':'DRAFT_NOT_FOR_PRINTING',
        'SourceBoard':'Official V1.2; physical unit is user-reported V1.1',
        'Registration':'PCB rear uses original cap post-top Z as hypothesis; no physical Z proof',
        'GoldPosts':'Not used in original enclosure per user; replacement design authorized, no physical removal performed',
        'FixingRole':'Screen-board fixing system confirmed by user; inter-board supports and FPC retained',
        'ValidationScope':'Numeric report must be regenerated after changing parameters; connector/FPC/strap/temperature fit unverified'
    }.items():
        meta.addProperty('App::PropertyString',prop,'Evidence'); setattr(meta,prop,value)
    p=add_sheet(doc,cfg,variant)
    orig=native(doc,'Part::Feature','OriginalFixingCap','原底盖固定结构／保留隔板（本机来源）')
    orig.Shape=cap_shape
    note(orig,'User confirmed screen-board fixing role; cap source unchanged, private local reference only.')
    refs=[]
    for i,(label,path) in enumerate(mesh_paths.items()):
        obj=native(doc,'Mesh::Feature','ReferenceMesh'+str(i),label+'（参照）')
        obj.Mesh=Mesh.Mesh(str(path)); refs.append(obj)
    core,screen,columns=add_board_references(doc,cfg)
    glass=box(doc,'GlassDatumPlaceholder','35.111 mm','41.478 mm','0.2 mm',
              'P.CaseCenterX-35.111 mm/2','20.70537 mm-41.478 mm/2','P.GlassDatumZ-0.2 mm')
    glass.Label='玻璃前沿候选：XY非实测，仅示高度'
    note(glass,'Not a measured glass outline, thickness or seating model.')
    # Retain the original cap and use its rear plane as a structural bulkhead.
    front='P.OriginalFloorOuterZ+P.FlangeOverlap'
    height=f'({front})-P.OpeningZ'
    outer=rounded(doc,'RearOuter','P.CaseWidth','P.CaseLength','P.CornerRadius',
                  height,'P.CaseCenterX','P.CaseCenterY','P.OpeningZ')
    inner=rounded(doc,'RearVoid','P.CaseWidth-2*P.Wall','P.CaseLength-2*P.Wall',
                  'P.CornerRadius-P.Wall',f'({height})+2 mm',
                  'P.CaseCenterX','P.CaseCenterY','P.OpeningZ-1 mm')
    walls=cut(doc,'RearWalls',outer,inner)
    flange_outer=rounded(doc,'FlangeOuter','P.CaseWidth','P.CaseLength','P.CornerRadius',
                         'P.FlangeOverlap','P.CaseCenterX','P.CaseCenterY','P.OriginalFloorOuterZ')
    flange_void=rounded(doc,'FlangeVoid','35 mm','39 mm','3 mm','P.FlangeOverlap+2 mm',
                        'P.CaseCenterX','20.70537 mm','P.OriginalFloorOuterZ-1 mm')
    flange=cut(doc,'TransitionFlange',flange_outer,flange_void)
    battery=box(doc,'BatteryEnvelope','P.BatteryL','P.BatteryW','P.BatteryH',
                'P.BatteryCX-P.BatteryL/2','P.BatteryCY-P.BatteryW/2','P.ComponentFrontZ-P.BatteryH')
    battery.Label='电池本体上限34×21×10：封装／线束另验'
    speaker=box(doc,'SpeakerEnvelope','P.SpeakerL','P.SpeakerW','P.SpeakerH',
                'P.SpeakerCX-P.SpeakerL/2','P.SpeakerCY-P.SpeakerW/2','P.ComponentFrontZ-P.SpeakerH')
    speaker.Label='喇叭SKU27859本体上限：发声面／线束另验'
    motor=cylinder(doc,'MotorEnvelope','P.MotorD/2','P.MotorH',
                   'P.MotorCX','P.MotorCY','P.MotorFrontZ-P.MotorH')
    motor.Label='电机Ø10×2.7名义：焊点／背胶另验'
    bframe=frame(doc,'BatteryLocator','P.BatteryL','P.BatteryW','P.BatteryCX','P.BatteryCY','P.OriginalFloorOuterZ')
    sframe=frame(doc,'SpeakerLocator','P.SpeakerL','P.SpeakerW','P.SpeakerCX','P.SpeakerCY','P.OriginalFloorOuterZ')
    structure=[orig,walls,flange,bframe,sframe]
    if variant=='A':
        mo=cylinder(doc,'MotorPocketOuter','P.MotorD/2+P.FitXY+P.FrameWall','P.FrameHeight',
                    'P.MotorCX','P.MotorCY','P.OriginalFloorOuterZ-P.FrameHeight')
        mi=cylinder(doc,'MotorPocketVoid','P.MotorD/2+P.FitXY','P.FrameHeight+2 mm',
                    'P.MotorCX','P.MotorCY','P.OriginalFloorOuterZ-P.FrameHeight-1 mm')
        structure.append(cut(doc,'MotorPocket',mo,mi))
    else:
        bridge=box(doc,'MotorBridge','P.SpeakerL+2*(P.FitXY+P.FrameWall)',
                   'P.MotorD+2*(P.FitXY+P.FrameWall)','P.MotorCarrier',
                   'P.SpeakerCX-P.SpeakerL/2-P.FitXY-P.FrameWall',
                   'P.MotorCY-P.MotorD/2-P.FitXY-P.FrameWall',
                   'P.MotorCarrierTopZ-P.MotorCarrier')
        structure.append(bridge)
        for i,sgn in enumerate([-1,1]):
            leg=box(doc,'MotorBridgeLeg'+str(i),'P.FrameWall',
                    'P.MotorD+2*(P.FitXY+P.FrameWall)',
                    'P.OriginalFloorOuterZ-P.MotorCarrierTopZ+P.MotorCarrier',
                    f'P.SpeakerCX+{sgn}*(P.SpeakerL/2+P.FitXY)' + ('-P.FrameWall' if sgn<0 else ''),
                    'P.MotorCY-P.MotorD/2-P.FitXY-P.FrameWall','P.MotorCarrierTopZ-P.MotorCarrier')
            structure.append(leg)
        mo=cylinder(doc,'MotorPocketOuter','P.MotorD/2+P.FitXY+P.FrameWall','P.FrameHeight',
                    'P.MotorCX','P.MotorCY','P.MotorCarrierTopZ-P.MotorCarrier-P.FrameHeight')
        mi=cylinder(doc,'MotorPocketVoid','P.MotorD/2+P.FitXY','P.FrameHeight+2 mm',
                    'P.MotorCX','P.MotorCY','P.MotorCarrierTopZ-P.MotorCarrier-P.FrameHeight-1 mm')
        structure.append(cut(doc,'MotorPocket',mo,mi))
    bosses=[]; pilots=[]; lidholes=[]
    for i,sgn in enumerate([-1,1]):
        x=f'P.CaseCenterX+{sgn}*P.BossOffsetX'
        bosses.append(cylinder(doc,'ClosureBoss'+str(i),'P.BossRadius','P.BossDepth',x,'P.BossY','P.OpeningZ'))
        pilots.append(cylinder(doc,'ClosurePilot'+str(i),'P.PilotRadius','P.PilotDepth+1 mm',x,'P.BossY','P.OpeningZ-1 mm'))
        lidholes.append(cylinder(doc,'LidHole'+str(i),'P.LidHoleRadius','P.LidThickness+P.LidLipHeight+2 mm',x,'P.BossY','P.OpeningZ-P.LidThickness-1 mm'))
    structure.extend(bosses)
    print('Recomputing native structure '+variant,flush=True)
    native_module=fuse(doc,'AnalyticRearModule',structure[1:])
    body_base=fuse(doc,'RearStructureUncut',[orig,native_module])
    # Preserve source triangulated faces here. Refining a mixed faceted/analytic
    # union can invalidate a result even when its individual solids are valid.
    body_base.Refine=False
    vents=[]
    for i,dy in enumerate([-4.5,-1.5,1.5,4.5]):
        vents.append(cylinder(doc,'SoundVent'+str(i),'0.9 mm','P.Wall+4 mm',
            'P.CaseCenterX-P.CaseWidth/2-1 mm',f'P.SpeakerCY+{dy} mm','P.VentZ',True))
    body=cut(doc,'RearHousing',body_base,fuse(doc,'RearHousingTools',pilots+vents))
    body.Refine=False
    body.Label='A0后舱 '+variant+'：原固定结构＋新附件舱（草案）'
    lidplate=rounded(doc,'LidPlate','P.CaseWidth','P.CaseLength','P.CornerRadius','P.LidThickness',
                     'P.CaseCenterX','P.CaseCenterY','P.OpeningZ-P.LidThickness')
    lipouter=rounded(doc,'LidLipOuter','P.CaseWidth-2*(P.Wall+P.LidFit)',
                     'P.CaseLength-2*(P.Wall+P.LidFit)','P.CornerRadius-P.Wall-P.LidFit',
                     'P.LidLipHeight','P.CaseCenterX','P.CaseCenterY','P.OpeningZ')
    lipinner=rounded(doc,'LidLipVoid','P.CaseWidth-2*(P.Wall+P.LidFit+P.LidLipWall)',
                     'P.CaseLength-2*(P.Wall+P.LidFit+P.LidLipWall)',
                     'P.CornerRadius-P.Wall-P.LidFit-P.LidLipWall','P.LidLipHeight+2 mm',
                     'P.CaseCenterX','P.CaseCenterY','P.OpeningZ-1 mm')
    lip=cut(doc,'LidLip',lipouter,lipinner)
    reliefs=[]
    for i,sgn in enumerate([-1,1]):
        reliefs.append(cylinder(doc,'LipBossRelief'+str(i),'P.BossRadius+P.LidFit',
             'P.LidLipHeight+2 mm',f'P.CaseCenterX+{sgn}*P.BossOffsetX','P.BossY','P.OpeningZ-1 mm'))
    lip=cut(doc,'RelievedLidLip',lip,fuse(doc,'LipReliefTools',reliefs))
    lid=cut(doc,'RearLid',fuse(doc,'LidUncut',[lidplate,lip]),fuse(doc,'LidHoleTools',lidholes))
    lid.Label='可拆后盖：两个闭合孔为设计值，螺丝未选定'
    rf=box(doc,'AntennaReservation','35.111 mm','10 mm','40 mm',
           'P.CaseCenterX-35.111 mm/2','P.AntennaStartY','P.PCBRearZ-40 mm')
    rf.Label='天线暂定避让带（非厂家RF净距）'
    doc.recompute()
    # Keep a local connector reference diagnostic separate from the real
    # packaged connector/caps envelope, which has not been measured.
    cn3_xy=[core_world_xy(cfg,x,y) for x,y in [(8.41228442,7.009003),(27.46225902,10.008997)]]
    cn3=box(doc,'CN3ReferenceEnvelope','19.0499746 mm','2.999994 mm',
            'P.GlobalElectronicsHeight',f'{min(a[0] for a in cn3_xy)} mm',
            f'{min(a[1] for a in cn3_xy)} mm','P.PCBRearZ-P.GlobalElectronicsHeight')
    note(cn3,'V1.2 silk-only XY body and global electronics height; diagnostic, not measured plastic/caps envelope.')
    doc.recompute()
    finals=[body,lid,battery,speaker,motor,core,screen]+columns
    checks={}
    for obj in finals:
        shape=obj.Shape
        if shape.isNull():
            doc.saveAs(str(out/('FAILED_'+variant+'_NOT_FOR_PRINTING.FCStd')))
            diagnostics=[]
            for item in structure+[native_module,body_base,body]:
                sh=item.Shape
                diagnostics.append({'name':item.Name,'state':list(item.State),
                    'null':sh.isNull(),'valid':None if sh.isNull() else sh.isValid(),
                    'solids':0 if sh.isNull() else len(sh.Solids)})
            (out/('failed-shape-'+variant+'.json')).write_text(json.dumps(diagnostics,ensure_ascii=False,indent=2),encoding='utf-8')
            raise RuntimeError('Null final shape: '+obj.Name+'; state='+str(obj.State))
        checks[obj.Name]={'valid':shape.isValid(),'solids':len(shape.Solids),'volume_mm3':shape.Volume}
        if shape.isNull() or not shape.isValid() or len(shape.Solids)!=1:
            doc.saveAs(str(out/('FAILED_'+variant+'_NOT_FOR_PRINTING.FCStd')))
            (out/('failed-shape-'+variant+'.json')).write_text(json.dumps({
                'object':obj.Name,'check':checks[obj.Name],
                'solid_centres':[list(s.CenterOfMass) for s in shape.Solids]
            },ensure_ascii=False,indent=2),encoding='utf-8')
            raise RuntimeError('Invalid or disconnected final shape: '+obj.Name+' '+str(checks[obj.Name]))
    collisions=[]
    pairs=[(body,battery),(body,speaker),(body,motor),(lid,battery),(lid,speaker),(lid,motor),
           (lid,body),(battery,speaker),(battery,motor),(speaker,motor),
           (battery,rf),(speaker,rf),(motor,rf)]
    for a,b in pairs:
        volume=a.Shape.common(b.Shape).Volume
        collisions.append({'a':a.Name,'b':b.Name,'common_volume_mm3':volume,'pass':volume<0.001})
    bb=body.Shape.BoundBox
    report={
        'variant':variant,'status':'ok','release':'DRAFT_NOT_FOR_PRINTING',
        'freecad_version':list(App.Version()),'shape_checks':checks,'collision_checks':collisions,
        'modelled_checks_pass':all(c['pass'] for c in collisions),
        'rear_body_bbox_mm':[bb.XLength,bb.YLength,bb.ZLength],
        'glass_datum_to_outer_lid_mm':p.GlassDatumZ.Value-(p.OpeningZ.Value-p.LidThickness.Value),
        'board_registration':'candidate post-top datum, not physically verified',
        'reference_mount_fit_rms_mm':0.240279,'PCB_scaled':False,
        'reference_diagnostics':{
          'core_top_flipped_x':True,
          'reference_outline_bbox_mm':[core.Shape.BoundBox.XLength,core.Shape.BoundBox.YLength],
          'USB1_anchor_xy_mm':core_world_xy(cfg,4.318,17.873),
          'SW1_anchor_xy_mm':core_world_xy(cfg,33.426999,27.873),
          'CN3_silk_reference_vs_cap_mm3':cn3.Shape.common(orig.Shape).Volume,
          'CN3_silk_reference_vs_housing_mm3':cn3.Shape.common(body.Shape).Volume,
          'glass_above_source_frame_mm':p.GlassDatumZ.Value-10.437347,
          'minimum_nominal_M2_shaft_radial_margin_mm':1.265-1.0-.256050,
          'mount_margin_is_print_approval':False},
        'missing_clearance_models':['actual FPC route','USB/SPK/battery connector and cable envelopes',
          'complete packaged battery','actual glass profile','strap and articulation','selected fastener heads/engagement'],
        'original_fixing_role':'screen-board fixing system, user-confirmed',
        'outward_gold_posts_in_assembly':False,'hardware_changed':False,
        'notes':['A0 deliberately preserves original cap floor as bulkhead; no thin local tray assumed',
          'All allowance, frame, fastener and antenna-strip parameters are proposals, not verified safety limits',
          'No pin/connector/temperature/structural/acoustic/physical fit release follows from these scoped checks']
    }
    doc.recompute()
    fcstd=out/('WristFlow_A0_'+variant+'_NOT_FOR_PRINTING.FCStd')
    doc.saveAs(str(fcstd))
    Part.export([body,lid],str(out/('WristFlow_A0_'+variant+'_NOT_FOR_PRINTING.step')))
    (out/('report_'+variant+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    report['fcstd']=str(fcstd)
    App.closeDocument(doc.Name)
    return report


def main():
    req=json.loads(Path(os.environ['WRISTFLOW_CAD_REQUEST']).read_text(encoding='utf-8'))
    out=Path(req['out']); cfg=json.loads(Path(req['parameters']).read_text(encoding='utf-8'))
    source=Path(req['source_zip'])
    if hashlib.sha256(source.read_bytes()).hexdigest()!=cfg['source_zip_sha256']:
        raise ValueError('Source ZIP differs from inspected baseline; refusing implicit geometry replacement')
    sources=out/'private_source_meshes'; sources.mkdir(exist_ok=True)
    names=['外壳.stl','底部外壳.stl','V0.2黄山派-按钮其一.stl','V0.2黄山派-按钮其二.stl','上横杆.stl','下横杆.stl']
    paths={}
    with ZipFile(source) as z:
        for name in names:
            raw=z.read(name)
            n=struct.unpack_from('<I',raw,80)[0]
            if len(raw)!=84+50*n: raise ValueError('Unexpected binary STL member')
            dest=sources/name; dest.write_bytes(raw); paths[name]=dest
    mesh=Mesh.Mesh(str(paths['底部外壳.stl']))
    cap=Part.Shape(); cap.makeShapeFromMesh(mesh.Topology,.02)
    cap=Part.makeSolid(cap).removeSplitter()
    if not cap.isValid() or len(cap.Solids)!=1: raise ValueError('Reference cap conversion failed')
    reports=[]
    ref_paths={k:v for k,v in paths.items() if k!='底部外壳.stl'}
    for v in req['variants']:
        reports.append(build_variant(cfg,cap,ref_paths,out,v))
    result={'status':'ok','reports':reports,'source_hash_verified':True,
            'hardware_verified':False,'print_release':False}
    (out/'build-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('WRISTFLOW_CAD_BUILD_FINISHED',flush=True)


# FreeCADCmd executes Python files in its console namespace, not necessarily
# Python's ordinary __main__. The explicit launcher request is the entry gate.
if os.environ.get('WRISTFLOW_CAD_REQUEST') and not getattr(builtins,'_wristflow_cad_entry_ran',False):
    builtins._wristflow_cad_entry_ran=True
    try:
        main()
    except Exception:
        req=json.loads(Path(os.environ['WRISTFLOW_CAD_REQUEST']).read_text(encoding='utf-8'))
        (Path(req['out'])/'build-result.json').write_text(json.dumps({'status':'error','traceback':traceback.format_exc()},ensure_ascii=False,indent=2),encoding='utf-8')
        raise
