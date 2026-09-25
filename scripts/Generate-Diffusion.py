"""Whole-hour white/gray artwork. Bitmap generation only; C is exported by LVGL Editor."""
from pathlib import Path
import math
import random
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "ui/xml/images"
SCALE = 4
SIZE = (342, 282)


def contour(start, commands):
    points = [start]
    current = start
    for command in commands:
        if len(command) == 2:
            current = command
            points.append(current)
            continue
        a, b, end = command
        origin = current
        for step in range(1, 33):
            t = step / 32
            points.append(tuple((1-t)**3 * origin[i] + 3*(1-t)**2*t*a[i]
                                + 3*(1-t)*t*t*b[i] + t**3*end[i] for i in (0, 1)))
        current = end
    return [(round(x*SCALE), round(y*SCALE)) for x, y in points]


def silhouette(digit):
    mask = Image.new("L", (SIZE[0]*SCALE, SIZE[1]*SCALE))
    draw = ImageDraw.Draw(mask)
    if digit == 9:
        return silhouette(6).transpose(Image.Transpose.ROTATE_180)
    if digit == 7:
        outer = contour((30, 11), [(312, 11), (312, 53),
            ((249, 121), (204, 198), (178, 272)), (119, 272),
            ((145, 190), (187, 119), (247, 55)), (30, 55)])
    elif digit == 6:
        outer = contour((283, 40), [((257, 17), (226, 8), (184, 8)),
            ((90, 8), (39, 65), (39, 154)), ((39, 232), (82, 274), (174, 274)),
            ((260, 274), (303, 233), (303, 174)), ((303, 117), (262, 88), (202, 88)),
            ((156, 88), (124, 100), (102, 119)), ((109, 73), (133, 49), (181, 49)),
            ((212, 49), (238, 59), (254, 75)), (283, 40)])
    elif digit == 1:
        outer = contour((105, 54), [(158, 10), (212, 10), (212, 273),
            (156, 273), (156, 72), (105, 110)])
    elif digit == 0:
        outer = contour((174, 8), [((91, 8), (48, 55), (48, 141)),
            ((48, 228), (91, 274), (174, 274)), ((257, 274), (300, 228), (300, 141)),
            ((300, 55), (257, 8), (174, 8))])
    elif digit == 2:
        outer = contour((46, 71), [((65, 26), (111, 8), (175, 8)),
            ((252, 8), (296, 41), (296, 91)), ((296, 132), (269, 156), (222, 180)),
            (128, 228), (298, 228), (298, 273), (46, 273), (46, 230),
            (193, 150), ((223, 134), (239, 119), (239, 96)),
            ((239, 68), (215, 51), (174, 51)), ((133, 51), (109, 67), (96, 90)), (46, 71)])
    elif digit == 3:
        outer = contour((48, 46), [((86, 19), (126, 8), (176, 8)),
            ((251, 8), (293, 37), (293, 80)), ((293, 106), (278, 124), (255, 136)),
            ((286, 147), (302, 168), (302, 199)), ((302, 247), (252, 274), (177, 274)),
            ((121, 274), (73, 260), (41, 229)), (77, 194),
            ((104, 218), (134, 230), (177, 230)), ((220, 230), (244, 219), (244, 195)),
            ((244, 171), (221, 159), (174, 159)), (128, 159), (128, 115), (174, 115),
            ((215, 115), (237, 105), (237, 83)), ((237, 62), (216, 51), (177, 51)),
            ((139, 51), (105, 62), (80, 80)), (48, 46)])
    elif digit == 4:
        outer = contour((207, 10), [(267, 10), (267, 176), (311, 176),
            (311, 222), (267, 222), (267, 273), (211, 273), (211, 222),
            (30, 222), (30, 179), (207, 10)])
    elif digit == 5:
        outer = contour((65, 11), [(291, 11), (291, 55), (117, 55), (110, 113),
            ((134, 103), (158, 99), (188, 99)), ((262, 99), (304, 130), (304, 185)),
            ((304, 241), (254, 274), (175, 274)), ((119, 274), (74, 260), (41, 232)),
            (75, 194), ((102, 218), (136, 230), (174, 230)),
            ((223, 230), (246, 214), (246, 186)), ((246, 158), (223, 141), (180, 141)),
            ((149, 141), (127, 147), (104, 161)), (51, 151), (65, 11)])
    elif digit == 8:
        outer = contour((174, 8), [((100, 8), (49, 37), (49, 80)),
            ((49, 107), (64, 124), (86, 136)), ((58, 148), (39, 170), (39, 201)),
            ((39, 249), (93, 274), (174, 274)), ((253, 274), (308, 249), (308, 201)),
            ((308, 170), (289, 148), (261, 136)), ((285, 124), (299, 107), (299, 80)),
            ((299, 37), (248, 8), (174, 8))])
    else:
        raise ValueError(digit)
    draw.polygon(outer, fill=255)
    if digit == 6:
        hole = contour((101, 178), [((101, 147), (126, 132), (173, 132)),
            ((222, 132), (245, 148), (245, 179)), ((245, 212), (222, 232), (174, 232)),
            ((128, 232), (101, 212), (101, 178))])
        draw.polygon(hole, fill=0)
    elif digit == 0:
        hole = contour((174, 52), [((127, 52), (107, 88), (107, 141)),
            ((107, 195), (127, 230), (174, 230)), ((222, 230), (242, 195), (242, 141)),
            ((242, 88), (222, 52), (174, 52))])
        draw.polygon(hole, fill=0)
    elif digit == 4:
        draw.polygon(contour((211, 77), [(211, 176), (107, 176)]), fill=0)
    elif digit == 8:
        for bounds in ((105, 50, 243, 119), (97, 157, 251, 232)):
            draw.ellipse(tuple(v*SCALE for v in bounds), fill=0)
    return mask


def material(index):
    if index == 2:
        return Image.new("L", (SIZE[0]*SCALE, SIZE[1]*SCALE), 249)
    base = 135 if index in (0, 4) else 185
    rng = random.Random(20260924 + index)
    pixels = []
    for y in range(SIZE[1]):
        for x in range(SIZE[0]):
            haze = 5 * math.sin(x/59 + math.cos(y/74)) + 4 * math.cos(y/91 + x/153)
            pixels.append(round(base + haze + rng.uniform(-2, 2)))
    surface = Image.new("L", SIZE)
    surface.putdata(pixels)
    return surface.resize((SIZE[0]*SCALE, SIZE[1]*SCALE), Image.Resampling.BICUBIC)


def hour_mask(hour):
    if hour < 10:
        return silhouette(hour)
    pieces = [silhouette(int(d)).crop(silhouette(int(d)).getbbox()) for d in str(hour)]
    gap = 18 * SCALE
    pair = Image.new("L", (sum(p.width for p in pieces)+gap, max(p.height for p in pieces)))
    x = 0
    for piece in pieces:
        pair.paste(piece, (x, (pair.height-piece.height)//2))
        x += piece.width+gap
    ratio = min(1, 310*SCALE/pair.width, 266*SCALE/pair.height)
    pair = pair.resize((round(pair.width*ratio), round(pair.height*ratio)), Image.Resampling.LANCZOS)
    result = Image.new("L", (SIZE[0]*SCALE, SIZE[1]*SCALE))
    result.paste(pair, ((result.width-pair.width)//2, (result.height-pair.height)//2))
    return result


def hour_art(hour):
    mask = hour_mask(hour)
    left, _, right, _ = mask.getbbox()
    width = right-left
    side = .18 if hour >= 10 else .27
    # Whole-hour coverage; each outer pane is half the width of the inner pane.
    cuts = [left+round(width*f) for f in (0, side/3, side, 1-side, 1-side/3, 1)]
    result = ImageChops.multiply(mask, material(2))
    for index in (0, 1, 3, 4):
        a, b = cuts[index:index+2]
        clip = Image.new("L", mask.size)
        ImageDraw.Draw(clip).rectangle((a, 0, b-1, mask.height), fill=255)
        clip = clip.filter(ImageFilter.GaussianBlur(.24*SCALE))
        blur = (1.8 if index in (0, 4) else .8) * SCALE
        glass = ImageChops.multiply(mask.filter(ImageFilter.GaussianBlur(blur)), material(index))
        result = Image.composite(glass, result, clip)
    return result.resize(SIZE, Image.Resampling.LANCZOS).convert("RGB")


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGB", (342*4, 282*3))
    for hour in range(1, 13):
        art = hour_art(hour)
        art.save(OUT / f"hour_{hour}.png")
        sheet.paste(art, (((hour-1)%4)*342, ((hour-1)//4)*282))
    proof = ROOT / "artifacts/hour-resource-sheet.png"
    proof.parent.mkdir(exist_ok=True)
    sheet.save(proof)
    print("Generated grayscale hours 1-12; sheet is a bitmap asset proof, not an LVGL screenshot")
