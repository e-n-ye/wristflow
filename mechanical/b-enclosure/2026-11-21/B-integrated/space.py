"""Conservative assembly-aware mounting heights from the actual PCB top and bottom."""
import numpy as np
import shapely as sh
from shapely.geometry import box as rect,Polygon
from assembly import B,F,V,box,HOLES,RF_INNER,REAR_FLEX,Part,FLAT_END_Y
from layout_space import (PITCH,GRID_X,GRID_Y,HEIGHTS,shadow,group_union,footprint,largest_rectangle)

def calculate(s,g,board_holes=HOLES,rf_positive_x=False):
    # Read the real B-rep loops. Eroding a triangle-union outline can inflate submicron seams into false keepouts.
    top=max([f for f in s['Board'].Faces if abs(f.BoundBox.ZMin-F)<1e-6 and f.BoundBox.ZLength<1e-6],key=lambda f:f.Area)
    loops=[top.OuterWire]+[w for w in top.Wires if not w.isSame(top.OuterWire)]
    xy=[[(p.x,p.y) for p in w.discretize(Deflection=.001)] for w in loops]
    board=Polygon(xy[0],xy[1:])
    assert board.is_valid and len(loops)==5 and abs(board.area-top.Area)<.035
    inset=board.buffer(-.301)
    rf=rect(-RF_INNER,-25,23,25) if rf_positive_x else rect(-23,-25,RF_INNER,25)
    def members(*gs):return [k for k in s if g[k] in gs]
    fixed=members('Fixed')
    rear=[k for k in members('Lid','Circuit','Cell','Fasteners','Jig','Meter','Mated','FreeHarness','Links','Tools') if k!='Board']
    projected={}
    def proj(k):
        if k not in projected:projected[k]=shadow(s[k])
        return projected[k]
    # Reserve both complete contact banks and the service operations rather than just the gold spots.
    banks=[group_union([proj(k) for k in s if k.startswith(prefix)]).envelope for prefix in ['DBG','LPad','Sleeve']]
    screws=group_union([sh.Point(x,y).buffer(1.3) for x,y in board_holes])
    wire=group_union([proj(k) for k in s if k.startswith(('Lead','FreeLead'))]).envelope
    rear_route=group_union([proj(k) for k in ['RearRun','Crossing','Stiffener','FlexPlug','CN1','CN1Access','FeedSpace']])
    front_route=group_union([proj(k) for k in ['FrontRun','Crossing','FeedSpace']])
    # Photo masks use the glass outline to approximate XY. Keep the dark islands, edge
    # bond and taped section reserved until their absolute heights are measured.
    local_parts=group_union([rect(-18,-14,-8.5,-1.5), rect(-11.5,-12,5,0),
                             rect(-18,-4,-8.5,11), rect(-17,9.5,-7,17),
                             rect(-8,-21,10,-9)])
    flat_patch=proj('FrontRun').intersection(rect(-7.3,-12.2,15.7,FLAT_END_Y))
    flat_patch=flat_patch.difference(local_parts).difference(rect(11,-5.8,20,0))
    approach=group_union([front_route.difference(flat_patch),local_parts])
    screen_z=s['Screen'].BoundBox.ZMin
    nominal_p=screen_z-s['FrontRun'].common(Part.makeBox(80,40+FLAT_END_Y,8,V(-40,-40,10))).BoundBox.ZMin
    rear_hold=group_union([*banks,screws,wire,proj('ModuleSubstrate')])
    X,Y=np.meshgrid(GRID_X+PITCH*np.arange(148),GRID_Y+PITCH*np.arange(174))
    tiles=sh.box(X,Y,X+PITCH,Y+PITCH)
    cache={}
    def clip(k,lo,hi):
        b=s[k].BoundBox;lo=max(lo,b.ZMin);hi=min(hi,b.ZMax)
        if hi-lo<.00001:return Polygon()
        key=(k,round(lo,6),round(hi,6))
        if key not in cache:
            cache[key]=(proj(k) if lo<=b.ZMin+.00001 and hi>=b.ZMax-.00001 else
                       shadow(s[k].common(Part.makeBox(100,110,hi-lo,V(-50,-55,lo)))))
        return cache[key]
    masks={};stats={};examples={};gains=[];pending=[]
    audit={'photo_local_rectangles_xy':[[ -18,-14,-8.5,-1.5],[-11.5,-12,5,0],[-18,-4,-8.5,11],[-17,9.5,-7,17],[-8,-21,10,-9]],
           'flat_review_extent_xy':[-7.3,-12.2,15.7,FLAT_END_Y],'narrow_return_held_xy':[11,-5.8,20,0],
           'screen_back_reference_z':screen_z,'flat_projection_mm2':flat_patch.area,
           'nominal_extra_stack_mm':round(nominal_p,5),'front':[], 'rear':[],
           'rf_host_material_mm2':s['Board'].common(Part.makeBox(RF_INNER+30,60,3,V(-RF_INNER if rf_positive_x else -30,-30,B-1))).Volume/(F-B)}
    def grid(allowed):
        sh.prepare(allowed)
        return sh.covers(allowed,tiles)
    def area(m):
        return float(m.sum()*PITCH**2)
    battery_cells=grid(proj('Battery'))
    for side,heights in HEIGHTS.items():
        masks[side]=[];stats[side]=[]
        for h in heights:
            stop=F+h+.20 if side=='Front' else B
            obstruction=[clip(k,-20,stop) for k in fixed]
            if side=='Rear':
                obstruction.extend(clip(k,B-h-.20-(.30 if k=='Battery' else 0),B+.01) for k in rear)
                obstruction.extend([rear_hold,rear_route])
                before_rf=inset.difference(group_union(obstruction).buffer(.22))
                allowed=before_rf.difference(rf)
                m=grid(allowed)
                audit['rear'].append({'height_mm':h,'excluded_by_rf_only_mm2':area(grid(before_rf)&~m),
                                      'usable_fully_under_cell_mm2':area(m&battery_cells)})
            else:
                def available(front):
                    return inset.difference(group_union(obstruction+[front]).buffer(.22)).difference(rf)
                original=grid(available(front_route))
                full=available(group_union([approach,flat_patch]))
                released=available(approach)
                tight,open_grid=grid(full),grid(released)
                limit=round(screen_z-stop,5)
                fits=nominal_p<=limit+1e-6
                allowed,m=(released,open_grid) if fits else (full,tight)
                same_basis=grid(available(front_route.difference(flat_patch))) if fits else original
                pending.append(same_basis&~m)
                gains.append(m&~original)
                audit['front'].append({'height_mm':h,'extra_stack_limit_mm':limit,'previous_area_mm2':area(original),
                         'same_other_constraints_mm2':area(same_basis),'photo_estimated_pending_mm2':area(same_basis&~m),
                         'with_patch_held_mm2':area(tight),'with_patch_clear_mm2':area(open_grid),
                         'added_mm2':area(m&~original),'removed_mm2':area(original&~m)})
            if masks[side]:assert not np.any(m & ~masks[side][-1])
            masks[side].append(m);r=largest_rectangle(m)
            if r:
                x,y,w,l=r;assert allowed.buffer(1e-8).covers(rect(x,y,x+w,y+l))
            stats[side].append({'height_mm':h,'area_mm2':float(m.sum()*PITCH**2),'largest_xywh':r})
    # Search all cells, and both orientations, for the 8 x 6 x 2 test enclosure.
    from scipy.ndimage import distance_transform_edt
    h=2.;idx=HEIGHTS['Rear'].index(h);mask=masks['Rear'][idx];dist=distance_transform_edt(mask)
    from numpy.lib.stride_tricks import sliding_window_view
    candidates=[]
    for w,l in [(8,6),(6,8)]:
        nx=int(w/PITCH);ny=int(l/PITCH);ok=sliding_window_view(mask,(ny,nx)).all(axis=(-2,-1))
        jj,ii=np.where(ok)
        if len(ii):
            score=dist[jj+ny//2,ii+nx//2];a=score.argmax();j,i=jj[a],ii[a]
            candidates.append((float(score[a]),float(GRID_X+i*PITCH),float(GRID_Y+j*PITCH),w,l))
    if candidates:
        _,x,y,w,l=max(candidates)
        sample=box(w,l,h,x+w/2,y+l/2,B-h)
        for k in fixed+rear+REAR_FLEX+['Crossing']:
            if s[k].BoundBox.intersect(sample.BoundBox):assert sample.common(s[k]).Volume<.00001,k
        examples['8x6x2']=[x,y,w,l,h]
    return stats,masks,examples,board,rf,audit,flat_patch,local_parts,gains,pending
