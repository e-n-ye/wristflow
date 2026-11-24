# B 方案结构研究

这里维护 2026-11-21 和 2026-11-22 的参数化 FreeCAD 研究。板卡源与 19.20 mm 比较档的评审见 [B-R2 硬件设计](../../hardware/README.md)。这些几何用于研究尺寸、净空和装拆顺序。B-R2 尚无实物，不能据此放行加工、打印或试戴。

| 日期与子研究 | 用途与入口 |
|---|---|
| 2026-11-21 [source](2026-11-21/source) | A/B 参数外形、基础器件包络和重开核验，入口 `target_model.py`，也可用本目录旧启动器 `build.py` |
| 2026-11-21 [B-service-section](2026-11-21/B-service-section) | 拆盖维修剖面、线束、工具空间与 PCB 支撑，入口 `service.py`，`geometry.json` 保存原版结果 |
| 2026-11-21 [B-prelayout](2026-11-21/B-prelayout) | 以本机黄山派封装校核 B 的器件包络，入口 `check.py <模型> <参考工程> --out <新目录>` |
| 2026-11-21 [B-board-space](2026-11-21/B-board-space) | 双面板材面积、高度分层和禁布区，入口 `layout_space.py` |
| 2026-11-21 [B-display-route](2026-11-21/B-display-route) | 屏尾穿槽、翻折和扣合，入口 `route.py`，几何基准见 `config.json` |
| 2026-11-21 [B-integrated](2026-11-21/B-integrated) | 合并维修、走屏尾及双面限高，入口 `deliver.py`，保留前一版限高数值和本版回读 |
| 2026-11-22 [B-slot](2026-11-22/B-slot) | 屏尾窗口和折端变化，入口 `slot_study.py` |
| 2026-11-22 [B-bridge](2026-11-22/B-bridge) | 电机孔与槽之间的窄桥、声道与协同初排，入口 `compare.py`、`coordinated.py` |
| 2026-11-22 [B-crown](2026-11-22/B-crown) | 居中表冠、排线和立板连接、末端支撑及 R2 减薄，顺序见下方 |

## 本机工作目录

使用 FreeCAD 1.1.4 及其 Python 环境。求投影的脚本需要 NumPy 和 Shapely，绘图另需 Pillow 与 Windows 微软雅黑。舱体和 PCB 生成共用 `WRISTFLOW_B_WORKSPACE`，默认 `artifacts/wristflow-b`。屏尾走向用到本机 `reference/official-v12.epro`。[参考来源与版本哈希](../../hardware/reference/README.md)

在仓库根目录中运行以下 Git Bash 命令。先准备合法本机参考文件。每次建模使用新的目标目录，前一步产物按下表路径在同一工作目录内供下一步读取。

```bash
export WRISTFLOW_B_WORKSPACE="$(cygpath -am artifacts/wristflow-b)"
freecad='<本机 FreeCAD.exe>'
python -B mechanical/b-enclosure/run.py --freecad "$freecad" \
  --script 2026-11-21/source/target_model.py --out "$WRISTFLOW_B_WORKSPACE/2026-11-21"
```

按下表将 `--script` 和 `--out` 换为相应路径。末列相对 `WRISTFLOW_B_WORKSPACE`。

| 脚本 | 输出目录 |
|---|---|
| `2026-11-21/B-service-section/service.py` | `2026-11-21/B-service-section` |
| `2026-11-21/B-display-route/route.py` | `2026-11-21/B-display-route` |
| `2026-11-21/B-integrated/deliver.py` | `2026-11-21/B-integrated` |
| `2026-11-22/B-slot/slot_study.py` | `2026-11-22/B-slot/fold-end-fixed` |
| `2026-11-22/B-bridge/coordinated.py` | `2026-11-22/B-bridge/mirror-port-3` |
| `2026-11-22/B-crown/crown_fit.py` | `2026-11-22/B-crown/BMQ22-B` |
| `2026-11-22/B-crown/center_fit.py` | `2026-11-22/B-crown/centered` |
| `2026-11-22/B-crown/shared_edge.py` | `2026-11-22/B-crown/B-layout-review` |
| `2026-11-22/B-crown/routing_review.py` | `2026-11-22/B-crown/B-connections` |
| `2026-11-22/B-crown/one_rear_flex.py` | `2026-11-22/B-crown/B-rear-FPC` |
| `2026-11-22/B-crown/lower_support.py` | `2026-11-22/B-crown/B-lower-support` |
| `2026-11-22/B-crown/review_revision.py` | `2026-11-22/B-crown/B-review-R2` |

每个构建输出 `builder.ok` 和自己的几何回读。失败目录保留，查看 `error.txt` 和 `freecad.log`。原版 `geometry.json` 保存评审时的尺寸、验证结果和部分原文件哈希，后续步骤读取的是本机新生成的对应报告。

2026-11-24 使用 FreeCAD 1.1.4 在新目录跑通基础外形和表中 12 步，生成最终 R2 模型及板框 STEP。此结果覆盖建模和现有脚本内的几何断言，实物与制造公差待核。

图表脚本读取本机阶段产物，通常用首个位置参数指定输出目录，其中 `diagram.py` 接收目标 PNG 文件名。模型、STEP、图像、临时宏和第三方参考留本机。社区模型及派生物遵循 [A0 的分发边界](../wearable/README.md)。
