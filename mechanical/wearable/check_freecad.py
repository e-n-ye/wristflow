"""Native reopen, STEP roundtrip and bounded parameter-change verification.

Run with FreeCADCmd after setting WRISTFLOW_CAD_CHECK_OUT to the build directory.
All edits are in memory and restored; original CAD files are never saved here.
"""
import json
import math
import os
from pathlib import Path
import traceback
import FreeCAD as App
import Part


def require(condition,message):
    if not condition:
        raise AssertionError(message)


def collision_checks(doc):
    pairs=[('RearHousing','BatteryEnvelope'),('RearHousing','SpeakerEnvelope'),
           ('RearHousing','MotorEnvelope'),('RearLid','BatteryEnvelope'),
           ('RearLid','SpeakerEnvelope'),('RearLid','MotorEnvelope'),
           ('RearLid','RearHousing'),('BatteryEnvelope','SpeakerEnvelope'),
           ('BatteryEnvelope','MotorEnvelope'),('SpeakerEnvelope','MotorEnvelope'),
           ('BatteryEnvelope','AntennaReservation'),('SpeakerEnvelope','AntennaReservation'),
           ('MotorEnvelope','AntennaReservation')]
    results=[]
    for a,b in pairs:
        volume=doc.getObject(a).Shape.common(doc.getObject(b).Shape).Volume
        results.append({'a':a,'b':b,'common_volume_mm3':volume,
                        'pass':math.isfinite(volume) and 0<=volume<.001})
    return results


def check(out,variant):
    doc=App.openDocument(str(out/f'WristFlow_A0_{variant}_NOT_FOR_PRINTING.FCStd'))
    doc.recompute()
    for name in ['RearHousing','RearLid','CorePCBReference','ScreenPCBReference',
                 'BatteryEnvelope','SpeakerEnvelope','MotorEnvelope','AntennaReservation',
                 *['InterboardSupport'+str(i) for i in range(4)]]:
        shape=doc.getObject(name).Shape
        require(shape.isValid() and len(shape.Solids)==1,'Reopened invalid shape: '+name)
    require(not any('Invalid' in obj.State for obj in doc.Objects),'Invalid dependency on reopen')
    collisions=collision_checks(doc)
    require(all(item['pass'] for item in collisions),'Reopened model has a scoped collision: '+str(collisions))
    original_z=doc.P.OpeningZ.Value
    original_h=doc.P.BatteryH.Value
    original_volume=doc.RearHousing.Shape.Volume
    cell=doc.P.getCellFromAlias('BatteryH')
    changed_h=max(original_h+4,doc.P.ComponentFrontZ.Value-original_z+3)
    doc.P.set(cell,str(changed_h)+' mm'); doc.recompute()
    changed_z=doc.P.OpeningZ.Value
    # A starts battery-limited; B starts motor-stack-limited. In either case
    # a 14mm cell must push the lid further back without moving its front.
    require(changed_z<original_z-2,'Battery thickness failed to move opening')
    require(abs(doc.RearLid.Shape.BoundBox.ZMin-(changed_z-doc.P.LidThickness.Value))<1e-5,
            'Lid did not follow changed opening')
    require(doc.RearHousing.Shape.isValid() and len(doc.RearHousing.Shape.Solids)==1,
            'Thickness edit broke connected housing')
    doc.P.set(cell,str(original_h)+' mm'); doc.recompute()
    require(abs(doc.P.OpeningZ.Value-original_z)<1e-6,'Opening restore failed')
    require(abs(doc.RearHousing.Shape.Volume-original_volume)<.01,'Geometry restore failed')
    width_cell=doc.P.getCellFromAlias('CaseWidth')
    original_width=doc.P.CaseWidth.Value
    original_offset=doc.P.BossOffsetX.Value
    doc.P.set(width_cell,str(original_width+2)+' mm'); doc.recompute()
    require(abs(doc.P.BossOffsetX.Value-original_offset-1)<1e-6,'Closure bosses failed to follow width')
    require(doc.RearHousing.Shape.isValid() and len(doc.RearHousing.Shape.Solids)==1,
            'Width edit disconnected closure bosses')
    doc.P.set(width_cell,str(original_width)+' mm'); doc.recompute()
    # Run the actual collision checker with a deliberately moved antenna band.
    # The model is restored in memory; this must reject an otherwise valid edit.
    antenna_cell=doc.P.getCellFromAlias('AntennaStartY')
    original_antenna_y=doc.P.AntennaStartY.Value
    doc.P.set(antenna_cell,str(doc.P.BatteryCY.Value)+' mm'); doc.recompute()
    negative=collision_checks(doc)
    require(any(item['a']=='BatteryEnvelope' and item['b']=='AntennaReservation'
                and not item['pass'] for item in negative),'Collision negative control failed')
    doc.P.set(antenna_cell,str(original_antenna_y)+' mm'); doc.recompute()
    require(all(item['pass'] for item in collision_checks(doc)),'Collision restore failed')
    step=Part.Shape()
    step.read(str(out/f'WristFlow_A0_{variant}_NOT_FOR_PRINTING.step'))
    require(step.isValid() and len(step.Solids)==2,'STEP does not contain two valid solids')
    require(abs(step.Volume-doc.RearHousing.Shape.Volume-doc.RearLid.Shape.Volume)<.1,
            'STEP volume differs from source')
    App.closeDocument(doc.Name)
    return {'variant':variant,'reopen_valid':True,'parameter_battery_height':True,
            'battery_h_before_after_mm':[original_h,changed_h],
            'opening_z_before_after_mm':[original_z,changed_z],
            'parameter_width_plus2mm':True,'restored_in_memory':True,
            'collision_checks':collisions,'modelled_checks_pass':True,
            'collision_negative_control':'moved antenna band rejected','STEP_valid_solids':2,
            'saved_input_modified':False,'hardware_verified':False,'print_release':False}


if os.environ.get('WRISTFLOW_CAD_CHECK_OUT'):
    out=Path(os.environ['WRISTFLOW_CAD_CHECK_OUT'])
    try:
        result={'status':'ok','checks':[check(out,v) for v in ['A','B']]}
    except Exception:
        result={'status':'error','traceback':traceback.format_exc()}
    (out/'native-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False),flush=True)
