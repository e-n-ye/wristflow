"""Place reference footprints in the B mechanical envelope; FreeCAD 1.1.4 Python."""
from os import environ
import argparse
import json
import math
import xml.etree.ElementTree as ET
from pathlib import Path
from zipfile import ZipFile

parser = argparse.ArgumentParser()
parser.add_argument("model", type=Path)
parser.add_argument("reference_epro", type=Path)
parser.add_argument("--out", required=True, type=Path, help="New local output directory")
args = parser.parse_args()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=False)

import FreeCAD as App
import Part
from PIL import Image, ImageDraw, ImageFont
with ZipFile(args.model) as z:
    xml = ET.fromstring(z.read("Document.xml"))
    shapes = {}
    for obj in xml.findall("./ObjectData/Object"):
        name = obj.get("name")
        entry = obj.find('./Properties/Property[@name="Shape"]/Part')
        if entry is None or name.startswith(("View", "Cut", "Redmi")):
            continue
        shape = Part.Shape()
        shape.importBrepFromString(z.read(entry.get("file")).decode())
        shapes[name] = shape

pcb = shapes["MainPCB"]
z0 = pcb.BoundBox.ZMin
# X,Y use the existing FreeCAD coordinates. Envelope height includes a design allowance.
zones = [
    ("P", "AW32001ECSR", -10, -14.2, 6, 6, 1.2, "d44799f8b00747f3924a07b59ac69248"),
    ("I", "LSM6DS3TR-C", -3.0, -14.0, 5, 5, 1.2, "e379d1c5f48a457bb446872fec533cbd"),
    ("A", "NS4150B", 9.8, 14, 7, 7.5, 1.4, "96339976ee384ef992d3dbeb4e83c5eb"),
    ("R", "升降压候选区", 6, -14, 8, 6, 2.0, None),
]
canvas = Image.new("RGB", (1320, 845), "white")
d = ImageDraw.Draw(canvas)
ink = "#193a40"
muted = "#4d646a"
def write(x, y, s, size=23, bold=False, color=ink):
    path = (str(Path(environ['SystemRoot']) / 'Fonts') + '/msyh') + ("bd" if bold else "") + ".ttc"
    d.text((x,y), s, fill=color, font=ImageFont.truetype(path, size))
def xy(x, y):
    return (354-14*x, 456-14*y)
def rect(x, y, w, h):
    return xy(x+w/2,y+h/2) + xy(x-w/2,y-h/2)
write(40,24,"B 版主板  /  首轮器件预布局",40,True)
write(42,90,"从后盖看向主板    ·    辅助器件按功能分区，固定件沿用装配模型",23,False,muted)
d.rounded_rectangle(rect(0,0,36.8,43.2),radius=64,fill="#eef4f2",outline=ink,width=3)
d.rectangle(rect(-7,18.075,18.3,7.05),fill="white",outline="#95a5a8")
for k, label in [("ModuleSubstrate","模组"),("Motor","电机"),("Battery","电池投影")]:
    bb=shapes[k].BoundBox
    x,y=bb.Center.x,bb.Center.y
    bounds=rect(x,y,bb.XLength,bb.YLength)
    if k=="Motor":
        d.ellipse(bounds,outline="#9d899d",width=3)
    else:
        d.rounded_rectangle(bounds,radius=8,outline="#a2aaa5",width=3)
    lx,ly=xy(x,y)
    write(lx-24,ly-14,label,15,False,muted)
peers={}
results=[]
with ZipFile(args.reference_epro) as lib:
    for key,name,x,y,w,h,t,fp in zones:
        probe=Part.makeBox(w,h,t,App.Vector(x-w/2,y-h/2,z0-t))
        foot=Part.makeBox(w,h,.04,App.Vector(x-w/2,y-h/2,z0))
        assert abs(foot.common(pcb).Volume-w*h*.04)<1e-5,key+" board edge"
        for k,s in {**shapes,**peers}.items():
            if k!="MainPCB" and probe.BoundBox.intersect(s.BoundBox):
                assert probe.common(s).Volume<1e-5,(key,k)
        peers[key]=probe
        d.rounded_rectangle(rect(x,y,w,h),radius=4,fill="#cfe8df" if fp else "#f5e1b8",outline="#24745b" if fp else "#a76a23",width=3)
        pads=[]
        if fp:
            for raw in lib.read("FOOTPRINT/"+fp+".efoo").decode().splitlines():
                r=json.loads(raw)
                if not r or r[0]!="PAD":continue
                px,py=[v*.0254 for v in r[6:8]]
                # Rear-view X reflection is applied by xy().
                pw,ph=[v*.0254 for v in r[10][1:3]]
                theta=math.radians(r[8]); co,si=abs(math.cos(theta)),abs(math.sin(theta))
                dx,dy=pw*co+ph*si,pw*si+ph*co
                assert abs(px)+dx/2<w/2 and abs(py)+dy/2<h/2,(key,r[5])
                d.rectangle(rect(x+px,y+py,dx,dy),fill="#426c63")
                pads.append(r[5])
        else:
            cx,cy=xy(x,y);write(cx-10,cy-17,"R",24,True)
        xa,ya=xy(x+w/2,y+h/2)
        if fp:write(xa+3,ya+1,key,16,True)
        results.append(dict(part=name,center_mm=[x,y],allocated_mm=[w,h,t],reference_pad_count=len(pads),clash=False))
write(700,160,"朝电池一面的四块位置",29,True)
for n,(tag,line,sub) in enumerate([
    ("P","充电与电芯温度输入","6 × 6 × 1.2 mm   /   靠电池接头"),
    ("I","六轴传感器","5 × 5 × 1.2 mm   /   避开电机投影"),
    ("A","喇叭功放","7 × 7.5 × 1.4 mm   /   靠喇叭"),
    ("R","可选升降压电源","8 × 6 × 2.0 mm   /   芯片、电感和电容待选"),
]):
    yy=228+n*103
    write(700,yy,tag,28,True,"#2b7059" if tag!="R" else "#9b651c")
    write(754,yy,line,25,True);write(754,yy+43,sub,18,False,muted)
write(700,661,"屏座位置仍以 W-010 排线、扣合高度为准。",20,False,muted)
write(40,785,"几何试排    ·    原板封装只作候选    ·    未布线，热设计、天线与整机低压均待验证",20,False,muted)
canvas.save(out/"B-prelayout.png")
(out/"positions.json").write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding="utf-8")
print(results)
