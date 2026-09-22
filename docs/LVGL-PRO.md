# LVGL Pro 中文使用说明与 XML 路线复核

核验日期：2026-09-22。适用范围：个人 WristFlow 原型，黄山派尚未到货。有限范围的 grill-with-docs 已完成，用户选择 Community 图形编辑器预览/导出 C，CI 编译提交的生成代码。本机真实预览、官方 GUI 导出与固定 SDK 编译已通过，详见 [视觉演示与操作步骤](UI-DEMO.md)。

## 1. 许可证分别管什么

依据为官方价格页、EULA v1.3 和 XML Specification License v1.0。价格是核验当日页面信息。用户自行登录后，账号已有 Community License，无需另填申请表；本轮已在现有编辑器选择并重新打开许可页确认选中状态，未购买许可。

| 名称 | 中文解释 | 对本项目的影响 |
|---|---|---|
| LVGL Open / core | 图形库，单独采用 MIT 许可 | 使用图形库本身不需要购买 Pro；分发时保留适用的版权和许可声明 |
| Community | 免费、非商业社区许可 | 当前个人原型可使用；官方列出全部编辑器功能、无到期日、非商业使用无席位上限 |
| Evaluation | 免费评估许可 | 与 Community 不同，可能有到期日或席位限制；不能据此开展商业开发 |
| CLI | 命令行导出、校验、测试、截图等工具 | Community 和 Evaluation 不包含；不能默认建立免费的官方 CLI 自动生成流水线 |
| Growth / Product / Platform | 商业许可 | Growth 和 Platform 按协议报价；Product 页面列为 20,000 美元/产品、最少 5 年，具体以许可确认为准 |
| Output | 自己的 XML、生成 C、资源等产物 | EULA 7.2 明确归用户所有；合法生成的产物可在许可范围内脱离编辑器维护 |

“开源项目”不自动等于“非商业使用”。EULA 将销售、向付费客户交付、用于营利经营以及间接变现等列入商业范围；本项目当前个人原型没有因此产生购买商业许可的前置要求。

Community 是无到期日许可；Evaluation 到期会停止工具访问，需要续评估或适用的其他许可。商业 Product 许可到期后编辑器访问也会停止，但 EULA 5.1 明确允许继续独立使用、修改此前合法生成的产物。不能把这解释成评估产物自动取得商业使用授权。

本机现有编辑器为 LVGL Pro Editor 2.0.1，实际预览使用 LVGL 9.4.0。`Account > Manage License` 的 Community License 显示 `No Expiry`，已选中；顶部 `NON-COMMERCIAL USE ONLY` 是非商业使用提示。页面曾显示 Offline，但本轮仍完成导出和预览编译，这不能推断离线校验周期或无限离线保证。官网静态页曾列 1.2.1，与实际安装版本不同，不能据页脚决定工程版本。未把账号、令牌或许可页截图提交到仓库。

## 2. 第一次重新使用时的注意事项

1. 从官方入口选择 Windows 编辑器，区分 Community 与有期限的 Evaluation；如实际界面与本页不一致，记录版本和提示原文后再核查。
2. 官方安装说明要求 Windows 具备 Git、Ninja；首次编译预览会下载 EMSDK 4.0.6 等依赖。编辑器能打开不等于预览工具链已准备好，首次运行应记录网络及下载失败。
3. 从含 `project.xml`、`globals.xml` 的 `ui/xml/` 打开工程。先核对目标分辨率 390x450、LVGL 版本和主题，再检查页面，避免只看 XML 编辑区是否报语法错误。
4. 字号由字体资源决定，不能只移动标签或增大标签容器。大号数字、中文字符集、字体许可、资源体积需要分别检查；UTF-8 文件编码不能补足缺失的字形。
5. XML 保持视觉职责：布局、样式、资源、控件标识和预览样例。业务状态、页面导航、电源策略、外设调用留在核心和适配层。演示数据不作为传感器或连接已工作的证据。
6. 官方编辑器与 CLI 都提供 C 导出。`*_gen.c`、`*_gen.h` 会在重新导出时覆盖；不要把手写业务逻辑放进去。提交 XML、必要资源与所采用路线要求的生成代码，记录编辑器版本和生成方式。
7. 导出 C 后设备可以关闭 `LV_USE_XML`，这省去运行时 XML 解析需求，但 LVGL 对象、字体和绘制仍有内存及执行成本。`watch_core` 零堆分配约束不等于整个 UI 零堆分配。
8. Online Share 是另一项能力：公共仓库可免费分享，需 GitHub 登录；官方说明其浏览器查看/编辑不能保存 XML 修改或编译代码，不能代替桌面导出链路。

## 3. 当前项目与编辑器之间的缺口

| 检查项 | 已核实结果 | 尚需证明 |
|---|---|---|
| SDK | v2.5.1，commit `421126d9f476ed8e2a6f0b0ca28a9f241c182e65`，gitlink 与工作树 HEAD 一致 | 不为 UI 预览擅自升级 SDK 或子模块 |
| 内置 LVGL | `vendor/SiFli-SDK/external/lvgl_v9/lv_version.h:9` 为 9.4.0 | 最新 Pro 导出的 C 是否在这一固定版本编译 |
| XML 工程版本 | 已统一为 9.4.0，simple 主题；实际预览日志确认 9.4.0 | 不跟随官方示例擅自升级 SDK |
| XML / Observer | SDK 有 MIT XML loader；当前 Huangshan Bringup 配置关闭 XML、开启 Observer | 可用源码接口不等于工程已接入 UI |
| 构建接入 | 新增独立 `ui_demo`，编译官方 GUI 导出的 C/H、内嵌字体和图片；本机链接通过 | CI 只编译已提交生成代码，不启动 Pro，也不证明 XML 与 C 永远同步 |
| 核心逻辑 | 本仓库尚无 `watch_core` 实现 | 交互文档不是状态机测试通过的证据 |

保持“XML 描述视觉、纯 C 核心拥有产品行为、适配层连接两者”的方向。本机已跑通官方导出链路；`ui_demo` 的手势回调只是演示适配，不是已经实现或验证了 `watch_core`。

## 4. 已提交 XML 的源码审核

审核对象为 main `2f1bfc1` 的四个 XML。以下是历史问题定位，已在本轮视觉演示修复，并在真实编辑器重现旧版错位/缺字后核验新画面。

| 严重度 | 定位 | 证据与影响 |
|---|---|---|
| 高 | `ui/xml/screens/screen_watchface.xml:63`，以及电池、分页点和卡片中的同类写法 | 元素属性先应用，随后子元素 `remove_style_all` 调用 `lv_obj_remove_style_all`，会清除前面设置的局部样式，包括坐标、尺寸、颜色。依据 SDK `src/others/xml/parsers/lv_xml_obj_parser.c:92`、`:236` 和 `src/core/lv_obj_pos.c:59` |
| 高 | `ui/xml/components/card_container.xml:15` | 静态 style 中 `$card_width`、`$card_height` 未走 view 创建阶段的参数替换。样式注册直接调用尺寸转换，不能获得预期的组件参数。依据 SDK `src/others/xml/lv_xml_style.c:158` 与 `lv_xml.c:728` |
| 中 | `ui/xml/screens/screen_watchface.xml:96`，`ui/xml/globals.xml:75` | 大数字没有绑定大号字体；字体声明为空，当前 Bringup 默认 Montserrat 20，中文字体未接入。不能保证预期字号或中文日期显示 |
| 中 | `ui/xml/globals.xml:36` | 分页点 y=430、高 6，未满足本文件声明的底部 20px 安全区；页面注释写 y=425，实际几何参数不一致 |

上表 SDK 相对路径均以 `vendor/SiFli-SDK/external/lvgl_v9/` 为根。修复采用显式几何/样式、view 属性中的组件参数、实际字体资源和 y=423 分页点；五页 [渲染证据](evidence/2026-09-22-ui/overview.png) 已留存。

## 5. 本次有限范围的 grilling

已确认：进入文档流程；保留既有产品需求、硬件路线、SDK 锁定；XML 仅承担视觉职责；本轮不创建通用 UI 编辑器或迁入旧项目架构。用户进一步确认变暗态首次短按 KEY1 仅提亮并消耗该次按键，下一次短按才导航；后续主机测试应覆盖同次松键不导航、下一次短按正常导航。

已有共识能够直接澄清的项目，已同步到 [交互契约](UI-INTERACTION.md)：

- 变暗按当前用户亮度的约 20% 计算，不是固定设置为面板 20%；依据原始共识第 2.1 节。
- 首次触摸防误触涵盖这次接触直到抬起或取消，不能让后续 release/click/gesture 穿透。
- 息屏是显示状态，深睡是另一个需条件和硬件证据支持的状态；AMOLED 没有液晶背光。
- 返回应用列表使用返回手势；短按 KEY1 回主表盘。2 分钟边界采用既有细则中的大于等于。

用户已确认免费图形编辑器路线：Community 预览并导出 C，提交源 XML、资源和生成代码，CI 编译生成代码。代价是需要人工 GUI 导出、检查差异和截图；本轮没有建立免费的官方 CLI 自动生成流水线。该决定和实测步骤集中记录于本页及 UI-DEMO，不额外重复建立一套架构文档。

## 6. 下一项有界实验与结束条件

首个 XML 链路实验按最多 2 小时的建议执行。实际完成表盘、三张信息环页面和控制中心；真实预览、官方 GUI 导出和固定 SDK 本机编译通过。验收仍分层：

1. 修复表盘的结构与字体，在真实 LVGL 渲染器上得到 390x450 画面；检查非空、中文、数字字号、边界和资源加载。
2. 若采用官方导出路线，导出 C 并在固定 SiFli-SDK 下编译；保留编译日志、配置和产物。导出能成功但目标编译失败，仍属于未通过。
3. 通过后扩展心率、活动、系统状态和控制中心演示，继续按 XML 语义、实际渲染、目标编译分层验证。静态演示不要求先实现业务状态机。

超时或出现版本、许可、资源阻断时，保留最小复现和未完成项，再决定后续实验；不通过升级 SDK、手写第二份 HTML 画面或仅增加 XML 文件数来宣称链路通过。设备端触摸、BLE、深睡和功耗仍全部待板验证。

## 7. 是否提炼个人 skill

本轮已跑通真实流程，并复现了样式清除顺序、字体范围、图片格式和临时预览字体加载等问题。用户选择创建本机纯说明型 `lvgl-xml-visual` skill，内容为版本检查、正确 XML 模式、字体/资源接入、GUI 导出、真实渲染检查与生成物维护；不包含业务逻辑或自制 XML 转换器，不默认调用付费 CLI。该 skill 位于用户个人 skills 目录，不发布到此仓库；完整项目经验仍保留在本页和 UI-DEMO。

XML Specification License 允许个人/组织内部脚本或插件辅助 XML 开发，并允许在 LVGL 固件项目中分享 XML；同时明确限制未经许可向外分发读取、生成或解释该规范的 UI 编辑器、转换器和开发工具。WristFlow 是公开仓库，因此任何专用开发工具或含工具的 skill 在决定公开前须单独核对许可范围。本轮不发布这类工具。

## 8. 核验记录与来源

- 本地同步与核对命令：`git fetch origin`、`git status --short --branch`、`git log -6 --oneline --decorate`、`git ls-tree HEAD vendor/SiFli-SDK`；审核起点为同步的 main `2f1bfc1`，未跟踪 `huang.html` 保留。
- 文档审核 PR #5 已通过 Windows CI 并 Rebase 合入 main `a523955`。之后的视觉演示分支从该主干建立，记录见 UI-DEMO；不要把前次文档 CI 当成本次生成 C 的验证。
- Python XML 语法检查命令如下；语法良好仍不等于 LVGL schema、渲染或生成代码通过。本轮另外执行了 GUI 校验/预览/导出及四目标本机编译，无新增硬件证据。

```bash
python -c 'from pathlib import Path; import xml.etree.ElementTree as ET; files=sorted(Path("ui/xml").rglob("*.xml")); [(ET.parse(p), print("XML syntax OK:", p.as_posix())) for p in files]'
```

- 官方资料使用 `curl -fLsS --max-time 20 <URL>` 只读获取，HTML 正文使用 Python 标准库 `html.parser` 提取；未依据缓存摘要填写价格。
- [价格与功能表](https://lvgl.io/pro/pricing)：Community、CLI、商业价格，官网桌面版版本信息。
- [EULA v1.3](https://lvgl.io/pro/legal/eula)：2.4 免费许可、2.6 与 MIT 的关系、5.1 到期、7.2 产物所有权；具体使用范围以适用许可为准。
- [编辑器许可](https://lvgl.io/docs/pro/editor/license)、[Windows 安装](https://lvgl.io/docs/pro/editor/installation)、[CLI](https://lvgl.io/docs/pro/cli)。
- [项目文件](https://lvgl.io/docs/pro/syntax/project)、[使用导出的 C](https://lvgl.io/docs/pro/integration/using-exported-c-code)、[Online Viewer](https://lvgl.io/docs/pro/online-viewer)。
- [XML 规范许可 v1.0](https://lvgl.io/docs/pro/syntax/xml-license)：2025 年 6 月版本，内部工具与公开工具边界。
- [项目原始共识](reference/手表原型方案_v0.2_共识.md)、[项目术语](../CONTEXT.md)。
