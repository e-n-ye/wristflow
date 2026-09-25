"""Rebuild font subsets and diffusion bitmaps; no XML conversion."""
from pathlib import Path
import argparse
import runpy

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--fonts-only", action="store_true", help="Preserve existing watchface bitmaps")
parser.add_argument("--lvgl-root", type=Path, default=ROOT / "vendor/SiFli-SDK/external/lvgl_v9",
                    help="Locked SDK LVGL directory (for isolated worktrees)")
args = parser.parse_args()
LVGL = args.lvgl_root
OUTPUT = ROOT / "ui/xml"
TEXT = (
    "月日周二步心率次分最近测量今日活动距离目标千米系统状态电池蓝牙已连接未充电"
    "运行时间控制中心勿扰手筒常亮设置小时钟关闭开启分钟数完成度选择组件新增页面"
    "秒表应用简洁扩散返回暂停继续开始重置已选显示盘即将推出"
    "血氧压力活力指标计步站立天气睡眠闹钟供未接入传感器功能等待手机同步暂无数据，"
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
            [0xf004, 0xf012, 0xf013, 0xf017, 0xf053, 0xf054, 0xf0eb, 0xf185, 0xf186,
             0xf240, 0xf293, 0xf54b, 0xf04b, 0xf04c, 0xf2f1, 0xf00c, 0xf390, 0xf2f2,
             0xf043, 0xf118, 0xf70c, 0xf183, 0xf0c2, 0xf236, 0xf0f3, 0xf201])

if not args.fonts_only:
    runpy.run_path(str(ROOT / "scripts/Generate-Diffusion.py"), run_name="__main__")
