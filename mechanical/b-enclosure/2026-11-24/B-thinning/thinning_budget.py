"""Read-only Z budget for thinning the R2 watch without changing the module, cell, screen or PPG chip.
Values come from the saved R2 assembly and the R2 placement BOM; trial assumptions are listed in ASSUME."""
import csv, hashlib, json
import FreeCAD as App
from workspace import ROOT, WORKSPACE

MODEL = WORKSPACE/'2026-11-22'/'B-crown'/'B-review-R2'/'WristFlow-B-Review-R2.FCStd'
BOM = ROOT/'hardware'/'reviews'/'2026-11-22-b-r2'/'placement-BOM.csv'
TIER = dict(drop=1.40, front=0.30)          # 19.20 mm comparison tier of the R2 review
ASSUME = dict(assembly=0.20, swell=0.50,     # R2 rule under the cell: assembly 0.20 + remaining 0.50
    window=0.50, air=0.10, stiffener=0.20,   # optics trim; 0.20 is a JLC FPC steel stiffener option
    back_clearance=0.15, tray=0.20, tape=0.10, ppg_clearance=0.15,
    front_clearance=0.30, screen=2.46, rim=0.05,
    max30102=1.55, spk_thin=3.00, cell_4mm=4.30, swell_4mm=0.40)

def run(out):
    doc = App.openDocument(str(MODEL))
    try:
        b = {o.Name: o.Shape.BoundBox for o in doc.Objects if hasattr(o, 'Shape') and o.Shape.Solids}
        z0 = lambda k: round(b[k].ZMin, 3)
        z1 = lambda k: round(b[k].ZMax, 3)
        board0, board1 = z0('Board'), z1('Board')
        j201 = round(board0 - min(z0(k) for k in ('CN1', 'FlexPlug', 'RearRun', 'Stiffener')), 3)
        r2 = dict(bump=-z0('Baffle'), bb=z0('Battery'), batt=z1('Battery') - z0('Battery'),
                  ins=z1('Separator') - z1('Battery'), rgap=board0 - z1('Separator'), board=board1 - board0,
                  module=z1('ModuleParts') - board1, fclr=z0('FrontRun') - z1('ModuleParts'),
                  fpc_tape=z0('Screen') - z0('FrontRun'), screen=z1('Screen') - z0('Screen'),
                  rim=z1('Frame') - z1('Screen'))
        r2 = {k: round(v, 3) for k, v in r2.items()}
        hang = {'BatPlug': round(board0 - z0('BatPlug'), 3), 'ShuntCap': round(board0 - z0('KeyBody'), 3)}
        spk_base, spk_body = z0('Speaker'), round(z1('Speaker') - z0('Speaker'), 3)
        motor_below = round(board0 - z0('MotorSeat'), 3)
        window = round(z1('Optic0') - z0('Optic0'), 3)
        sha = hashlib.sha256(MODEL.read_bytes()).hexdigest()
    finally:
        App.closeDocument(doc.Name)

    base = dict(r2, rgap=r2['rgap'] - TIER['drop'], fclr=r2['fclr'] - TIER['front'])
    total = lambda s: round(sum(s.values()), 3)
    a = ASSUME
    ppg_top = round(-r2['bump'] + 0.02 + a['window'] + a['air'] + a['max30102'] + 0.12 + 0.05 + a['stiffener'], 3)
    bb_new = round(max(0.90 + a['back_clearance'] + a['tray'] + a['tape'], ppg_top + a['ppg_clearance']), 3)
    plan = dict(base, bb=bb_new, rgap=round(j201 + a['assembly'] + a['swell'], 3),
                fclr=a['front_clearance'], screen=a['screen'])
    plan['bump'] = round(r2['bump'] - (bb_new - ppg_top - a['ppg_clearance']), 3)

    def strap(s, speaker):
        bottom = s['bb'] + s['batt'] + s['ins'] + s['rgap']
        room = bottom - (spk_base + speaker)
        return {k: round(room - h - a['assembly'], 3) for k, h in hang.items()}

    rows = list(csv.DictReader(BOM.open(encoding='utf-8-sig')))
    tall = sorted(r['位号'] for r in rows if r['板'] == 'main' and '电池投影' in r['局部区域']
                  and float(r['装配Z预算 mm']) > j201)
    bare = dict(plan, rgap=a['assembly'] + a['swell'])
    cell4 = dict(plan, batt=a['cell_4mm'], rgap=round(j201 + a['assembly'] + a['swell_4mm'], 3))
    result = dict(model=str(MODEL.relative_to(WORKSPACE)), model_sha256=sha, tier=TIER, assume=ASSUME,
        r2_layers=r2, r2_total=total(r2), tier_layers=base, tier_total=total(base),
        measured=dict(j201_stack_below_board=j201, window=window, hanging_below_board=hang,
                      motor_seat_below_board=motor_below, motor_raise_needed=round(motor_below - j201, 3),
                      speaker_body=spk_body),
        ppg_top_after_trim=ppg_top, battery_bottom_after_trim=bb_new,
        parts_taller_than_j201_under_cell=tall,
        plan_layers=plan, plan_total=total(plan), plan_body=round(total(plan) - plan['bump'], 3),
        plan_total_flush_rim=total(dict(plan, rim=a['rim'])),
        plan_strap_margin=strap(plan, spk_body),
        reference_bare_rear_face_total=total(bare), reference_bare_strap_margin=strap(bare, spk_body),
        deferred_4mm_cell_total=total(cell4), deferred_4mm_cell_strap_margin=dict(
            speaker_r2=strap(cell4, spk_body), speaker_3p0=strap(cell4, a['spk_thin'])))
    (out/'budget.json').write_text(json.dumps(result, ensure_ascii=False, indent=1), encoding='utf-8')
