"""Draw both placement maps in the same PCB coordinates using the native height zones."""
import subprocess
from os import environ
from shutil import which
from pathlib import Path
from xml.sax.saxutils import escape

from PIL import Image
from shapely import union_all
from shapely.geometry import Polygon
from shapely.ops import polylabel
from layout_space import shadow

WIDTH, HEIGHT = 1520, 1120
INK = '#172e37'
MUTED = '#4d656e'
BOARD = '#586975'
BACKGROUND = '#edf2f3'
COLORS = {.6:'#ffd166', .7:'#e98b46', 1:'#b9d78d', 2:'#77c6a4', 3:'#3eadae', 4:'#6298cd', 6:'#685aa8'}


def render_map(root, model, watch, report):
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}" font-family="Microsoft YaHei, sans-serif">',
             '''<defs>
             <pattern id="pending" patternUnits="userSpaceOnUse" width="9" height="9"><rect width="9" height="9" fill="#a5afb6"/><path d="M-2 2L2 -2M0 9L9 0M7 11L11 7" stroke="#4e5b65" stroke-width="2"/></pattern>
             <pattern id="fpc" patternUnits="userSpaceOnUse" width="8" height="8"><rect width="8" height="8" fill="#455660"/><circle cx="4" cy="4" r="1" fill="#9da9af"/></pattern>
             <pattern id="rf" patternUnits="userSpaceOnUse" width="9" height="9"><rect width="9" height="9" fill="#495965"/><path d="M0 4.5H9" stroke="#a8b3ba" stroke-width="1"/></pattern>
             </defs>''']

    def rect(x, y, w, h, fill, radius=0):
        parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}"/>')

    def text(x, y, label, size, color=INK, weight=400, anchor='start', extra=''):
        parts.append(f'<text x="{x:.2f}" y="{y:.2f}" text-anchor="{anchor}" fill="{color}" font-size="{size}" font-weight="{weight}" {extra}>{escape(str(label))}</text>')

    def line(x1, y1, x2, y2, color, width=1, dash=''):
        parts.append(f'<path d="M{x1} {y1} L{x2} {y2}" stroke="{color}" stroke-width="{width}" stroke-dasharray="{dash or "none"}"/>')

    def xy(x, y, side):
        return (383 if side=='Front' else 1136)+12.0*x, 508-12.0*y

    def shape(geom, side, fill, stroke='none', width=1, dash=''):
        if geom.is_empty:
            return
        if geom.geom_type!='Polygon':
            for part in geom.geoms:
                if part.geom_type in ('Polygon','MultiPolygon','GeometryCollection'):
                    shape(part,side,fill,stroke,width,dash)
            return
        rings=[]
        for ring in (geom.exterior,*geom.interiors):
            points=[xy(x,y,side) for x,y in ring.coords]
            rings.append('M'+' L'.join(f'{x:.2f},{y:.2f}' for x,y in points)+' Z')
        path=' '.join(rings)
        parts.append(f'<path d="{path}" fill="{fill}" fill-rule="evenodd" stroke="{stroke}" stroke-width="{width}" stroke-linejoin="round" stroke-dasharray="{dash or "none"}"/>')

    board_shape=model.Board.Shape
    top=max((f for f in board_shape.Faces if abs(f.BoundBox.ZMin-board_shape.BoundBox.ZMax)<1e-6),key=lambda f:f.Area)
    wires=[top.OuterWire]+[w for w in top.Wires if not w.isSame(top.OuterWire)]
    loops=[[(p.x,p.y) for p in w.discretize(Deflection=.001)] for w in wires]
    board=Polygon(loops[0],loops[1:])
    assert abs(board.area-report['board_mm2'])<.035 and len(loops)==5
    rect(0,0,WIDTH,HEIGHT,'white')
    text(50,65,'B 版主板 · 器件焊后限高',45,weight=700)
    text(52,113,'两面按同一板框对齐。穿线窗朝上，表冠在右，电池侧也沿用屏幕侧的坐标。',23,MUTED)

    for side,title,cx,left in [('Front','屏幕侧',383,37),('Rear','电池侧',1136,790)]:
        rect(left,147,693,650,BACKGROUND,16)
        text(left+24,200,title,33,weight=700)
        focus=.6 if side=='Front' else 3.
        rows=report['heights'][side]
        cumulative={r['height_mm']:r['area_mm2'] for r in rows}
        text(left+666,197,f'{focus:g} mm 器件可摆 {cumulative[focus]:,.2f} mm²',21,MUTED,anchor='end')
        bands={obj.Height.Value:shadow(obj.Shape) for obj in model.getObject(side).Group}
        for i,row in enumerate(rows):
            h=row['height_mm']
            area=row['area_mm2']-(rows[i+1]['area_mm2'] if i+1<len(rows) else 0.)
            assert abs(bands.get(h,Polygon()).area-area)<.01,(side,h,area)
        allowed=union_all(list(bands.values()))
        assert abs(allowed.area-sum(g.area for g in bands.values()))<.01
        shape(board,side,BOARD)
        shape(shadow(model.getObject('Reserved'+side).Shape).intersection(board).difference(allowed),side,'url(#rf)')
        if side=='Front':
            pending=shadow(model.HeightPending.Shape)
            assert abs(pending.area-report['space_audit']['front'][0]['photo_estimated_pending_mm2'])<.01
            shape(shadow(model.FrontFilm.Shape).intersection(board).difference(allowed),side,'url(#fpc)')
            shape(pending,side,'url(#pending)')
        else:
            for name in ['ModuleParts','RearRun','CN1']:
                shape(shadow(model.getObject(name).Shape),side,'#344650','#b4bec2',1)
            shape(shadow(model.Radiator.Shape),side,'none','#637582',2,'5 4')
        for h,geom in sorted(bands.items()):
            shape(geom,side,COLORS[h])
        shape(board,side,'none','#354b56',2)
        if side=='Rear':
            shape(shadow(watch.Battery.Shape),side,'none','#202f38',2,'10 6')
        x,y=xy(-.455,9.99,side)
        rect(x-34,y-13,68,23,BACKGROUND,3)
        text(x,y+5,'穿线窗',17,MUTED,weight=700,anchor='middle')
        for h,geom in sorted(bands.items()):
            polys=[geom] if geom.geom_type=='Polygon' else list(geom.geoms)
            largest=max(polys,key=lambda p:p.area)
            interior=polylabel(largest,tolerance=.05)
            x,y=xy(interior.x,interior.y,side)
            if interior.distance(largest.boundary)>1.20:
                text(x,y+5,f'{h:g}',16,'white' if h==6 else '#23343b',700,'middle')
        if side=='Front':
            x,y=xy(-3,-6.5,side)
            text(x,y+4,'贴屏宽片',18,'white',700,'middle',extra='style="paint-order:stroke;stroke:#455660;stroke-width:4"')
        else:
            x,y=xy(-.65,-8.3,side)
            text(x,y,'思澈模组',21,'white',700,'middle')
            x,y=xy(2.8,4.1,side)
            text(x,y+5,'折返屏尾',16,'white',700,'middle')
            x,y=xy(10.55,5.135,side)
            text(x,y+3,'CN1',14,'white',700,'middle',extra=f'transform="rotate(-90 {x} {y})"')
            x,y=xy(-14.2,-8.3,side)
            text(x,y,'天线',17,'#344650',700,'middle')
        entries=[h for h in sorted(bands)]
        text(left+5,849,'颜色所示最高试算档',20,MUTED)
        step=127 if side=='Rear' else 170
        for i,h in enumerate(entries):
            x=left+6+i*step
            rect(x,873,27,27,COLORS[h],5)
            text(x+37,895,f'{h:g} mm',22,weight=700)
        if side=='Front':
            text(left+5,942,f"0.7 mm 档可摆 {cumulative[.7]:.2f} mm²，0.8 mm 档为 {cumulative[.8]:g}。",20,MUTED)
        else:
            text(left+5,942,'虚线圈出电芯投影，其中的彩色区域仍可布置器件。',20,MUTED)

    line(42,971,1478,971,'#b5c5c8',1)
    for x,fill,label in [(47,BOARD,'实板其余预留'),(337,'url(#fpc)','屏背 FPC 预留'),(656,'url(#pending)','高度待核'),(936,'url(#rf)','射频预留'),(1200,BACKGROUND,'板外或孔槽')]:
        rect(x,998,29,24,fill,3)
        text(x+39,1019,label,20)
    text(48,1073,'填色按已核算的高度档划分。屏背贴片按 0.12 mm 名义厚度，斜纹待核的 133 mm² 暂不计入可摆面积。',20,MUTED)
    parts.append('</svg>')
    svg=root/'B-placement-review.svg'
    svg.write_text('\n'.join(parts),encoding='utf-8')
    browser=which('chrome') or which('msedge') or next(
        p for p in (
            Path(environ.get('ProgramW6432', environ['ProgramFiles']))/'Google/Chrome/Application/chrome.exe',
            Path(environ.get('ProgramFiles(x86)', environ['ProgramFiles']))/'Microsoft/Edge/Application/msedge.exe',
        ) if p.is_file()
    )
    png=(root/'B-placement-review.png').resolve()
    profile=root/'height-figure-browser'
    subprocess.run([str(browser),'--headless','--disable-gpu','--hide-scrollbars','--no-first-run',
                    '--disable-extensions',f'--user-data-dir={profile}',f'--window-size={WIDTH},{HEIGHT+140}',
                    '--force-device-scale-factor=1.5',f'--screenshot={png}',svg.resolve().as_uri()],
                   check=True,timeout=45,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    with Image.open(png) as im:
        page=im.crop((0,0,int(WIDTH*1.5),int(HEIGHT*1.5)))
    page.save(png)
