from pathlib import Path
import sys
import json
import hashlib
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from assembly import build,App,V,Part,F,B,HOLES,TEN,RF_INNER,REAR_FLEX
from checks import test
from space import calculate
from layout_space import native,footprint,rgb,PALETTE,HEIGHTS
import FreeCADGui as Gui

def run(dest):
    dest=Path(dest)
    Gui.activateWorkbench('PartWorkbench')
    s,g,l,c,n,held=build()
    qa=test(s,g,held)
    assert not qa['penetrations_mm3'],qa['penetrations_mm3']
    stats,masks,examples,real,rf,audit,flat_patch,local_parts,gains,pending=calculate(s,g)
    previous=json.loads((HERE/'release-baseline.json').read_text(encoding='utf8'))
    assert stats['Rear']==previous['heights']['Rear']
    assert all(abs(old['area_mm2']-now['previous_area_mm2'])<.1 for old,now in zip(previous['heights']['Front'],audit['front']))
    audit['adapter_gap_observation_mm']=[0,15]
    audit['adapter_observation']='原屏与原转接板可贴合至沿板面法线分开约 15 mm。新表固定由屏周边胶与中框三点支承完成。'
    qa['space_audit']=audit
    qa.update({'heights':stats,'box_fit':examples,'measure':'0.25mm complete grid cells; XY obstacle clearance 0.20 + 0.02 projection guard, board edge 0.30; Z 0.20 and cell extra 0.30',
               'screen':'Wide sheet follows display. Photo-registered uniform patch uses local installed height; unmeasured islands and moving tail stay reserved.','rf_hold':'X <= -11.10 across host board reserved; keepout is a mechanical budget, not RF certification',
               'freecad':App.Version()[:3]})
    def group(doc,k,label):
        obj=doc.addObject('App::DocumentObjectGroup',k);obj.Label=label;return obj
    def add(doc,grp,k,label,shape,color,note=''):
        o=doc.addObject('Part::Feature',k);o.Label=label;o.Shape=shape;grp.addObject(o)
        o.ViewObject.ShapeColor=color;o.ViewObject.LineColor=(.17,.21,.23);o.ViewObject.LineWidth=1
        o.addProperty('App::PropertyString','Basis','Review').Basis=note
        return o
    def pic(doc,name,view,up=(0,1,0),size=(1280,900)):
        doc.recompute();v=Gui.getDocument(doc.Name).activeView();v.setAnimationEnabled(False)
        v.setCameraType('Orthographic');v.setCameraOrientation(App.Rotation(V(1,0,0),V(*up),V(*view),'ZYX').Q)
        v.fitAll();Gui.updateGui()
        if name:v.saveImage(str(dest/(name+'.png')),*size,'White')
    def save(doc,name):
        f=dest/(name+'.FCStd');vol={o.Name:o.Shape.Volume for o in doc.Objects if o.TypeId=='Part::Feature'}
        doc.recompute();doc.saveAs(str(f));App.closeDocument(doc.Name)
        d=App.openDocument(str(f));d.recompute()
        for k,v in vol.items():
            shape=d.getObject(k).Shape;assert shape.isValid() and abs(shape.Volume-v)<1e-4,k
        App.closeDocument(d.Name)
        qa.setdefault('files',{})[f.name]={'reopened_objects':len(vol),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
    cc={'ModuleParts':(.43,.52,.59),'ModuleSubstrate':(.16,.35,.32),'Antenna':(.85,.26,.19),'FrontRun':(.76,.44,.18),
        'RearRun':(.95,.64,.17),'Crossing':(.92,.55,.15),'CN1':(.13,.33,.48),'Motor':(.51,.37,.64),'LidFlex':(.79,.49,.17)}
    d=App.newDocument('IntegratedB');d.Label='WristFlow B · 贴屏宽片与主板三点固定'
    group_labels={'Fixed':'中框、前屏与电机','Band':'表耳、22 mm 表带与带扣','Display':'原屏 FPC 与补强','Circuit':'主板、模组、CN1 与接口','Fasteners':'主板锁紧螺钉','Cell':'电池、可拆托板','Mated':'三线电池插头和线束',
                  'Lid':'后盖、PPG、喇叭和十触点','Links':'日常测流短接帽','Tools':'维修工具及穿窗通道','Meter':'六针测流夹具','Jig':'串口夹具','FreeHarness':'分离后的电池插头','ViewCut':'穿屏尾纵剖面 X = −4 mm'}
    gs={k:group(d,k,v) for k,v in group_labels.items()};objs={}
    for k,shape in s.items():
        objs[k]=add(d,gs[g[k]],k,l[k],shape,cc.get(k,c[k]),n[k])
    cell=s['Battery'];cell_z=cell.BoundBox.ZMax
    cell_face=next(f for f in cell.Faces if abs(f.BoundBox.ZMin-cell_z)<1e-6 and f.BoundBox.ZLength<1e-6)
    film_t=.10
    separator=cell_face.extrude(V(0,0,film_t))
    bare=B-cell_z
    near=s['ModuleParts'].distToShape(separator)[0]
    assert abs(near-(qa['module_to_cell_mm']-film_t))<1e-6
    qa['separator_study']={'status':'optional envelope, outside the 5 mm pack; current board layout does not close its worst-case budget',
        'total_thickness_mm':film_t,'cell_to_bare_board_mm':round(bare,3),'film_to_module_mm':round(near,3),
        'margin_after_existing_0p20_and_0p30_mm':round(near-.20-.30,3),'pack_max_with_extra_film_for_same_budget_mm':round(cell.BoundBox.ZLength+near-.5,3)}
    study=group(d,'IsolationStudy','可选试算 · 电芯朝主板侧的绝缘片')
    film=add(d,study,'Separator','0.10 mm 含胶厚度占位 · 模组处差 0.10 mm',separator,(.75,.46,.72),'电芯外额外叠层试算；材料、固定方式及电池寿命末期尺寸未定。')
    film.ViewObject.Visibility=False;study.ViewObject.Visibility=False
    clip=Part.makeBox(.45,49,20,V(-4.225,-24.5,-2));inst={'Fixed','Display','Circuit','Cell','Mated','Lid','Links','Fasteners','Band'}
    extra={}
    for k in s:
        if g[k] not in inst or g[k]=='Band':continue
        ss=clip.common(s[k])
        if ss.Volume>.00001:
            extra[k]=add(d,gs['ViewCut'],'Cut'+k,l[k]+' · 剖切',ss,cc.get(k,c[k]))
    def show(active,omit=()):
        for k,o in gs.items():o.ViewObject.Visibility=k in active
        for k,o in objs.items():o.ViewObject.Visibility=g[k] in active and k not in omit
        for k,o in extra.items():o.ViewObject.Visibility='ViewCut' in active
    show(inst);pic(d,'full',(.27,-.28,1),(.98,.22,0),(1600,620))
    band_far=['BandTailShort','BandTailLong','Buckle','BuckleTongue','Keeper']
    show(inst,band_far);pic(d,'body',(.48,-.55,1))
    show(inst-{'Lid','Cell','Mated','Band'});pic(d,'inside',(0,0,-1))
    show({'Lid'},[]);pic(d,'lid',(.25,-.32,1))
    show({'ViewCut'});pic(d,'section',(1,0,0),(0,0,1))
    show(inst-{'Lid','Band'}|{'Jig'});pic(d,'uart',(.32,-.42,-1))
    show({'Fixed','Fasteners','Circuit','Display'},[k for k in s if k not in {'Frame','GlassTape','Board','FrontRun','Crossing','RearRun','CN1','Stiffener','FlexPlug','Clamp1','Clamp2','Clamp3'}])
    pic(d,'fixing',(.37,-.40,1))
    show(inst);pic(d,None,(.27,-.28,1),(.98,.22,0));save(d,'WristFlow-B-Integrated')

    d=App.newDocument('IntegratedBSpace');d.Label='WristFlow B · 原屏折返后双面器件空间'
    pcb=group(d,'PCB','新板框、模组和屏尾位置');rfg=group(d,'RF','天线端预留与电池投影');examples_g=group(d,'Example','可选 · 8 × 6 × 2 mm 样块')
    fl=group(d,'Film','可选 · 屏背宽片、活动段与局部凸起投影')
    cell_ref=group(d,'CellOutline','可选 · 电池投影轮廓')
    front=group(d,'Front','屏幕侧器件包络');rear=group(d,'Rear','电池侧器件包络')
    for k in ['Board','ModuleParts','ModuleSubstrate','CN1','Crossing','RearRun','Stiffener','BatHeader','KeyBody',*[f'LPad{i}' for i in range(10)],*[f'Clamp{i}' for i in (1,2,3)]]:
        obj=add(d,pcb,k,l[k],s[k],(.66,.68,.68) if k=='Board' else cc.get(k,c[k]))
    add(d,rfg,'Radiator','天线 · 电芯投影重叠 109.8 mm²',s['Antenna'],cc['Antenna'])
    rf_slab=Part.makeBox(RF_INNER+30-.00001,50,.025,V(-30,-25,B+.10))
    rf_band=Part.makeCompound([q for q in s['Board'].common(rf_slab).Solids if q.Volume>1e-8])
    assert rf_band.isValid()
    for face,z in [('Front',F+.003),('Rear',B-.028)]:
        add(d,rfg,'Reserved'+face,'天线端机械预留',rf_band.translated(V(0,0,z-B-.10)),(.77,.44,.36))
    add(d,fl,'FrontFilm','屏侧固定 FPC 参考轮廓',s['FrontRun'],cc['FrontRun'])
    add(d,fl,'FlatCheck','红叉内参与限高计算的平贴区',native(flat_patch,F+.81,.02),(.16,.69,.49),'仅对平坦处作条件试放。')
    add(d,fl,'LocalParts','胶带、器件岛及根部 · 照片估计预留',native(local_parts.intersection(real),F+.81,.02),(.61,.36,.66))
    cell_top=max(s['Battery'].Faces,key=lambda f:f.Area)
    border=add(d,cell_ref,'CellEdge','电芯外沿投影 · 从电池侧看',cell_top.OuterWire.translated(V(0,0,B-6.2-cell_top.BoundBox.ZMin)),(.08,.43,.26))
    border.ViewObject.LineColor=(.08,.43,.26);border.ViewObject.LineWidth=4
    for face,heights in HEIGHTS.items():
        for i,h in enumerate(heights):
            band=masks[face][i].copy()
            if i+1<len(heights):band&=~masks[face][i+1]
            if not band.any():continue
            poly=footprint(band);shape=native(poly,F if face=='Front' else B,h if face=='Front' else -h)
            o=add(d,front if face=='Front' else rear,face+'Zone'+str(i),('屏侧 ' if face=='Front' else '电池侧 ')+str(h)+' mm',shape,rgb(PALETTE[face][i]))
            o.addProperty('App::PropertyArea','AreaAtLeast','Budget').AreaAtLeast=stats[face][i]['area_mm2']
            o.addProperty('App::PropertyLength','Height','Budget').Height=h
            o.addProperty('App::PropertyArea','ThisBand','Budget').ThisBand=poly.area
    if examples:
        x,y,w,ll,h=examples['8x6x2']
        add(d,examples_g,'PowerBlock','8 × 6 × 2 mm 样块 · 隐藏蓝色包络后查看',Part.makeBox(w,ll,h,V(x,y,B-h)),(.93,.63,.15))
    for group_obj in [examples_g,fl,cell_ref]:
        group_obj.ViewObject.Visibility=False
        for o in group_obj.Group:o.ViewObject.Visibility=False
    for face,obj in [('Front',front),('Rear',rear)]:
        actual=sum(q.Shape.Volume for q in obj.Group)
        expected=sum((r['height_mm']-(rs[i-1]['height_mm'] if i else 0))*r['area_mm2'] for rs in [stats[face]] for i,r in enumerate(rs))
        assert abs(actual-expected)<.001
    pic(d,'face_front',(.37,-.4,1));pic(d,'face_rear',(.38,-.4,-1))
    if gains[0].any():
        gained=add(d,fl,'NewArea','与上版相比可补放低件的整格',native(footprint(gains[0]),F+.74,.02),(.92,.64,.14))
        gained.ViewObject.Visibility=False
    if pending[0].any():
        tentative=add(d,fl,'HeightPending','仍可规划 · 照片位置和高度待核',native(footprint(pending[0]),F+.74,.02),(.65,.48,.75))
        tentative.ViewObject.Visibility=False
    save(d,'WristFlow-B-Placement')
    st=dest/'B-board-outline.step';s['Board'].exportStep(str(st));restored=Part.read(str(st))
    assert restored.isValid() and abs(restored.Volume-s['Board'].Volume)<.00001 and len(restored.Solids)==1
    qa['step_sha256']=hashlib.sha256(st.read_bytes()).hexdigest()
    (dest/'geometry.json').write_text(json.dumps(qa,ensure_ascii=False,indent=2),encoding='utf8')
    (dest/'reopened.ok').write_text('ok')
