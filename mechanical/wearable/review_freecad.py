"""Executed by an explicitly generated local FreeCAD macro for visual QA.

REQUEST is supplied by that macro; no terminal commands or downloads are run.
"""
import json
from pathlib import Path
import traceback
import FreeCAD as App
import FreeCADGui as Gui


def visual_review(request):
    out=Path(request['out'])
    variant=request['variant']
    source=out/('WristFlow_A0_'+variant+'_NOT_FOR_PRINTING.FCStd')
    doc=App.openDocument(str(source))
    for obj in doc.Objects:
        if obj.ViewObject:
            obj.ViewObject.Visibility=False
    colours={
        'RearHousing':((.78,.83,.9),72),
        'RearLid':((.7,.75,.83),0),
        'BatteryEnvelope':((1.0,.64,.15),5),
        'SpeakerEnvelope':((.19,.57,.91),8),
        'MotorEnvelope':((.62,.32,.82),0),
        'CorePCBReference':((.12,.22,.38),30),
        'ScreenPCBReference':((.1,.47,.43),35),
    }
    for name,(rgb,alpha) in colours.items():
        obj=doc.getObject(name)
        obj.ViewObject.ShapeColor=rgb
        obj.ViewObject.LineColor=(.15,.18,.23)
        obj.ViewObject.Transparency=alpha
        obj.ViewObject.Visibility=name!='RearLid'
    for obj in doc.Objects:
        if obj.Name.startswith('InterboardSupport'):
            obj.ViewObject.ShapeColor=(.7,.7,.73)
            obj.ViewObject.Visibility=True
    # Preserve main, KEY and strap-bar meshes as visual references only.
    for i in range(5):
        obj=doc.getObject('ReferenceMesh'+str(i))
        obj.ViewObject.ShapeColor=(.28,.5,.7) if i==0 else (.48,.68,.6)
        obj.ViewObject.Transparency=55 if i==0 else 10
        obj.ViewObject.Visibility=True
    doc.recompute()
    view=Gui.activeDocument().activeView()
    view.setAnimationEnabled(False) if hasattr(view,'setAnimationEnabled') else None
    view.setCameraType('Orthographic')
    # Camera looks toward +Z from the open rear, with an oblique view.
    view.setCameraOrientation(App.Rotation(App.Vector(0,0,-1),App.Vector(1,-1,1)).Q)
    view.fitAll()
    Gui.updateGui()
    view.saveImage(str(out/('open_iso_'+variant+'.png')),1600,1200,'White')
    view.viewBottom(); view.fitAll(); Gui.updateGui()
    view.saveImage(str(out/('rear_plan_'+variant+'.png')),1600,1200,'White')
    doc.getObject('RearLid').ViewObject.Visibility=True
    doc.getObject('RearHousing').ViewObject.Transparency=0
    for name in ['BatteryEnvelope','SpeakerEnvelope','MotorEnvelope','CorePCBReference','ScreenPCBReference']:
        doc.getObject(name).ViewObject.Visibility=False
    view.setCameraOrientation(App.Rotation(App.Vector(0,0,-1),App.Vector(1,-1,1)).Q)
    view.fitAll(); Gui.updateGui()
    view.saveImage(str(out/('closed_iso_'+variant+'.png')),1600,1200,'White')
    # Open-file default is the transparent, open-lid review state.
    doc.getObject('RearLid').ViewObject.Visibility=False
    doc.getObject('RearHousing').ViewObject.Transparency=72
    for name in ['BatteryEnvelope','SpeakerEnvelope','MotorEnvelope','CorePCBReference','ScreenPCBReference']:
        doc.getObject(name).ViewObject.Visibility=True
    view.fitAll(); Gui.updateGui()
    doc.save()
    result={'variant':variant,'status':'ok','gui_rendered':True,
            'files':[f'open_iso_{variant}.png',f'rear_plan_{variant}.png',f'closed_iso_{variant}.png'],
            'camera_view_direction':list(view.getViewDirection()),
            'print_release':False,'hardware_verified':False}
    (out/('visual-review_'+variant+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


try:
    visual_review(REQUEST)
except Exception:
    (Path(REQUEST['out'])/('visual-review-'+REQUEST['variant']+'-error.json')).write_text(
        json.dumps({'status':'error','traceback':traceback.format_exc()},ensure_ascii=False,indent=2),encoding='utf-8')
    raise
