# 天气 UI 增量

更新：2026-10-05。本增量把 Product 的天气入口从占位页改为可浏览的五页天气界面：当前天气、逐小时预测、每日预测、天气指数和日升日落。当前数据来自 `core/weather.c` 的确定性本地 fixture，页面已经预留加载中、暂无数据、更新失败和重试状态，手机天气 provider 尚未接入。

本轮按 Redmi Watch 4 日升日落页实拍重做了曲线视觉。日升日落页使用一张 336×128 的透明 ARGB8888 贴图：白色日间弧线、虚线地平线和地平线下较暗的延伸线在贴图中固定；太阳点、标题、日出/日落文字和天气背景仍由页面控件与运行时负责。贴图没有嵌入整张手表照片，也没有把相机曝光、屏幕扫描线或透视差异当作设计目标。晴天使用明亮蓝色，阴天/多云使用明显偏灰的灰蓝色，背景由 fixture 状态驱动。

## XML、Pro 导出和运行时的准确关系

- 视觉源是 `ui/xml/screens/screen_weather_sun.xml`；贴图源是 `ui/xml/images/weather_sun_track.png`，其生成脚本为 `scripts/Generate-Weather-Sun.py`。`ui/xml/globals.xml` 以 `argb8888` 注册 `weather_sun_track`。
- 使用 LVGL Pro Editor 2.0.1 Community、LVGL 9.4.0 打开 `ui/xml/project.xml`，在目标 `huangshan_390x450` 中预览日升日落页并执行 `Ctrl+B`（Export Code and Recompile）。输出区记录 `Project compiled successfully`、`Initializing custom C code using LVGL v9.4.0`；原始输出保留在 `artifacts/lvgl-pro-weather/20261004/pro_output.txt`。
- Pro 生成物包括 `ui/xml/screens/screen_weather_sun_gen.c/.h`、`ui/xml/wristflow_ui_gen.c/.h`、`ui/xml/images/weather_sun_track_data.c` 和 `ui/xml/file_list_gen.cmake`。资源清单实际编译 `weather_sun_track_data.c`，生成代码把 `weather_sun_track_image` 绑定到 `weather_sun_track`。
- `ui/runtime/weather_screen.c:create_sun_page()` 挂载 `screen_weather_sun_create()`，再覆盖生成页面根背景色，所以日升日落页的结构和贴图来自 XML/Pro 生成页面，天气状态背景仍由运行时传入。
- 目前只有日升日落页由生成页面挂载。`screen_weather_current_gen.c/.h`、`screen_weather_hourly_gen.c/.h`、`screen_weather_daily_gen.c/.h` 和 `screen_weather_indices_gen.c/.h` 虽在 Pro 生成清单中，但五页外层与前四页内容仍由 `ui/runtime/weather_screen.c` 运行时 C 创建；不能把五页都称为 XML 驱动。

## Redmi Watch 4 视觉基线

用户提供的实机照片确认天气页面的层级：顶部标题与时间固定，当前天气突出显示温度和天气状态，逐小时/未来天气使用横向分页，天气指数与日升日落作为独立信息页，底部分页点持续可见。照片拍摄造成的色差、透视和屏幕纹理只用于判断层级，不复制进 UI。

背景色随天气状态变化：晴天使用明亮的高饱和蓝色；阴天切换为明显偏灰的灰蓝色。五页在同一天气状态下保持一致的页面底色，白色文字与图标在两种底色上保持可读。

日升日落贴图位于 XML `(x=27, y=157)`，尺寸 `336×128`；全局地平线约为 `y=240`，弧顶约为 `y=160`，左右交点约为 `(72,240)` 与 `(318,240)`。太阳点中心为 `(272,199)`，处于下降段且高于地平线；暗色延伸线位于地平线下方。

## 行为与实现

- 当前页显示城市、更新时间、温度、天气状况、最高/最低温和空气质量。
- 逐小时页支持 24 小时数据横向分页；每日页支持 7 天数据横向分页。外层五页纵向滚动，预测页内容区保留水平手势，边缘返回仍可用。
- 指数页显示空气质量、湿度、风力和紫外线；日升日落页显示时间和太阳轨迹。
- `ui/runtime/weather_screen.c` 负责五页容器、滚动状态、动态背景和日升日落页挂载；`core/weather.c` 只提供 fixture 和状态文案；注册表仍由 `ui/runtime/app_registry.c` 统一提供天气卡片摘要和页面入口。

## 可复核证据

工作树为 `C:/Users/13984/.codex/worktrees/weather-ui/wristflow`，分支 `codex/weather-ui`，HEAD `9ce5b3e32e338a279741464709f11b8bc3213931`。SDK `421126d9f476ed8e2a6f0b0ca28a9f241c182e65` 及两个子模块未修改。

- 主机命令：`cmake --build artifacts/weather-host-build --parallel 6`；`ctest --test-dir artifacts/weather-host-build --output-on-failure --timeout 60`，最终 **16/16** 通过。`tests/weather_ui.c` 覆盖五页、水平/垂直分页、加载中/暂无数据/错误/重试、返回边界、晴天/多云底色，以及贴图尺寸、ARGB8888、弧顶、虚线间隔、暗色延伸、地平线和太阳点关系。
- 快照：`artifacts/weather-host-build/renders/weather_current.ppm`、`weather_current_sunny.ppm`、`weather_hourly.ppm`、`weather_daily.ppm`、`weather_indices.ppm`、`weather_sun.ppm`；`weather_sun.png` 是同一真实 LVGL 快照的 PNG 预览。当前页两种底色可由像素复核为灰蓝 `#687f91` 与亮蓝 `#0bb9f2`。
- Product 命令：`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 6`，退出码 0，官方 SCons 和产物校验通过。最新记录为 `artifacts/product/20261005-141441-099/result.json`；`main.bin` 为 7,612,188 B，SHA-256 `ee2ec45da3a93f02d47259a067c83c8d65f1ec27e78eae77f6b830e8447198b6`，`hardware_verified=false`。
- 烧录前确认 USB-only、未接电池，并枚举到 `USB-SERIAL CH340 (COM5)`、VID:PID `1A86:7523`、实例 `USB\VID_1A86&PID_7523\6&A8355E6&3&1`。使用 sftool 0.2.5 对 bootloader/main/ftab 执行三镜像 `write_flash --verify`，退出码 0；未整片擦除、未写 settings。启动串口记录到 CO5300、FT6146、`display on` 和 Product runtime 启动。原始证据见 `artifacts/flash/weather-20261005/evidence.json`；屏幕曲线视觉和触摸仍待用户现场观察确认。

这些结果分别证明源码/资源可读、Pro 导出成功、主机行为与视觉快照通过、Product 可编译、三镜像烧录校验通过并正常启动。

2026-10-05 16:25 手势修复真机验证：固件（`main.bin` 7,612,588 B）重新烧录并软复位后，用户现场观察确认：
- 天气预测与未来天气页面中，横向单页滑动正常，已消除因惯性连跳多页的现象。
- 页面中部内容区域直接上下拖动可顺畅带动外层五页垂直滚动，垂直滚动链放行生效。
- 用户同时观察到滑动中存在轻微掉帧（不够丝滑），该现象为既有已知表现，当前不作为关键路径阻塞项，留作后续性能阶段专项优化。
- 当前下一步转入手机天气数据同步（Gadgetbridge BLE 协议）需求对齐与落地。

2026-10-05 17:45 Gadgetbridge BLE 天气数据同步与全链路验证：
- 协议解析与事件联动：实现 Gadgetbridge `GB({"t":"weather",...})` 严格 JSON 解析，提取城市 `loc`、温度 `temp`（开尔文自适应减 273.15 转摄氏度）、天气文本 `txt`、OWM 天气代码 `code`、湿度 `hum`、风力 `wind`。
- 双向触发与时效：进入页面无数据或点击重试时主动通过 BLE 发送 `\n{"t":"weather"}\n` 请求；手机推送到达后台静默更新，RAM 运行时保留；3 小时软过期。
- 视觉与主题联动：由 OWM code / txt 关键字判定晴天明亮蓝（`#0bb9f2`）或多云灰蓝（`#687f91`），并在数据更新时自动刷新前台活动页面。
- 自动化与主机测试：`phone_protocol` 单元测试覆盖各类有效/非法/边界天气报文；CTest 16/16 全部通过。
- 硬件构建与烧录：`scripts/Flash.ps1` 扩展支持 `product` 目标；SCons 编译通过（`main.bin` 7,613,924 B），三镜像成功烧录至 COM5 并完成 `--verify`。
- 真机诊断验证：通过串口受控 RTS 脉冲复位后系统正常启动；执行 `wf_ble mock_weather` 注入测试报文，真机串口回读 `[product] weather updated: 杭州市 25 C, condition=晴 code=800 hum=65%`，端到端解析与业务分发闭环。

2026-10-05 18:05 乱码与时间同步修复真机闭环：
- 中文乱码根因修复：`apps/product/src/phone_protocol.c` 原 `normalize` 对 `c >= 128` 单字节盲目转为 `\u00xx`，导致 UTF-8 多字节中文（如“杭州市”、“晴”）被拆散为 Latin-1 符号（`æ · å`）。新增多字节 UTF-8 序列探测与直通逻辑，兼顾 ISO-8859-1 回退兼容。修复后真机直接正确显示“杭州市”、“晴”。
- 指数页缺字方块修复：`ui/runtime/weather_screen.c` 指数数值采用纯 ASCII `metric_56` 字体，无法显示全角破折号 `—` 产生缺失字块 `[]`。将空值改用 ASCII `--`，并将风力提取纯数值，避免 88px 容器字符溢出截断。
- 右上角时间联动：`ui/runtime/apps.c` 与 `weather_screen.c` 接入系统 RTC 快照动态联动；未校时显示 `--:--`，校时后各预测与指数页右上角时间随系统实时更新。
- 硬件复验：编译生成 `main.bin`（7,614,332 B）并烧录通过；受控 RTS 脉冲复位后注入 `wf_time 1791223500` 与 `wf_ble mock_weather`，串口准确输出 `[product] weather updated: 杭州市 25 C, condition=晴 code=800 hum=65%`，屏幕时间同步与中文字符正常。

2026-10-05 18:30 卡片缺字方块修复与 BLE 主动同步落地：
- **架构与设计决策记录（可追溯性备查）**：
  - **背景与问题**：表盘主页天气卡片（`metric_half` / `metric_full`）副标题展示动态天气状况（如“晴”、“阴”、“雨”、“雪”）时，原 XML 声明使用的 `body_20` 为静态页面精简字库，未收录这些汉字，导致实机出现缺字方块 `[]`。
  - **决策对比与采纳方案**：用户确认采纳**定向运行时重定向方案**（在 `ui/runtime/components.c:paint()` 仅针对天气组件副标题动态挂载 `notification_22` 完整字库），放弃重新导出 XML 字库的冗长重度方案。
  - **决策依据与协调性论证**：
    1. **字源 100% 同源**：在 `scripts/Generate-Ui-Assets.py` 中，`body_20` 与 `notification_22` 均派生自同一套字体源文件（开源思源黑体 `NotoSansSC-Regular.ttf`），字重、笔画骨架完全一致，绝无字体杂糅违和感。
    2. **排版安全性**：`notification_22`（22px）相比原 `body_20`（20px）仅增加 2 个像素。卡片副标题区域（`metric_half.xml`）宽度预留 280px，卡片行高 174px，显示“晴 25°/18°”或“等待手机同步”具有极大的排版留白，无任何超框、截断或溢出风险。
    3. **组件隔离安全性**：仅对 `app->id == "weather"` 的天气卡片动态挂载，心率、活力指标、系统状态等其他组件继续保留 `body_20`，完全不破坏现有组件体系。
    4. **全局视觉统一**：与天气应用内部页面（`weather_screen.c` 中“刚刚更新”、“空气质量”、“多云”等）统一使用的 `notification_22` 达到 100% 视觉统一步调。
  - **追溯说明**：此决策已冻结并归档。若后续组件系统整体重构字号层次或引入新字库，可直接定位并调整 `ui/runtime/components.c` 中的动态挂载逻辑。
- **BLE 订阅主动同步与缓存重置**：
  - 在 `apps/product/src/product_ble.c` 的 BLE 订阅事件（`PACKET_SUBSCRIBE && subscribed`）中，在发送版本号之后自动向手机 NUS 队列追加发送 `\n{"t":"weather"}\n` 报文，并在建立连接时自动调用 `wristflow_weather_reset()` 清空此前调试遗留的 Mock 数据，以便及时接收并呈现手机真实下发的实时天气。
  - 在串口调试命令 `wf_ble` 中新增 `clear_weather` 子命令，支持随时手动重置天气缓存并触发主界面刷新。
  - 在 `apps/product/src/main.c` 的 `WF_EVENT_PHONE` 事件中补充即时刷新 shell 快照（`wristflow_ui_shell_update`），使天气更新或重置时主页卡片无延迟实时刷新。
- **验证结论与真机确认（2026-10-05 18:43）**：
  - 主机 CTest 16/16 全部 PASS；
  - Product 固件编译成功（`main.bin` 7,614,596 B，构建产物见 `artifacts/product/20261005-182943-624`）；
  - **真机验收通过**：用户现场实机观察复核确认，主页天气卡片副标题中的缺字方块 `[]` 已彻底解决，动态汉字显示完整，排版无异常；
  - **当前待闭环项**：真实所在地天气同步联调（排查 Gadgetbridge 手机端天气数据源下发链路）。


