"""Rebuild demo font subsets and static faceted hour bitmaps; no XML conversion."""
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
LVGL = ROOT / "vendor/SiFli-SDK/external/lvgl_v9"
OUTPUT = ROOT / "ui/xml"
TEXT = (
    "月日周二步心率次分最近测量今日活动距离目标千米系统状态电池蓝牙已连接未充电"
    "运行时间控制中心勿扰手筒常亮设置小时钟关闭开启分钟数完成度选择组件新增页面"
)


def subset_font(source, target, family, codepoints):
    font = TTFont(source)
    font.flavor = None
    options = subset.Options()
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
    worker = subset.Subsetter(options=options)
    worker.populate(unicodes=codepoints)
    worker.subset(font)
    names = {1: family, 2: "Regular", 3: family + " Regular demo subset",
             4: family + " Regular", 6: family.replace(" ", "") + "-Regular"}
    for record in font["name"].names:
        if record.nameID in names:
            record.string = names[record.nameID].encode(record.getEncoding())
    font.save(target)
    print(f"{target.relative_to(ROOT)}: {target.stat().st_size} bytes")


subset_font(LVGL / "tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf",
            OUTPUT / "fonts/WristFlowSans-Regular.ttf", "WristFlow Sans",
            set(range(32, 127)) | {ord(c) for c in TEXT})
subset_font(LVGL / "scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff",
            OUTPUT / "fonts/WristFlowIcons.ttf", "WristFlow Icons",
            [0xf004, 0xf012, 0xf013, 0xf017, 0xf053, 0xf0eb, 0xf185, 0xf186,
             0xf240, 0xf293, 0xf54b])

# Static masks are intentionally independent of the deferred breathing animation.
scale = 4
font = ImageFont.truetype(str(LVGL / "tests/src/test_files/fonts/Montserrat-Bold.ttf"), 300 * scale)
colors = ["#8cecff", "#52c9fa", "#b8f5ff", "#59cfff", "#95e8ff"]
previews = []
for hour in range(1, 13):
    label = str(hour)
    box = font.getbbox(label)
    mask = Image.new("L", (box[2] - box[0], box[3] - box[1]))
    ImageDraw.Draw(mask).text((-box[0], -box[1]), label, font=font, fill=255)
    ratio = min(326 * scale / mask.width, 266 * scale / mask.height)
    mask = mask.resize((round(mask.width * ratio), round(mask.height * ratio)), Image.Resampling.LANCZOS)
    canvas = Image.new("L", (342 * scale, 282 * scale))
    left = (canvas.width - mask.width) // 2
    canvas.paste(mask, (left, (canvas.height - mask.height) // 2))
    draw = ImageDraw.Draw(canvas)
    art = Image.new("RGB", canvas.size, "black")
    paint = ImageDraw.Draw(art)
    for band, color in enumerate(colors):
        start = left + round(mask.width * band / len(colors))
        end = left + round(mask.width * (band + 1) / len(colors))
        paint.rectangle((start, 0, end, canvas.height), fill=color)
        if band:
            draw.rectangle((start, 0, start + 2 * scale - 1, canvas.height), fill=0)
    art = Image.composite(art, Image.new("RGB", art.size, "black"), canvas)
    art = art.resize((342, 282), Image.Resampling.LANCZOS)
    art.save(OUTPUT / f"images/hour_{hour}.png")
    previews.append(art)

proof = Image.new("RGB", (342 * 4, 282 * 3), "black")
for index, art in enumerate(previews):
    proof.paste(art, ((index % 4) * 342, (index // 4) * 282))
proof_path = ROOT / "artifacts/hour-resource-sheet.png"
proof_path.parent.mkdir(exist_ok=True)
proof.save(proof_path)
print("Generated static hours 1-12; resource sheet is not an LVGL screenshot")
