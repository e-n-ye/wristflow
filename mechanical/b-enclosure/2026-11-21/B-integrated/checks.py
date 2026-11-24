import json
from pathlib import Path
import FreeCAD as A
import Part
import shapely as sh
from assembly import B,F,SLOT,HOLES,TEN,REAR_FLEX,V,box,rounded,cylinder,down_prisms,comp,shadow,build,rect

def test(s,g,held,board_holes=HOLES,tray_holes=((-12.5,-20.77),(17.5,10.96))):
    checks={};hits={};eps=.0005
    def names(*groups):return [k for k in s if g[k] in groups]
    def check(label,moving,targets):
        count=0
        for key in targets:
            if moving.BoundBox.intersect(s[key].BoundBox):
                ov=moving.common(s[key]).Volume
                if ov>eps:hits[label+' / '+key]=round(ov,5)
            count+=1
        checks[label]=count
    def solids(keys):return comp(s[k] for k in keys)
    def sweep(keys,h):return comp(down_prisms(s[k],h) for k in keys)
    fixed=names('Fixed');pcb=names('Circuit');lid=names('Lid');cell=names('Cell')
    cable=names('Mated');locks=names('Fasteners');fpc=names('Display');jig=names('Meter','Jig')
    installed=fixed+pcb+lid+cell+cable+locks+fpc+names('Links')
    for k in ['ModuleParts','ModuleSubstrate','Antenna','CN1','Crossing','FrontRun','RearRun','Stiffener','FlexPlug','Motor','LidTower','LidFlex','ChargeLeads','SpeakerFlex']:
        ignore={k}
        if k in ['ModuleParts','ModuleSubstrate','Antenna']:ignore.update(['ModuleParts','ModuleSubstrate','Antenna'])
        if k in ['ChargeLeads','LidFlex','SpeakerFlex','LidTower']:ignore.update(['LidTower','LidFlex','SpeakerFlex','ChargeLeads','Speaker','PpgBoard'])
        if k=='ChargeLeads':ignore.update(['Contact0','Contact1'])
        check('mounted '+k,s[k],[t for t in installed if t not in ignore])
    check('lid release from mounted board and ribbon',sweep([k for k in lid if not k.startswith('CoverScrew')],16),fixed+pcb+locks+cable+cell+fpc+names('Links'))
    check('cell tray out before disconnecting display',sweep(['Battery','Tray'],18),fixed+pcb+fpc+locks)
    for i,(x,y) in enumerate(board_holes,1):
        check('driver for main board screw '+str(i),cylinder(1.30,B-.6+4,(x,y,-4)),fixed+pcb+fpc)
    for i,(x,y) in enumerate(tray_holes,1):
        check('driver for battery tray '+str(i),cylinder(.93,5.8,(x,y,-4)),fixed+['Battery']+fpc)
    check('remove PCB once screen tail is free and unthreaded',sweep(pcb,20),fixed+['FrontRun'])
    check('closed cell + tray against all board parts',solids(cell),pcb+locks+fpc)
    check('installed dock through-path for battery tool',s['PullAccess'],jig+names('Links')+fpc)
    check('3 mm battery plug removal',down_prisms(s['BatPlug'],3),jig+names('Links')+fpc+fixed+cell+[k for k in pcb if k!='BatHeader'])
    check('battery connector tool access',s['PullAccess'],fixed+cell+fpc+[k for k in pcb if k not in ['BatHeader','Board']])
    check('measure key removal',sweep(names('Links'),15),fixed+pcb+cell+locks+fpc)
    check('serial jig with key',sweep(names('Jig'),15),names('Links'))
    check('bench jig insertion',sweep(jig,15),fixed+cell+cable+fpc+locks+[k for k in pcb if not k.startswith(('DBG','I'))])
    for i in range(3):
        check('battery wire parked '+str(i),s['FreeLead'+str(i)],fixed+pcb+fpc+jig+['Tray'])
    check('CN1 tool when battery and back are removed',s['CN1Access'],
          fixed+locks+names('Links')+[k for k in pcb if k not in ['Board','CN1']])
    check('feed corridor clear of fitted board hardware with case open',s['FeedSpace'],
          fixed+locks+[k for k in pcb if k!='CN1'])
    check('nominal tipped head inside aperture',held,['Board'])
    # The screen is fixed. Feeding before final seating creates clearance for the rigid head to emerge.
    # Equivalent test in PCB coordinates: screen and frame are shifted +Z relative to the loosened PCB.
    stage=5.0
    tip=held.translated(V(0,0,F-held.BoundBox.ZMin+.1))
    for k in ['Screen','Frame','FrontRun']:
        ov=tip.common(s[k].translated(V(0,0,stage))).Volume
        if ov>eps:hits['threading head at 5mm separation / '+k]=round(ov,5)
    pads=[k for k in pcb if k.startswith(('LPad','DBG')) or (k.startswith('I') and k[1:].isdigit())]
    for k in pads:
        p=s[k].translated(V(0,0,.05));assert abs(p.common(s['Board']).Volume-p.Volume)<1e-5,k
    checks['fully supported electrical pads']=len(pads)
    pcbshape=s['Board'];area=pcbshape.Volume/(F-B)
    assert pcbshape.isValid() and len(pcbshape.Solids)==1 and abs(area-max(f.Area for f in pcbshape.Faces))<1e-5
    footprint=shadow(pcbshape)
    active=shadow(s['ModuleParts']); effective=round(active.intersection(footprint).area/active.area,4)
    antenna=shadow(s['Antenna'])
    assert antenna.intersection(footprint).area<.0005
    assert shadow(s['CN1']).difference(footprint).area<.0001
    assert effective>.995,effective
    ann={'radiator_area_mm2':round(antenna.area,2),'battery_projected_overlap_mm2':round(antenna.intersection(shadow(s['Battery'])).area,2),
         'radiator_to_cell_mm':round(s['Antenna'].distToShape(s['Battery'])[0],3),'host_pcb_under_radiator_mm2':0,
         'front_flex_projected_overlap_mm2':round(antenna.intersection(shadow(s['FrontRun'])).area,2)}
    # Positive geometric controls, plus actual three-decimal interface and skin-facing contact coordinates.
    assert s['RearRun'].common(s['ModuleSubstrate'].translated(V(0,12,0))).Volume>0
    assert s['Board'].common(s['CN1'].translated(V(0,0,.7))).Volume>.01
    return {'checks':checks,'penetrations_mm3':hits,'threshold_mm3':eps,'board_mm2':area,'board_thickness_mm':F-B,
            'cn1_xy':[10.545,5.135],'slot_center_xy':list(SLOT),'slot_size_radius_mm':[14,3,.5],
            'module_center_xy':[s['ModuleSubstrate'].BoundBox.Center.x,s['ModuleSubstrate'].BoundBox.Center.y],'lid_contacts_xy':[[s['LPad'+str(i)].BoundBox.Center.x,s['LPad'+str(i)].BoundBox.Center.y] for i in range(10)],'support_holes':board_holes,'rf':ann,
            'electronic_region_board_support_fraction':effective,
            'module_to_folded_tail_mm':round(min(s[k].distToShape(s['ModuleSubstrate'])[0] for k in REAR_FLEX+['Crossing']),3),
            'module_to_cell_mm':round(s['ModuleParts'].distToShape(s['Battery'])[0],3),
            'head_projected_size_mm':[held.BoundBox.XLength,held.BoundBox.YLength],
            'charge_contact_pitch_mm':3.5,'service_gap_study_mm':stage}
if __name__=='__main__':
    s,g,_,_,_,h=build()
    data=test(s,g,h);print(json.dumps(data,ensure_ascii=True));assert not data['penetrations_mm3']
