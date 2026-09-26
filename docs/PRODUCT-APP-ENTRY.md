# 默认组件页与统一应用入口

更新：2026-09-26。本增量落实用户最后确认的三页内容及点击跳转。源码、主机输入测试和固件构建已完成；之后按用户要求完成新版烧录、校验和启动采样，屏幕交互等待用户验收。完整组件编辑/设置契约见 [产品 UI 共识](PRODUCT-UI-SCOPE.md)。

## 行为与实现边界

| 默认页 | 内容 |
|---|---|
| 1 | 四个 quarter：心率、血氧、电量、压力 |
| 2 | 上 half 活力指标；下 quarter 计步、站立 |
| 3 | 上 half 天气；下 quarter 睡眠、闹钟 |

十个组件点击进入对应占位应用，显示名称、图标及未接入原因；电量摘要为 `USB`，其余缺失指标为 `--`，活力环不填充虚构进度。返回按钮或左缘右滑回到保留的组件页和原对象；KEY1 回表盘。三页仍与表盘组成循环。

`ui/runtime/app_registry.[ch]` 集中应用 ID、标题、图标、能力状态、原因、支持尺寸、页面工厂和数据提供者；默认槽位独立保存实例 ID 与配色。卡片路由、摘要与十四项产品菜单均取同一注册表。秒表、表盘选择、手电筒和现有设置沿用既有功能。`ui_demo` 保留旧三页演示数据与七项菜单，独立构建；其共享菜单元信息由注册表提供。

此时默认页配置是只读的 C 数据，几何来自官方 XML 生成物。尚不支持在设备上更换组件、增删页面或保存布局；两种颜色和实例字段为下一增量准备。未接入 BLE、IMU、PPG、真实电池或亮灭屏策略，未实现闹钟提醒。`USB` 是已确认供电条件，不是实时检测结果。

## 可复核证据

工作树 `C:/Users/13984/.codex/worktrees/product-runtime/wristflow`，分支 `codex/product-runtime`，构建基于 `145cc4c3ba99dd6c18c99bdabd0e382cd241ee4e` 加本次差异。以下 `artifacts/` 路径相对此工作树；源码哈希以两个构建 `result.json` 为准，不能只用构建时版本字符串识别固件。

- 资源：`uv run --offline --no-project --with pillow --with fonttools scripts/Generate-Ui-Assets.py --fonts-only`。增加所需中文字形及图标，不重生表盘图片。
- XML：Python ElementTree 检查 40 个 XML 格式通过；LVGL Editor **2.0.1 Community** 的 **Export Code and Recompile** 实际导出 C/字体/清单，日志 `Project compiled successfully`，预览和 SDK 均为 **LVGL 9.4.0**。
- 主机：`cmake --build artifacts/host-product --parallel 6`；`ctest --test-dir artifacts/host-product --output-on-failure`，最终 **8/8**。日志 `artifacts/product-entry-host-build.log`、`artifacts/product-entry-host-test.log`。
- 输入覆盖：十个槽位实际指针点击、返回按钮与左缘手势、原页/槽位对象保留、循环边界、KEY1、拖动回位防误触；十四菜单入口及菜单拖动；新标题/原因/图标字形；原有设置、手电筒亮度和 Demo 回归。没有把直接调用路由函数作为全部触摸证据。
- 真实 LVGL 快照：`artifacts/host-product/renders/product_cards_1/2/3.png`、`product_placeholder_1/2/3.png`、`product_launcher.png`；三页及代表占位页按 390×450 原像素拼接为 `artifacts/product-entry-preview.png`。人工检查中文、图标、边界与无数据状态，未发现遮挡或缺字。它们是主机渲染，不是硬件照片。

固件命令分别为 `pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product` 和 `-Example ui_demo`；助手复用已注册 SDK，实际官方命令 `scons --board=sf32lb52-lchspi-ulp -j6`，不烧录。新 checkout 的标准入口仍为 `scripts/Build.ps1`。

| 目标 | 记录目录 | main.bin 字节 | SHA-256 |
|---|---|---:|---|
| Product | `artifacts/product/20260926-002648-588/` | 3,108,080 | `2e62676a5d3951ab689822690cf29a6c85cb2d3260c3a16052042c3aa76e506a` |
| UI Demo | `artifacts/ui_demo/20260926-003100-175/` | 3,098,052 | `da209de67b9f94db39379734aea1995542daa00299859abbad321fd3dc1ddb27` |

两目标退出 0、产物检查通过，Product 193/193、UI Demo 189/189 源码记录与本地一致；[精简证据](evidence/2026-09-26/product-app-entry.json) 保存固件、主机日志、预览和核心源码哈希。SDK `421126d9f476ed8e2a6f0b0ca28a9f241c182e65` 及两个子模块版本不变、工作树干净。沿用 Python 3.13.15、SCons 4.10.1、Arm GNU 14.2.1；主机 Ninja/Debug、UCRT64 GCC。继续保留类型范围/省略初始化字段、SDK 空指针、链接 RWX 和缺失 DFU 镜像等编译警告，未隐藏。

## 本轮修复与下一步

初次字体检查发现标题缺“量”；快照复核发现天气提示缺“等”。补全字体裁剪与 XML 字符集合后重新官方导出，测试扩大到注册表全部标题和原因，最终通过。新 XML 弧线使用标准 style selector 子节点，兼容 XML 格式检查。官方首次预览的 TinyTTF cache/WASM 临时加载报错在资源导出、编译成功后消失；不把临时失败当作最终成功证据。

只读代理复核路由、事件上下文及 Demo 路径，未发现可操作问题；验证代理采集两目标构建和哈希，主代理复核截图与最终差异。没有触发云端 CI。新增三张参考照片按 SHA-256 归档，参考总数 35；照片中的数字不被当作产品读数。

变更文本的 UTF-8、JSON/XML、Markdown 本地链接检查通过。原始 `git diff --cached --check` 仅提示六个新增官方生成 C 文件的 EOF 空行；保留官方输出，用 `git -c core.whitespace=-blank-at-eof diff --cached --check` 验证其余空白规则，退出 0，不修改仓库或全局 Git 配置。

## 2026-09-26 新版烧录

用户要求先烧录新版供真机验收。工作树 `codex/product-runtime` 的 `feabc1d268b63b92e14bc4f097c0f32f1e9370b8` 无未提交改动；烧录前重新核对三镜像及 `sftool_param.json` 的 SHA-256，均与上述 Product 构建记录一致。Windows 当前在线设备为 `USB-SERIAL CH340 (COM5)`，VID/PID `1A86:7523`，实例 ID 与上次黄山派一致。

使用已安装 SiFli `sftool 0.2.5`，参数 `-p COM5 -c SF32LB52 -m nor -b 500000 --connect-attempts 3 --after soft_reset write_flash --verify`。按构建参数写入 bootloader `0x12010000`、main `0x12020000`、ftab `0x12000000`；未整片擦除，未向 settings 分区写入。工具退出码 0。镜像哈希分别为 `e59d70db44367d524fbb46a8a014ce0092462e86959d101a8e2e00ad27bb5e21`、`2e62676a5d3951ab689822690cf29a6c85cb2d3260c3a16052042c3aa76e506a`、`80e7647f47c008720f1bed83397bdb407295a27fe217fcf7e9d00dc9fd71fc5b`。命令输出与清单位于 `artifacts/flash/product/20260926-product-01/`。

在 1000000 baud、8N1 下 RTS 脉冲复位并读取启动串口：`settings` 分区创建成功，`[product] storage=ready restore=yes brightness=27 face=simple rtc=available`，CO5300 显示及 FT6146 触摸驱动打开，报告 `RTC=not synchronized, battery absent, BLE/PM pending`。这证明固件启动和旧设置读取，不代表新版十个组件的屏幕触摸交互已通过；后者等待用户反馈。RTC 复位后需重新校时，属于此前已知行为。

下一有界增量：建立组件页持久配置及写入完成/失败模型，再接长按编辑、同尺寸两色选择、左右插页、填满点勾、单次确认删除和 1–6 页边界。RTC 复位问题独立排查，功耗与长测继续后置。
