"""Generate readable native KiCad schematics for the four B-R2 boards.

Inputs are the locally restored, hash-verified EPRO projects (connectivity, pin names,
part numbers) and the installed KiCad symbol library. The page layout lives in
``schematic_layout.py``. Outputs stay local: KiCad projects, PDF, PNG previews, and an
equivalence report comparing the KiCad netlist against the reviewed EPRO connectivity.
"""
import argparse
import copy
import csv
import json
import math
import re
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
import zipfile
from collections import defaultdict
from pathlib import Path

from kicad_sexpr import Sym, dump, find, first, library_symbols
import schematic_layout

HERE = Path(__file__).resolve().parent
BOM = HERE.parent / 'reviews/2026-11-22-b-r2/placement-BOM.csv'
BOARDS = {
    'main': 'WristFlow-B-R2', 'rear': 'WristFlow-B-Rear-FPC',
    'crown': 'WristFlow-B-Crown', 'bridge': 'WristFlow-B-Crown-Contacts'}
GRID = 1.27
NS = uuid.UUID('6f1d0b52-6a55-4e0f-9b57-2b4f3f7d2a11')
FACE = 'DengXian'


def uid(*parts):
    return Sym(str(uuid.uuid5(NS, '/'.join(map(str, parts)))))


def S(*xs):
    return [Sym(xs[0]), *xs[1:]]


def snap(v):
    return round(round(v / GRID) * GRID, 4)


def font(size=1.27, face=None, bold=False, color=None):
    f = S('font')
    if face:
        f.append(S('face', face))
    f.append(S('size', size, size))
    if bold:
        f.append(S('bold', Sym('yes')))
    if color:
        f.append(S('color', *color))
    return f


def effects(size=1.27, justify=None, hide=False, face=None, bold=False, color=None):
    e = S('effects', font(size, face, bold, color))
    if justify:
        e.append(S('justify', *[Sym(j) for j in justify.split()]))
    if hide:
        e.append(S('hide', Sym('yes')))
    return e


# ---------------------------------------------------------------- EPRO input

def records(data):
    return [r for line in data.splitlines() if line.strip() and (r := json.loads(line))]


def on_segment(x, y, s):
    x1, y1, x2, y2 = s
    close = abs((x - x1) * (y2 - y1) - (y - y1) * (x2 - x1)) < 1e-5
    return close and min(x1, x2) - 1e-5 <= x <= max(x1, x2) + 1e-5 and min(y1, y2) - 1e-5 <= y <= max(y1, y2) + 1e-5


def read_board(path):
    """Read parts, symbol pins and the schematic pin connectivity of one EPRO."""
    with zipfile.ZipFile(path) as z:
        proj = json.loads(z.read('project.json'))
        board = next(iter(proj['boards'].values()))
        pcb = records(z.read('PCB/' + board['pcb'] + '.epcb'))
        cid = {r[3]: r[8] for r in pcb if r[0] == 'ATTR' and r[7] == 'Designator'}
        pads = defaultdict(dict)
        for r in pcb:
            if r[0] == 'PAD_NET':
                pads[cid[r[1]]][str(r[2])] = r[3]
        parts = {}
        for sheet in proj['schematics'][board['schematic']]['sheets']:
            rs = records(z.read(f"SHEET/{board['schematic']}/{sheet['id']}.esch"))
            at = defaultdict(dict)
            for r in rs:
                if r[0] == 'ATTR':
                    at[r[2]][r[3]] = r[4]
            wires = [(at[r[1]]['NET'], seg) for r in rs if r[0] == 'WIRE' for seg in r[2]]
            for c in [r for r in rs if r[0] == 'COMPONENT']:
                a = at[c[1]]
                ref = a['Designator']
                sym = records(z.read('SYMBOL/' + a['Symbol'] + '.esym'))
                active = False
                pinattr = defaultdict(dict)
                raw = []
                for v in sym:
                    if v[0] == 'PART':
                        active = v[1] == c[2]
                    if active and v[0] == 'PIN':
                        raw.append(v)
                    if active and v[0] == 'ATTR':
                        pinattr[v[2]][v[3]] = v[4]
                device = proj['devices'][a['Device']]
                p = parts.setdefault(ref, dict(
                    ref=ref, pins=[], nets={}, sheet=sheet['name'],
                    name=str(a.get('Name', '')).replace('  DNP', ''),
                    dnp=a.get('Add into BOM') == 'no' and device['attributes'].get('Convert to PCB') != 'no',
                    on_board=device['attributes'].get('Convert to PCB') != 'no',
                    device=device['title'],
                    footprint_id=a.get('Footprint', ''),
                    footprint=proj['footprints'].get(a.get('Footprint', ''), {}).get('title', '')))
                for pin in raw:
                    num = str(pinattr[pin[1]]['NUMBER'])
                    name = str(pinattr[pin[1]].get('NAME') or num)
                    px, py = c[3] + pin[4], c[4] + pin[5]
                    touching = {n for n, s in wires if on_segment(px, py, s)}
                    if len(touching) > 1:
                        raise ValueError(f'{ref}.{num} touches {touching}')
                    p['pins'].append(dict(num=num, name=name, x=pin[4], y=pin[5], angle=pin[7] % 360))
                    p['nets'][num] = next(iter(touching), '')
        for ref, nets in pads.items():
            for num, net in nets.items():
                if net and parts[ref]['nets'].get(num) != net:
                    raise ValueError(f'PCB pad {ref}.{num}={net} differs from schematic')
    return parts


def read_bom():
    with BOM.open(encoding='utf-8-sig') as f:
        rows = list(csv.reader(f))
    head = rows[0]
    out = {}
    for r in rows[1:]:
        d = dict(zip(head, r))
        out[d['位号']] = dict(mpn=d['型号'], lcsc=d['LCSC'], footprint=d['封装'])
    return out


# ---------------------------------------------------------------- symbols

class Library:
    """Embedded symbol definitions: adapted stock KiCad symbols and generated boxes."""

    def __init__(self, kicad_symbols):
        self.dir = Path(kicad_symbols)
        self.cache = {}
        self.defs = {}
        self.boxes = set()

    def stock(self, lib, name):
        if lib not in self.cache:
            self.cache[lib] = library_symbols(self.dir / f'{lib}.kicad_sym')
        return self.cache[lib][name]

    def adapt(self, key, lib, name, remap=None, stacked=None):
        """Copy a stock symbol as WF:<key>, renumber pins and optionally stack extra pins."""
        if key in self.defs:
            return self.defs[key]
        sym = copy.deepcopy(self.stock(lib, name))
        sym[1] = 'WF:' + key
        pins = []
        for unit in find(sym, 'symbol'):
            unit[1] = unit[1].replace(name, key, 1)
            for pin in find(unit, 'pin'):
                num = first(pin, 'number')
                if remap and num[1] in remap:
                    num[1] = remap[num[1]]
                if lib != 'power':
                    pin[1] = Sym('passive')
                pins.append((unit, pin))
        for base, extra in (stacked or {}).items():
            for unit, pin in pins:
                if first(pin, 'number')[1] == base:
                    for n in extra:
                        clone = copy.deepcopy(pin)
                        first(clone, 'number')[1] = n
                        clone.append(S('hide', Sym('yes')))
                        unit.append(clone)
        self.defs[key] = sym
        return sym

    def box(self, key, sides, title):
        """Generated IC/connector symbol. sides maps left/right/top/bottom to slot lists.
        A slot is None (gap) or a list of (number, name) stacked at one position."""
        if key in self.defs:
            return self.defs[key]
        pitch = 2.54
        L = 2.54
        nl, nr = len(sides.get('left', [])), len(sides.get('right', []))
        nt, nb = len(sides.get('top', [])), len(sides.get('bottom', []))

        def label(slot):
            if not slot:
                return ''
            name = slot[0][1]
            return name + (f' ×{len(slot)}' if len(slot) > 1 else '')
        wl = max([len(label(s)) for s in sides.get('left', [])] + [0])
        wr = max([len(label(s)) for s in sides.get('right', [])] + [0])
        width = max((wl + wr) * 1.0 + 5.08, (max(nt, nb) + 1) * pitch, 10.16)
        half_w = math.ceil(width / 2 / 2.54) * 2.54
        rows = max(nl, nr, 1)
        half_h = math.ceil(((rows - 1) * pitch / 2 + pitch) / 1.27) * 1.27
        if max(nt, nb) and not (nl or nr):
            half_h = 2.54 * 2
        body = S('symbol', f'{key}_0_1', S('rectangle', S('start', -half_w, half_h), S('end', half_w, -half_h),
                                            S('stroke', S('width', 0.254), S('type', Sym('default'))),
                                            S('fill', S('type', Sym('background')))))
        pins = S('symbol', f'{key}_1_1')

        def add(slot, x, y, angle):
            for i, (num, name) in enumerate(slot):
                shown = label(slot) if i == 0 else name
                if shown == num:
                    shown = '~'
                pin = S('pin', Sym('passive'), Sym('line'), S('at', x, y, angle), S('length', L),
                        S('name', shown, effects(1.0)), S('number', num, effects(1.0)))
                if i:
                    pin.append(S('hide', Sym('yes')))
                pins.append(pin)
        for side, angle in (('left', 0), ('right', 180)):
            slots = sides.get(side, [])
            top = (len(slots) - 1) * pitch / 2
            top = math.floor(top / 1.27 + 1e-6) * 1.27 if len(slots) % 2 == 0 else top
            for i, slot in enumerate(slots):
                if slot:
                    x = -half_w - L if side == 'left' else half_w + L
                    add(slot, x, round(top - i * pitch, 4), angle)
        for side, angle in (('top', 270), ('bottom', 90)):
            slots = sides.get(side, [])
            left = -(len(slots) - 1) * pitch / 2
            left = math.ceil(left / 1.27 - 1e-6) * 1.27
            for i, slot in enumerate(slots):
                if slot:
                    y = half_h + L if side == 'top' else -half_h - L
                    add(slot, round(left + i * pitch, 4), y, angle)
        sym = S('symbol', 'WF:' + key, S('pin_names', S('offset', 0.508)), S('exclude_from_sim', Sym('no')),
                S('in_bom', Sym('yes')), S('on_board', Sym('yes')),
                S('property', 'Reference', 'U', S('at', -half_w, half_h + 1.27, 0), effects(1.27, 'left')),
                S('property', 'Value', title, S('at', -half_w, -half_h - 1.27, 0), effects(1.27, 'left top')),
                S('property', 'Footprint', '', S('at', 0, 0, 0), effects(hide=True)),
                S('property', 'Datasheet', '', S('at', 0, 0, 0), effects(hide=True)),
                body, pins)
        self.defs[key] = sym
        self.boxes.add(key)
        return sym


def pins_of(sym):
    """Pin geometry of a symbol in library coordinates (y up)."""
    out = {}
    for unit in find(sym, 'symbol'):
        for pin in find(unit, 'pin'):
            x, y, a = first(pin, 'at')[1:4]
            out[first(pin, 'number')[1]] = (float(x), float(y), int(float(a)), bool(first(pin, 'hide')))
    return out


# ---------------------------------------------------------------- geometry

def rotate(dx, dy, rot):
    """Screen-space (y down) counter-clockwise rotation used by KiCad symbol placement."""
    r = math.radians(rot)
    c, s = round(math.cos(r)), round(math.sin(r))
    return dx * c + dy * s, -dx * s + dy * c


DIRS = {(1, 0): 'R', (-1, 0): 'L', (0, 1): 'D', (0, -1): 'U'}


class Page:
    def __init__(self, builder, key, title, paper='A3'):
        self.b, self.key, self.title, self.paper = builder, key, title, paper
        self.placed = {}
        self.wires = []
        self.items = []
        self.decor = {}
        self.stubs = {}
        self.bends = {}
        self.extra_decor = []

    # layout API -------------------------------------------------------
    def place(self, ref, x, y, rot=0):
        if ref in self.b.where:
            raise ValueError(f'{ref} placed twice')
        self.b.where[ref] = self
        self.placed[ref] = (snap(x), snap(y), rot)
        return self

    def part(self, ref, x, y, rot=None, top=None, left=None, pin=None):
        """Place a part; choose the rotation from the net that must face up or left,
        and optionally treat (x, y) as the location of one pin end."""
        if rot is None:
            rot = self._orient(ref, top, left)
        if pin:
            if not pin[0].isdigit() and pin not in self.b.symbol_pins(ref):
                pin = self.resolve(f'{ref}@{pin}').rsplit('.', 1)[1]
            lx, ly, _, _ = self.b.symbol_pins(ref)[pin]
            dx, dy = rotate(lx, -ly, rot)
            x, y = x - dx, y - dy
        return self.place(ref, x, y, rot)

    def _orient(self, ref, top, left):
        nets = self.b.parts[ref]['nets']
        pins = self.b.symbol_pins(ref)
        for rot in (0, 180, 90, 270):
            pos = {n: rotate(lx, -ly, rot) for n, (lx, ly, _, hidden) in pins.items() if not hidden}
            mine = [pos[n] for n in pos if nets[n] == (top or left)]
            other = [pos[n] for n in pos if nets[n] != (top or left)]
            if not mine or not other:
                raise ValueError(f'{ref}: cannot orient by {top or left}')
            if top and max(q[1] for q in mine) < min(q[1] for q in other) - 1 and len({round(q[0], 2) for q in mine + other}) == 1:
                return rot
            if left and max(q[0] for q in mine) < min(q[0] for q in other) - 1 and len({round(q[1], 2) for q in mine + other}) == 1:
                return rot
        raise ValueError(f'{ref}: no rotation puts {top or left} first')

    def resolve(self, spec):
        """'REF.PIN' or 'REF@NET' (the single visible pin of REF on NET)."""
        if '@' not in spec:
            return spec
        ref, net = spec.split('@', 1)
        pins = self.b.symbol_pins(ref)
        found = [n for n, v in self.b.parts[ref]['nets'].items() if v == net and n in pins and not pins[n][3]]
        if len(found) != 1:
            raise ValueError(f'{spec}: {len(found)} matching pins')
        return f'{ref}.{found[0]}'

    def pin(self, spec):
        spec = self.resolve(spec)
        ref, num = spec.rsplit('.', 1)
        x, y, rot = self.placed[ref]
        lx, ly, a, _ = self.b.symbol_pins(ref)[num]
        dx, dy = rotate(lx, -ly, rot)
        return snap(x + dx), snap(y + dy)

    def out(self, spec):
        """Outward direction of a pin (from body to the free end) in screen space."""
        spec = self.resolve(spec)
        ref, num = spec.rsplit('.', 1)
        _, _, rot = self.placed[ref]
        _, _, a, _ = self.b.symbol_pins(ref)[num]
        r = math.radians(a + 180)
        dx, dy = rotate(round(math.cos(r)), -round(math.sin(r)), rot)
        return int(dx), int(dy)

    def wire(self, *points, route='auto'):
        """Orthogonal wire through pins ('REF.PIN' / 'REF@NET') and (x, y) points."""
        pts = [self.pin(p) if isinstance(p, str) else (snap(p[0]), snap(p[1])) for p in points]
        names = list(points)
        path = [pts[0]]
        for i in range(1, len(pts)):
            (x0, y0), (x1, y1) = path[-1], pts[i]
            if x0 != x1 and y0 != y1:
                horizontal = route == 'h'
                if route == 'auto':
                    horizontal = True
                    if isinstance(names[i - 1], str):
                        horizontal = self.out(names[i - 1])[1] == 0
                path.append((x1, y0) if horizontal else (x0, y1))
            path.append((x1, y1))
        for a, b in zip(path, path[1:]):
            if a != b:
                self.wires.append((a, b))
        return self

    def stub(self, spec, length):
        self.stubs[self.resolve(spec)] = length
        return self

    def bend(self, spec, direction, run=2.54):
        """Leave the pin outward by `run`, then turn to `direction` before decorating."""
        self.bends[self.resolve(spec)] = (direction, run)
        return self

    def use(self, spec, kind):
        """Force a decoration: 'label', 'global', 'power', or 'none'."""
        self.decor[self.resolve(spec)] = kind
        return self

    def tag(self, x, y, net, direction='R'):
        """Extra label or power symbol at a free wire point."""
        self.extra_decor.append(((snap(x), snap(y)), net, direction))
        return self

    def box(self, x0, y0, x1, y1, title):
        self.items.append(('box', x0, y0, x1, y1, title))
        return self

    def text(self, x, y, s, size=1.6, justify='left bottom', bold=False):
        self.items.append(('text', x, y, s, size, justify, bold))
        return self


class Builder:
    def __init__(self, which, parts, bom, library, power):
        self.which, self.parts, self.bom, self.lib = which, parts, bom, library
        self.power = power
        self.pages = []
        self.where = {}
        self.syms = {}

    def page(self, key, title, paper='A3'):
        p = Page(self, key, title, paper)
        self.pages.append(p)
        return p

    # symbol choice ----------------------------------------------------
    def symbol(self, ref):
        if ref in self.syms:
            return self.syms[ref]
        p = self.parts[ref]
        dev = p['device']
        lib = self.lib
        if re.match(r'R\d', ref) and 'Meter bridge' not in dev:
            key = lib.adapt('R', 'Device', 'R')
        elif re.match(r'C\d', ref):
            key = lib.adapt('C', 'Device', 'C')
        elif re.match(r'L\d', ref):
            key = lib.adapt('FerriteBead', 'Device', 'FerriteBead')
        elif 'Meter bridge' in dev:
            key = lib.adapt('Bridge', 'Jumper', 'SolderJumper_2_Bridged')
        elif ref == 'D19':
            key = lib.adapt('D_Schottky', 'Device', 'D_Schottky')
        elif ref == 'D16':
            key = lib.adapt('D', 'Device', 'D')
        elif dev.startswith('ESD5451N'):
            key = lib.adapt('D_TVS', 'Device', 'D_TVS')
        elif ref == 'D18':
            key = lib.adapt('D_TVS_Uni', 'Device', 'D_Zener')
        elif ref == 'D17':
            key = lib.adapt('D_TVS_Uni_3P', 'Device', 'D_Zener', remap={'1': '3', '2': '1'}, stacked={'1': ['2']})
        elif ref in ('Q1', 'Q2'):
            key = lib.adapt('Q_NMOS_GSD', 'Device', 'Q_NMOS', remap={'G': '1', 'S': '2', 'D': '3'})
        elif ref == 'Q3':
            key = lib.adapt('Q_PMOS_GSD', 'Device', 'Q_PMOS', remap={'G': '1', 'S': '2', 'D': '3'})
        elif ref == 'BT1':
            key = lib.adapt('Battery_Pack', 'Device', 'Battery_Cell', remap={'1': 'P+', '2': 'P-'})
        elif ref == 'TH1':
            key = lib.adapt('Thermistor_NTC', 'Device', 'Thermistor_NTC', remap={'1': 'T', '2': 'RETURN'})
        elif len(p['pins']) == 1:
            key = lib.adapt('Land', 'Connector', 'TestPoint')
        else:
            key = lib.box(self.box_key(ref), self.box_sides(ref), dev)
        self.syms[ref] = key
        return key

    def box_key(self, ref):
        p = self.parts[ref]
        return re.sub(r'[^A-Za-z0-9_+-]', '_', p['device']) + ('_' + ref if ref in schematic_layout.PIN_SIDES else '')

    def box_sides(self, ref):
        p = self.parts[ref]
        names = {q['num']: q['name'] for q in p['pins']}
        custom = schematic_layout.PIN_SIDES.get(ref)
        if custom:
            sides = {}
            used = set()
            for side, slots in custom.items():
                sides[side] = []
                for slot in slots:
                    if slot is None:
                        sides[side].append(None)
                        continue
                    nums = slot if isinstance(slot, (list, tuple)) else [slot]
                    used.update(nums)
                    sides[side].append([(n, names[n]) for n in nums])
            missing = set(names) - used
            if missing:
                raise ValueError(f'{ref} pins not assigned: {sorted(missing)}')
            return sides
        sides = defaultdict(list)
        side_of = {0: 'left', 180: 'right', 90: 'bottom', 270: 'top'}
        order = {'left': lambda q: -q['y'], 'right': lambda q: -q['y'], 'top': lambda q: q['x'], 'bottom': lambda q: q['x']}
        for side in ('left', 'right', 'top', 'bottom'):
            group = [q for q in p['pins'] if side_of[q['angle']] == side]
            for q in sorted(group, key=order[side]):
                sides[side].append([(q['num'], q['name'])])
        return sides

    def symbol_pins(self, ref):
        return pins_of(self.symbol(ref))


# ---------------------------------------------------------------- emit

FLIP = {'left': 'right', 'right': 'left', 'top': 'bottom', 'bottom': 'top'}


def field_at(x, y, rot, justify):
    """Absolute field placement that reads horizontally after the symbol rotation."""
    if rot == 180:
        return S('at', x, y, 0), ' '.join(FLIP.get(j, j) for j in justify.split())
    return S('at', x, y, (360 - rot) % 360), justify


def text_width(s, size=1.27):
    return sum(size * (0.95 if ord(c) < 128 else 1.6) for c in s)


def physical_wire_ends(items, pin_at, project, page):
    """Give every tee a real endpoint and preserve it during editor wire cleanup."""
    wires = find(items, 'wire')
    endpoints = set(pin_at)
    for k in ('label', 'global_label', 'junction', 'symbol'):
        endpoints.update(tuple(first(item, 'at')[1:3]) for item in find(items, k))
    for wire in wires:
        endpoints.update(tuple(pt[1:]) for pt in find(first(wire, 'pts'), 'xy'))
    edges = set()
    for wire in wires:
        a, b = [tuple(pt[1:]) for pt in find(first(wire, 'pts'), 'xy')]
        stops = sorted(p for p in endpoints if on_segment(*p, (*a, *b)))
        edges.update(zip(stops, stops[1:]))
    branches = {tuple(first(j, 'at')[1:3]) for j in find(items, 'junction')}
    # Break only a collinear pair. At a four-way crossing the horizontal
    # pair enters below the tap and the vertical pair enters from its right.
    outgoing = defaultdict(set)
    for a, b in edges:
        horizontal = a[1] == b[1]
        outgoing[a].add(horizontal)
    emitted = []
    for a, b in sorted(edges):
        dx, dy = b[0] - a[0], b[1] - a[1]
        pts = [a]
        # Some KiCad editor load paths merge two collinear segments at a tap
        # before registering its symbol or third wire. Separate their axes
        # locally, without moving the pin, label, or junction.
        if b in branches and bool(dx) in outgoing.get(b, set()) and abs(dx) + abs(dy) > 1.27:
            if dx:
                pts.extend([(round(b[0]-1.27, 4), a[1]), (round(b[0]-1.27, 4), round(b[1]-1.27, 4))])
            else:
                pts.extend([(a[0], round(b[1]-1.27, 4)), (round(b[0]+1.27, 4), round(b[1]-1.27, 4))])
        pts.append(b)
        for start, end in zip(pts, pts[1:]):
            emitted.append(S('wire', S('pts', S('xy', *start), S('xy', *end)),
                             S('stroke', S('width', 0), S('type', Sym('default'))),
                             S('uuid', uid(project, page, 'wire', *start, *end))))
    return [item for item in items if item[0] != 'wire'] + emitted


def compile_board(builder, project, outdir, date):
    parts = builder.parts
    missing = set(parts) - set(builder.where)
    if missing:
        raise ValueError(f'{project}: unplaced parts {sorted(missing)}')
    page_of_net = defaultdict(set)
    for ref, p in parts.items():
        for num, n in p['nets'].items():
            if n:
                page_of_net[n].add(builder.where[ref].key)
    root = uid(project, 'root')
    all_libs = {}
    hier = len(builder.pages) > 1
    power_counter = [0]
    sheet_files = []
    for page in builder.pages:
        sheet_uuid = uid(project, 'sheet', page.key)
        path = f'/{root}/{sheet_uuid}' if hier else f'/{root}'
        items = []
        libs = {}
        junction_points = set()
        # Pin endpoints and their nets.
        pin_at = {}
        endpoints = []
        for ref, (x, y, rot) in page.placed.items():
            p = parts[ref]
            sym = builder.symbol(ref)
            libs[sym[1]] = sym
            for num, (lx, ly, a, hidden) in pins_of(sym).items():
                spec = f'{ref}.{num}'
                pt = page.pin(spec)
                net = p['nets'].get(num, '')
                if num not in p['nets']:
                    raise ValueError(f'{spec} is not a pin of the reviewed part')
                if pt in pin_at and pin_at[pt][1] != net:
                    raise ValueError(f'{spec} overlaps {pin_at[pt][0]} with a different net')
                pin_at.setdefault(pt, (spec, net))
                endpoints.append((spec, pt, net, hidden))
        # Wire connectivity (union-find on points).
        parent = {}

        def find_root(a):
            parent.setdefault(a, a)
            while parent[a] != a:
                parent[a] = parent[parent[a]]
                a = parent[a]
            return a

        def union(a, b):
            parent[find_root(a)] = find_root(b)
        wire_pts = set()
        for a, b in page.wires:
            if a[0] != b[0] and a[1] != b[1]:
                raise ValueError(f'{page.key}: diagonal wire {a}-{b}')
            union(a, b)
            wire_pts.update([a, b])
        for a, b in page.wires:
            for pt in list(wire_pts) + list(pin_at):
                if pt not in (a, b) and on_segment(pt[0], pt[1], (*a, *b)):
                    if pt in pin_at and pt not in wire_pts:
                        raise ValueError(f'{page.key}: pin {pin_at[pt][0]} lies on a wire interior')
                    union(pt, a)
                    junction_points.add(pt)
        degree = defaultdict(int)
        for a, b in page.wires:
            degree[a] += 1
            degree[b] += 1
        for pt, d in degree.items():
            if d >= 3 or (d >= 2 and pt in pin_at):
                junction_points.add(pt)
        group_net = {}
        for pt, (spec, net) in pin_at.items():
            if pt in wire_pts:
                g = find_root(pt)
                if group_net.setdefault(g, net) != net:
                    raise ValueError(f'{page.key}: wire joins {spec}={net} with {group_net[g]}')
        wired_pins = {pt for pt in pin_at if pt in wire_pts}
        decorated_groups = set()

        def power_symbol(pt, net, direction):
            power_counter[0] += 1
            ref = f'#PWR{power_counter[0]:03d}'
            ground = builder.power.get(net) == 'gnd'
            key = 'GND' if ground else 'VCC'
            sym = builder.lib.adapt('PWR_' + key, 'power', key)
            libs[sym[1]] = sym
            up = direction == 'U' if ground else direction != 'D'
            rot = (180 if up else 0) if ground else (0 if up else 180)
            x, y = pt
            ty = y - 3.4 if up else y + 3.4
            tx, just = x, ('bottom' if up else 'top')
            items.append(S('symbol', S('lib_id', sym[1]), S('at', x, y, rot), S('unit', 1),
                           S('exclude_from_sim', Sym('no')), S('in_bom', Sym('yes')), S('on_board', Sym('yes')),
                           S('dnp', Sym('no')), S('uuid', uid(project, page.key, ref)),
                           S('property', 'Reference', ref, S('at', x, y, 0), effects(hide=True)),
                           S('property', 'Value', net, field_at(tx, ty, rot, just)[0], effects(1.0, field_at(tx, ty, rot, just)[1])),
                           S('property', 'Footprint', '', S('at', x, y, 0), effects(hide=True)),
                           S('property', 'Datasheet', '', S('at', x, y, 0), effects(hide=True)),
                           S('pin', '1', S('uuid', uid(project, page.key, ref, 'pin'))),
                           S('instances', S('project', project, S('path', path, S('reference', ref), S('unit', 1))))))

        def label(pt, net, direction, kind):
            x, y = pt
            angle = {'R': 0, 'U': 90, 'L': 180, 'D': 270}[direction]
            if kind == 'global':
                just = 'left' if direction in 'RU' else 'right'
                items.append(S('global_label', net, S('shape', Sym('passive')), S('at', x, y, angle),
                               S('fields_autoplaced', Sym('yes')), effects(1.27, just),
                               S('uuid', uid(project, page.key, 'gl', x, y, net)),
                               S('property', 'Intersheetrefs', '${INTERSHEET_REFS}', S('at', x, y, 0),
                                 effects(1.27, just, hide=True))))
            else:
                just = 'left bottom' if direction in 'RU' else 'right bottom'
                items.append(S('label', net, S('at', x, y, angle), S('fields_autoplaced', Sym('yes')),
                               effects(1.27, just), S('uuid', uid(project, page.key, 'lb', x, y, net))))

        def kind_of(net, force=None):
            return force or ('power' if net in builder.power else ('global' if len(page_of_net[net]) > 1 else 'label'))

        def decorate(pt, net, direction, kind=None):
            kind = kind or kind_of(net)
            if kind == 'power':
                power_symbol(pt, net, direction)
            else:
                label(pt, net, direction, kind)

        stub_default = {'power': 2.54, 'global': 2.54, 'label': None}
        for spec, pt, net, hidden in endpoints:
            if hidden or pt in wired_pins:
                continue
            if pin_at[pt][0] != spec:
                continue  # stacked pin, handled by the visible one
            if not net:
                items.append(S('no_connect', S('at', *pt), S('uuid', uid(project, page.key, 'nc', spec))))
                continue
            kind = kind_of(net, page.decor.get(spec))
            if kind == 'none':
                continue
            dx, dy = page.out(spec)
            direction = DIRS[(dx, dy)]
            if spec in page.bends:
                turn, run = page.bends[spec]
                corner = (snap(pt[0] + dx * run), snap(pt[1] + dy * run))
                items.append(S('wire', S('pts', S('xy', *pt), S('xy', *corner)), S('stroke', S('width', 0), S('type', Sym('default'))),
                               S('uuid', uid(project, page.key, 'bend', spec))))
                pt = corner
                direction = turn
                dx, dy = {'R': (1, 0), 'L': (-1, 0), 'U': (0, -1), 'D': (0, 1)}[turn]
            length = page.stubs.get(spec)
            if length is None and kind == 'power':
                length = 0 if dy else 5.08
            if length is None:
                length = stub_default[kind] if stub_default[kind] is not None else snap(text_width(net) + 2.54)
            end = (snap(pt[0] + dx * length), snap(pt[1] + dy * length))
            if length:
                items.append(S('wire', S('pts', S('xy', *pt), S('xy', *end)), S('stroke', S('width', 0), S('type', Sym('default'))),
                               S('uuid', uid(project, page.key, 'stub', spec))))
            if kind == 'label':
                back = {'R': 'L', 'L': 'R', 'U': 'D', 'D': 'U'}[direction]
                label(end if length else pt, net, back if length else direction, 'label')
            else:
                decorate(end, net, direction, kind)
        # Free wire ends and wire groups without a name.
        for pt, net, direction in page.extra_decor:
            if pt not in wire_pts:
                raise ValueError(f'{page.key}: tag {net} at {pt} is not on a wire end')
            g = find_root(pt)
            if group_net.get(g) not in (None, net):
                raise ValueError(f'{page.key}: tag {net} on {group_net[g]}')
            group_net[g] = net
            decorate(pt, net, direction)
            decorated_groups.add(g)
        for g, net in group_net.items():
            if g in decorated_groups or net in builder.power:
                if net in builder.power and g not in decorated_groups:
                    raise ValueError(f'{page.key}: wired power net {net} needs a tag')
                continue
            segs = [(a, b) for a, b in page.wires if find_root(a) == g]
            a, b = max(segs, key=lambda s: (s[0][1] == s[1][1], abs(s[0][0] - s[1][0]) + abs(s[0][1] - s[1][1])))
            if a[1] == b[1]:
                span = abs(a[0] - b[0])
                x = snap(min(a[0], b[0]) + max(1.27, (span - text_width(net)) / 2))
                label((x, a[1]), net, 'R', kind_of(net))
            else:
                y = snap(max(a[1], b[1]) - 1.27)
                label((a[0], y), net, 'U', kind_of(net))
        for a, b in page.wires:
            items.append(S('wire', S('pts', S('xy', *a), S('xy', *b)), S('stroke', S('width', 0), S('type', Sym('default'))),
                           S('uuid', uid(project, page.key, 'w', *a, *b))))
        for pt in sorted(junction_points):
            items.append(S('junction', S('at', *pt), S('diameter', 0), S('color', 0, 0, 0, 0),
                           S('uuid', uid(project, page.key, 'j', *pt))))
        items = physical_wire_ends(items, pin_at, project, page.key)
        # Parts.
        for ref, (x, y, rot) in page.placed.items():
            p = parts[ref]
            sym = builder.symbol(ref)
            bom = builder.bom.get(ref, {})
            value = schematic_layout.VALUE_TEXT.get(p['name'], p['name'])
            lib_pins = pins_of(sym)
            xs, ys = [], []
            for num in lib_pins:
                px, py = page.pin(f'{ref}.{num}')
                xs.append(px)
                ys.append(py)
            vertical = (max(ys) - min(ys)) > (max(xs) - min(xs)) if len(lib_pins) > 1 else True
            boxed = sym[1][3:] in builder.lib.boxes
            if boxed:
                body = [u for u in find(sym, 'symbol') if first(u, 'rectangle')]
                sx, sy = first(first(body[0], 'rectangle'), 'start')[1:3]
                ref_at = (x + sx, y - sy - 3.6, 'left bottom')
                val_at = (x + sx, y - sy - 1.0, 'left bottom')
            elif len(lib_pins) == 1:
                if rot == 0:
                    ref_at = (x + 1.6, y - 5.4, 'left bottom')
                    val_at = (x + 1.6, y - 3.9, 'left bottom')
                else:
                    side = 1 if rot == 270 else -1
                    ref_at = (x + side * 5.4, y - 0.4, ('left' if side > 0 else 'right') + ' bottom')
                    val_at = (x + side * 5.4, y + 0.4, ('left' if side > 0 else 'right') + ' top')
            elif len(lib_pins) == 3 and not boxed:
                ref_at = (x + 5.08, y - 0.6, 'left bottom')
                val_at = (x + 5.08, y + 0.6, 'left top')
            elif vertical:
                ref_at = (x + 2.0, y - 0.6, 'left bottom')
                val_at = (x + 2.0, y + 0.6, 'left top')
            else:
                ref_at = (x, y - 2.3, 'bottom')
                val_at = (x, y + 2.3, 'top')
            over = schematic_layout.LABEL_AT.get(ref)
            if over:
                ref_at = (x + over[0][0], y + over[0][1], over[0][2])
                val_at = (x + over[1][0], y + over[1][1], over[1][2])
            in_bom = p['on_board']
            # Catalog titles can repeat across distinct land patterns in the EPRO.
            land_name = re.sub(r'[^A-Za-z0-9_.+-]', '_', p['footprint'])
            land_id = f"WF_B:{land_name}-{p['footprint_id'][:8]}" if p['on_board'] else ''
            props = [
                S('property', 'Reference', ref, field_at(ref_at[0], ref_at[1], rot, ref_at[2])[0],
                  effects(1.27, field_at(ref_at[0], ref_at[1], rot, ref_at[2])[1])),
                S('property', 'Value', value, field_at(val_at[0], val_at[1], rot, val_at[2])[0],
                  effects(1.1, field_at(val_at[0], val_at[1], rot, val_at[2])[1])),
                S('property', 'Footprint', land_id, S('at', x, y, 0), effects(hide=True)),
                S('property', 'Datasheet', '', S('at', x, y, 0), effects(hide=True)),
                S('property', 'MPN', bom.get('mpn', p['device']), S('at', x, y, 0), effects(hide=True)),
                S('property', 'LCSC', bom.get('lcsc', ''), S('at', x, y, 0), effects(hide=True)),
                S('property', 'EPRO_Footprint', bom.get('footprint', p['footprint']), S('at', x, y, 0), effects(hide=True)),
            ]
            items.append(S('symbol', S('lib_id', sym[1]), S('at', x, y, rot), S('unit', 1),
                           S('exclude_from_sim', Sym('no')), S('in_bom', Sym('yes' if in_bom else 'no')),
                           S('on_board', Sym('yes' if in_bom else 'no')), S('dnp', Sym('yes' if p['dnp'] else 'no')),
                           S('uuid', uid(project, ref)), *props,
                           *[S('pin', n, S('uuid', uid(project, ref, 'pin', n))) for n in lib_pins],
                           S('instances', S('project', project, S('path', path, S('reference', ref), S('unit', 1))))))
        # Frames and text.
        for it in page.items:
            if it[0] == 'box':
                _, x0, y0, x1, y1, title = it
                items.append(S('rectangle', S('start', x0, y0), S('end', x1, y1),
                               S('stroke', S('width', 0.3), S('type', Sym('solid')), S('color', 40, 60, 170, 1)),
                               S('fill', S('type', Sym('none'))), S('uuid', uid(project, page.key, 'box', title))))
                items.append(S('text', title, S('exclude_from_sim', Sym('no')), S('at', x1 - 1.5, y1 - 1.5, 0),
                               effects(2.2, 'right bottom', face=FACE, bold=True, color=(40, 60, 170, 1)),
                               S('uuid', uid(project, page.key, 'boxt', title))))
            else:
                _, x, y, s, size, just, bold = it
                items.append(S('text', s, S('exclude_from_sim', Sym('no')), S('at', x, y, 0),
                               effects(size, just, face=FACE, bold=bold),
                               S('uuid', uid(project, page.key, 'txt', x, y))))
        title_block = S('title_block', S('title', f'{project}  {page.title}'), S('date', date), S('rev', 'B-R2 评审版'),
                        S('company', 'WristFlow'), S('comment', 1, '由 hardware/generator/kicad_schematic.py 生成；电路与 2026-11-22 评审版 EPRO 逐引脚一致'),
                        S('comment', 2, 'PCB 使用项目封装库 WF_B，与本工程同名'))
        doc = S('kicad_sch', S('version', 20250114), S('generator', 'wristflow_kicad_schematic'),
                S('generator_version', '1'), S('uuid', sheet_uuid if hier else root), S('paper', page.paper), title_block,
                S('lib_symbols', *sorted(libs.values(), key=lambda s: s[1])), *items)
        all_libs.update(libs)
        name = f'{project}.kicad_sch' if not hier else f'{page.key}.kicad_sch'
        if not hier:
            doc.append(S('sheet_instances', S('path', '/', S('page', '1'))))
        (outdir / name).write_text(dump(doc) + '\n', encoding='utf-8')
        sheet_files.append((page, name, sheet_uuid))
    if hier:
        write_root(builder, project, outdir, root, sheet_files, date)
    write_library(outdir, all_libs)
    (outdir / f'{project}.kicad_pro').write_text(json.dumps(dict(
        meta=dict(filename=f'{project}.kicad_pro', version=3),
        sheets=[[str(root), 'Root']] + [[str(u), p.title] for p, _, u in sheet_files] if hier else [[str(root), 'Root']],
        schematic=dict(drawing=dict(default_text_size=50.0)), text_variables={}), indent=2), encoding='utf-8')
    return outdir / f'{project}.kicad_sch'


def write_library(outdir, libs):
    """Project library WF.kicad_sym matching the embedded symbols, plus its table."""
    symbols = []
    for name, sym in sorted(libs.items()):
        local = copy.deepcopy(sym)
        local[1] = name.split(':', 1)[1]
        symbols.append(local)
    doc = S('kicad_symbol_lib', S('version', 20241209), S('generator', 'wristflow_kicad_schematic'),
            S('generator_version', '1'), *symbols)
    (outdir / 'WF.kicad_sym').write_text(dump(doc) + '\n', encoding='utf-8')
    table = ('(sym_lib_table\n\t(version 7)\n\t(lib (name "WF") (type "KiCad") (uri "${KIPRJMOD}/WF.kicad_sym")'
             ' (options "") (descr "WristFlow B-R2 generated symbols"))\n)\n')
    (outdir / 'sym-lib-table').write_text(table, encoding='utf-8')


def write_root(builder, project, outdir, root, sheet_files, date):
    items = []
    overview = schematic_layout.OVERVIEW.get(builder.which, [])
    for i, (page, name, su) in enumerate(sheet_files):
        col, row = i % 3, i // 3
        x, y = 15 + col * 92, 45 + row * 52
        w, h = 82, 32
        items.append(S('sheet', S('at', x, y), S('size', w, h), S('exclude_from_sim', Sym('no')),
                       S('in_bom', Sym('yes')), S('on_board', Sym('yes')), S('dnp', Sym('no')),
                       S('fields_autoplaced', Sym('yes')),
                       S('stroke', S('width', 0.3), S('type', Sym('solid')), S('color', 40, 60, 170, 1)),
                       S('fill', S('color', 235, 240, 255, 1)), S('uuid', su),
                       S('property', 'Sheetname', page.title, S('at', x, y - 0.7, 0), effects(1.8, 'left bottom', face=FACE, bold=True)),
                       S('property', 'Sheetfile', name, S('at', x, y + h + 0.6, 0), effects(1.27, 'left top')),
                       S('instances', S('project', project, S('path', f'/{root}', S('page', str(i + 2)))))))
        for j, line in enumerate(schematic_layout.PAGE_SUMMARY.get(page.key, [])):
            items.append(S('text', line, S('exclude_from_sim', Sym('no')), S('at', x + 3, y + 7 + j * 5, 0),
                           effects(1.4, 'left bottom', face=FACE), S('uuid', uid(project, 'sum', page.key, j))))
    items.append(S('text', f'{project} 原理图（KiCad 原生，可读版）', S('exclude_from_sim', Sym('no')), S('at', 15, 28, 0),
                   effects(3.2, 'left bottom', face=FACE, bold=True), S('uuid', uid(project, 'roottitle'))))
    for j, line in enumerate(overview):
        items.append(S('text', line, S('exclude_from_sim', Sym('no')), S('at', 15, 152 + j * 5.5, 0),
                       effects(1.6, 'left bottom', face=FACE), S('uuid', uid(project, 'note', j))))
    title_block = S('title_block', S('title', f'{project}  目录与说明'), S('date', date), S('rev', 'B-R2 评审版'),
                    S('company', 'WristFlow'), S('comment', 1, '由 hardware/generator/kicad_schematic.py 生成；电路与 2026-11-22 评审版 EPRO 逐引脚一致'),
                    S('comment', 2, 'PCB 使用项目封装库 WF_B，与本工程同名'))
    doc = S('kicad_sch', S('version', 20250114), S('generator', 'wristflow_kicad_schematic'), S('generator_version', '1'),
            S('uuid', root), S('paper', 'A4'), title_block, S('lib_symbols'), *items,
            S('sheet_instances', S('path', '/', S('page', '1'))))
    (outdir / f'{project}.kicad_sch').write_text(dump(doc) + '\n', encoding='utf-8')


# ---------------------------------------------------------------- equivalence

def strip(name):
    return name.rsplit('/', 1)[-1] if name.startswith('/') else name


def equivalence(parts, bom, netlist):
    tree = ET.parse(netlist)
    got_nets = {}
    raw = {}
    for net in tree.findall('.//nets/net'):
        name = net.get('name')
        for node in net.findall('node'):
            got_nets[node.get('ref'), node.get('pin')] = '' if name.startswith('unconnected-') else strip(name)
            raw[node.get('ref'), node.get('pin')] = name
    comps = {}
    for c in tree.findall('.//components/comp'):
        fields = {f.get('name'): (f.text or '') for f in c.findall('fields/field')}
        props = {p.get('name'): p.get('value') for p in c.findall('property')}
        comps[c.get('ref')] = dict(value=c.findtext('value') or '', fields=fields, props=props)
    want = {(ref, num): n for ref, p in parts.items() for num, n in p['nets'].items()}
    pin_mismatch = [dict(pin=f'{r}.{n}', reviewed=want[r, n], kicad=got_nets.get((r, n)))
                    for r, n in sorted(want) if got_nets.get((r, n), '') != want[r, n]]
    extra_pins = sorted(f'{r}.{n}' for r, n in got_nets if (r, n) not in want)
    missing_pins = sorted(f'{r}.{n}' for r, n in want if (r, n) not in got_nets)
    part_mismatch = []
    for ref, p in parts.items():
        c = comps.get(ref)
        if not c:
            part_mismatch.append(dict(ref=ref, issue='missing'))
            continue
        b = bom.get(ref, {})
        expect = dict(MPN=b.get('mpn', p['device']), LCSC=b.get('lcsc', ''), EPRO_Footprint=b.get('footprint', p['footprint']))
        for k, v in expect.items():
            if c['fields'].get(k, '') != v:
                part_mismatch.append(dict(ref=ref, field=k, reviewed=v, kicad=c['fields'].get(k)))
        if ('dnp' in c['props']) != p['dnp']:
            part_mismatch.append(dict(ref=ref, field='DNP', reviewed=p['dnp'], kicad='dnp' in c['props']))
        if ('exclude_from_board' in c['props']) == p['on_board']:
            part_mismatch.append(dict(ref=ref, field='on_board', reviewed=p['on_board']))
    extra_parts = sorted(set(comps) - set(parts))
    nets = sorted({n for n in want.values() if n})
    # Partition check: every reviewed net is exactly one KiCad net and vice versa.
    groups = defaultdict(set)
    owners = defaultdict(set)
    for key, n in want.items():
        if n and key in raw:
            groups[n].add(raw[key])
            owners[raw[key]].add(n)
    split = {n: sorted(g) for n, g in groups.items() if len(g) != 1}
    merged = {k: sorted(v) for k, v in owners.items() if len(v) != 1}
    nc_shared = sorted(k for k, v in raw.items() if want.get(k) == '' and sum(1 for x in raw.values() if x == v) > 1)
    return dict(parts=len(parts), kicad_parts=len(comps), pins=len(want), connected_pins=sum(bool(n) for n in want.values()),
                nets=len(nets), pin_mismatches=pin_mismatch, extra_kicad_pins=extra_pins, missing_kicad_pins=missing_pins,
                part_mismatches=part_mismatch, extra_kicad_parts=extra_parts, split_nets=split, merged_nets=merged,
                shared_no_connect=nc_shared,
                equivalent=not (pin_mismatch or extra_pins or missing_pins or part_mismatch or extra_parts
                                or split or merged or nc_shared))


def compact_pdf(pdf):
    """KiCad embeds whole CJK fonts; subset them when PyMuPDF is available."""
    try:
        import pymupdf
    except ImportError:
        return
    doc = pymupdf.open(pdf)
    doc.subset_fonts()
    tmp = pdf.with_suffix('.tmp.pdf')
    doc.save(tmp, garbage=4, deflate=True)
    doc.close()
    tmp.replace(pdf)


def netlist_pins(path):
    out = {}
    for net in ET.parse(path).findall('.//nets/net'):
        name = net.get('name')
        for node in net.findall('node'):
            out[node.get('ref'), node.get('pin')] = '' if name.startswith('unconnected-') else strip(name)
    return out


def cross_check(ours, reference):
    """Compare our KiCad netlist with verify.py's independent KiCad solve of the reviewed EPRO."""
    a, b = netlist_pins(ours), netlist_pins(reference)
    keys = {k for k, v in a.items() if v} | {k for k, v in b.items() if v}
    diff = sorted(f'{r}.{n}: ours={a.get((r, n))!r} verify={b.get((r, n))!r}' for r, n in keys if a.get((r, n)) != b.get((r, n)))
    return dict(reference=str(reference), compared_pins=len(keys), differences=diff)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--projects', type=Path, required=True, help='Directory with the four restored reviewed .epro files')
    ap.add_argument('--out', type=Path, required=True, help='New output directory')
    ap.add_argument('--kicad-symbols', type=Path, required=True, help='KiCad share/kicad/symbols directory')
    ap.add_argument('--kicad-cli', help='kicad-cli executable: export PDF and netlist, run ERC and equivalence')
    ap.add_argument('--date', default='2026-11-25')
    ap.add_argument('--png', action='store_true', help='Also render PNG previews with PyMuPDF')
    ap.add_argument('--force', action='store_true', help='Replace an existing output directory')
    args = ap.parse_args()
    if args.out.exists():
        if not args.force:
            raise FileExistsError(f'Choose a new output directory: {args.out}')
        shutil.rmtree(args.out)
    args.out.mkdir(parents=True)
    bom = read_bom()
    lib = Library(args.kicad_symbols)
    summary = {}
    for which, project in BOARDS.items():
        parts = read_board(args.projects / f'{project}.epro')
        b = Builder(which, parts, bom, lib, schematic_layout.POWER)
        schematic_layout.LAYOUTS[which](b)
        outdir = args.out / project
        outdir.mkdir()
        sch = compile_board(b, project, outdir, args.date)
        if not args.kicad_cli:
            continue
        net = outdir / 'netlist.xml'
        subprocess.run([args.kicad_cli, 'sch', 'export', 'netlist', '--format', 'kicadxml', '-o', net, sch], check=True, stdout=subprocess.DEVNULL)
        pdf = outdir / f'{project}.pdf'
        subprocess.run([args.kicad_cli, 'sch', 'export', 'pdf', '-o', pdf, sch], check=True, stdout=subprocess.DEVNULL)
        compact_pdf(pdf)
        erc = outdir / 'erc.json'
        subprocess.run([args.kicad_cli, 'sch', 'erc', '--format', 'json', '-o', erc, sch], check=True, stdout=subprocess.DEVNULL)
        violations = [v for s in json.loads(erc.read_text(encoding='utf-8')).get('sheets', []) for v in s['violations']]
        types = defaultdict(int)
        for v in violations:
            types[v['type']] += 1
        result = equivalence(parts, bom, net)
        reference = args.projects / 'native-qa' / project / 'compiled.xml'
        if reference.exists():
            result['verify_py_cross_check'] = cross_check(net, reference)
            if result['verify_py_cross_check']['differences']:
                result['equivalent'] = False
        result.update(pages=len(b.pages), erc=dict(sorted(types.items())))
        summary[project] = result
        print(project, 'equivalent' if result['equivalent'] else 'MISMATCH', result['parts'], 'parts',
              result['connected_pins'], 'connected pins', result['nets'], 'nets', dict(types))
        if args.png:
            import pymupdf
            doc = pymupdf.open(pdf)
            for i, pg in enumerate(doc):
                pg.get_pixmap(dpi=110).save(outdir / f'page-{i + 1}.png')
    if summary:
        try:
            import pymupdf
        except ImportError:
            pymupdf = None
        if pymupdf:
            book = pymupdf.open()
            for project in BOARDS.values():
                book.insert_pdf(pymupdf.open(args.out / project / f'{project}.pdf'))
            book.save(args.out / 'WristFlow-B-R2-schematics.pdf', garbage=4, deflate=True)
        (args.out / 'equivalence.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')
        if not all(r['equivalent'] for r in summary.values()):
            raise SystemExit('KiCad netlist differs from the reviewed connectivity; see equivalence.json')


if __name__ == '__main__':
    main()
