# WristFlow B-R2 硬件设计

2026-11-22 评审版含四块板的电路和初排。主板尚未完成布线，四块板均无实物或上板验证。[设计评审](reviews/2026-11-22-b-r2/设计评审.md)保存同版 BOM、参考位号差异、净空和回读。唯一下一步见 [STATUS](../docs/STATUS.md#current)。

**母版工具过渡期（2026-11-25 起）**　原理图母版改在 KiCad，由 [`kicad_schematic.py`](generator/kicad_schematic.py) 按[版式源](generator/schematic_layout.py)生成，网表与评审版逐引脚一致。PCB 初排仍在 EPRO（`placement.ndjson`、`members.sha256` 未改），正式迁到 KiCad 是另一项变更。EPRO 内的旧原理图页仍按评审版恢复，不再维护其排版。[重绘记录、等价证据与画板风险](reviews/2026-11-25-kicad-schematic/README.md) · [用户决定](../docs/BASELINE.md#kicad-master-20261125)

| 板卡 | 评审范围 |
|---|---|
| [B-R2 主板](boards/b-r2-main/README.md) | 模组、充电、电源、屏触、音频、传感器、表冠和服务接口 |
| [后盖柔板](boards/b-rear-fpc/README.md) | MAX30102 光学岛、10P 尾指、喇叭和充电端子 |
| [表冠立板](boards/b-crown/README.md) | BMQ22 编码器、定位孔及直角焊端 |
| [表冠水平触点板](boards/b-crown-contacts/README.md) | 四个压合金面及立板焊端 |

## 设计源与发布范围

- [电路](generator/circuit.py)、[项目库组合](generator/library.py)和[原理图及 PCB 生成器](generator/write_project.py)维护电路和生成规则。每块板的 `placement.ndjson` 固定评审版板框、网络、器件位置和焊盘到网络的关联。相邻的 `members.sha256` 固定完整 EPRO 各成员的内容哈希。
- [结构研究](../mechanical/b-enclosure/README.md)维护 FreeCAD 建模脚本和参数。候选布局可由 [机械投影](generator/mechanical.py)、[退板扫掠](generator/sweep.py)和[初排](generator/place.py)在本机重新计算。
- 官方黄山派模板和库、系统库、数据手册及来源核对见 [外部资料](reference/README.md)。完整 EPRO、原恢复 ZIP、KiCad QA 板、结构投影以及渲染了库符号或焊盘的原理图和 1:1 PDF 留在本机。两份结构投影还包含从官方屏尾轮廓推导的避让几何。社区网格、可能含该网格的模型和图像、日志及各阶段生成物留在忽略目录。

## 本机复现

使用 Python 3.12 或更新版本及 Shapely 2.1.2。嘉立创 EDA 专业版 **4.1.105** 提供已核对的本地系统库，KiCad CLI **10.0.8** 提供已验证的 PCB 导入和独立求网。评审版恢复只依赖这两份[合法本机输入](reference/README.md)与仓库源码，无需运行 FreeCAD 或登录 EDA。输出目录须使用新路径。

```bash
python -m pip install shapely==2.1.2
python -B hardware/generator/restore_projects.py \
  '<本机黄山派 V1.2 .epro>' '<本机 lceda-std.elib>' artifacts/wristflow-b/eda
python -B hardware/generator/verify.py \
  --projects artifacts/wristflow-b/eda --kicad-cli '<本机 kicad-cli 可执行文件>'
```

`restore_projects.py`逐一核对四个工程全部成员的 SHA256，成功后交付到新目录。ZIP 容器时间戳可变。可将生成的 EPRO 导入嘉立创 EDA 的本地项目。KiCad 复核产生的 `native-qa/` 和电气回读位于项目输出目录，已知 ERC、DRC 和待连项保留在评审记录。

高级复现由 `WRISTFLOW_B_WORKSPACE` 选择本机工作目录，默认 `artifacts/wristflow-b`。把合法本机输入以 `reference/official-v12.epro` 和 `reference/lceda-std.elib` 提供给该工作目录。以下脚本生成的工程、图纸和 CAD 文件仅供本机使用。

- `write_project.py --reviewed-placement` 使用已保存的初排。省略此参数时，从 `eda-inputs/constraints.json` 和 `eda-inputs/sweep.json` 重新求初排。这两个输入由 FreeCAD Python 运行 `mechanical.py` 和 `sweep.py` 生成，并读取工作目录下 `2026-11-22/B-crown/B-review-R2` 的模型和板框 STEP。
- `schematic_view.py` 在 `eda/` 的本机 EPRO 上生成 `schematics.pdf`。`report.py` 使用机械投影和本机库重算布局，生成 BOM、空区、`both-sides.png` 和 `layout-1to1.pdf`。两者需要 Matplotlib 与 Windows 微软雅黑。FreeCAD Python 内可运行 `fit.py` 生成装配回读与本机 STEP。
- `publish_design.py` 沿用历史归档方式生成本机 `restore-kit/`，其中 `design.zip` 仍包含第三方模板。只有处理旧本机恢复包时才使用 `restore_projects.py --design <本机 design.zip>`。

## KiCad 原理图

在已恢复的四份评审版 EPRO 上生成 KiCad 10 原生原理图。除恢复所需依赖外，需要本机 KiCad 自带的 `share/kicad/symbols` 标准库和 kicad-cli。如果装了 PyMuPDF，脚本还会对 PDF 字体做子集化、合并四板 PDF，并可选输出 PNG 预览。

```bash
python -m pip install pymupdf
python -B hardware/generator/verify.py \
  --projects artifacts/wristflow-b/eda --kicad-cli '<本机 kicad-cli 可执行文件>'
python -B hardware/generator/kicad_schematic.py \
  --projects artifacts/wristflow-b/eda --out artifacts/wristflow-b/kicad \
  --kicad-symbols '<KiCad 安装目录>/share/kicad/symbols' \
  --kicad-cli '<本机 kicad-cli 可执行文件>' --png
```

每块板输出一个 KiCad 工程目录，内含原理图、项目库 `WF.kicad_sym`、`sym-lib-table`、网表、ERC 和 PDF。总目录下另有四板合订的 `WristFlow-B-R2-schematics.pdf` 和 `equivalence.json`。脚本逐引脚比对 KiCad 网表与 EPRO 原理图引脚、PCB `PAD_NET`，并检查网络划分和器件属性。存在 `verify.py` 的 `native-qa/*/compiled.xml` 时，还会与这份独立求网逐脚比对。任何差异都以非零状态退出。输出目录须为新路径，`--force` 会替换已有目录。这些输出带有标准库符号和 EPRO 引脚名，留在本机，不入库。中文使用 Windows 自带的等线（DengXian）。

2026-11-25 用 Python 3.12.9、Shapely 2.1.2、PyMuPDF 2.1 和 KiCad CLI 10.0.8 复现：四板差异为零，同一输入两次生成的 22 个 KiCad 文件逐字节相同。

## 2026-11-24 复现记录

2026-11-24 用 Python 3.13.15 和 Shapely 2.1.2 在新目录恢复四份工程，137 个成员与评审版全数一致。KiCad CLI 10.0.8 复查四块板，原理图与 PCB 的网络一致，独立求网差异为零。主板仍有 313 个待连项，以及评审列出的间距与 ERC 告警。固件编译和硬件验证未在本轮执行。
