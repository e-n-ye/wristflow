"""Plot PCB geometry read back from KiCad and the corresponding checked 3D solids."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Polygon, Rectangle


def render(directory, g, fit):
    plt.rcParams["font.family"] = "Microsoft YaHei"
    board_color, copper, ink = "#18635f", "#f0cb78", "#143638"
    highlight = {"U4", "U15", "U21", "D19", "U3", "J201", "U1"}
    for side, title in [("F", "屏侧"), ("B", "后盖侧")]:
        fig, ax = plt.subplots(figsize=(9, 10), facecolor="#f6f5ed")
        ax.set_aspect("equal")
        ax.set_facecolor("#f6f5ed")
        ax.add_patch(Polygon(g["outline"], color=board_color))
        for hole in g["holes"]:
            ax.add_patch(Polygon(hole, color="#f6f5ed"))
        if side == "B":
            ax.add_patch(Rectangle((-17, -20.4), 32, 30.5, fill=False, ls="--", lw=1, color="#9adbd2"))
        for r, (face, x0, y0, x1, y1, h) in fit["envelopes"].items():
            if face != side:
                continue
            ax.add_patch(
                Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor="#22504f", edgecolor="#a6cebb", lw=0.55)
            )
        for t in g["tracks"]:
            if t["side"] == side:
                ax.plot(*zip(t["start"], t["end"]), color="#ea9968", lw=max(0.75, 7 * t["width"]))
            elif side == "B" and t["side"] == "L3":
                ax.plot(*zip(t["start"], t["end"]), color="#81d4e0", lw=0.9, ls="--")
        for f in g["footprints"].values():
            if f["side"] != side:
                continue
            for pad in f["pads"]:
                for poly in pad["polygons"]:
                    ax.add_patch(Polygon(poly, color=copper, lw=0))
        for v in g["vias"]:
            ax.add_patch(Circle(v["xy"], v["diameter"] / 2, facecolor=copper, lw=0))
            ax.add_patch(Circle(v["xy"], v["drill"] / 2, facecolor=ink, lw=0))
        for r, (face, x0, y0, x1, y1, h) in fit["envelopes"].items():
            if face != side:
                continue
            ax.text(
                (x0 + x1) / 2,
                (y0 + y1) / 2,
                r,
                ha="center",
                va="center",
                fontsize=9 if r in highlight else 6.3,
                weight="bold" if r in highlight else "normal",
                color="white",
                bbox={"facecolor": "#283b3c", "alpha": 0.82, "edgecolor": "none", "pad": 0.55},
            )
        ax.set_xlim((-20, 20) if side == "F" else (20, -20))
        ax.set_ylim(-23.6, 23.1)
        ax.set_xlabel("装配 X（mm）")
        ax.set_ylabel("装配 Y（mm）")
        ax.set_title(f"WristFlow B-R2　/　{title}", loc="left", fontsize=19, color=ink, pad=14)
        ax.spines[:].set_visible(False)
        fig.text(
            0.11,
            0.025,
            "金色为焊盘与通孔，橙色为试布走线。"
            + ("蓝色虚线为 L3 扇出，矩形虚线为 0.96 mm 限高区。" if side == "B" else "顶部朝向 J1 表带端。"),
            color=ink,
            fontsize=10,
        )
        fig.savefig(
            directory / ("front.png" if side == "F" else "back.png"),
            dpi=190,
            bbox_inches="tight",
            pad_inches=0.28,
        )
        plt.close(fig)
