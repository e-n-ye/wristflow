# 工程与所有权地图

本页只描述入口及对应关系，不保存任务队列或复制锁文件参数。活动项见 [STATUS 当前区](STATUS.md#current)，操作按 [任务索引](../PROJECT_INDEX.md) 选取。原地图基线为 `8be5821`；2026-10-10 按主线 `64c7e43` 复核下载入口，倒计时从 `56b4ceb` 完成第一轮、从 `e6e58be` 接第二轮，不代表重审全部工程。C2 草稿的内容不能自动套用到本工作树。

## 目标、配置与手写区

| 对象 | 事实来源与所有权 | 操作 / 验证入口 |
|---|---|---|
| 产品集成 | [apps/product](../apps/product/)；`project/SConstruct`、`SConscript`、`proj.conf`、`rtconfig.py` 与 `src/` 由项目维护 | [PRODUCT-RUNTIME](PRODUCT-RUNTIME.md)；Build 的 `product` 目标；联合验收另按版本绑定 |
| 固定数据演示 | [apps/ui_demo](../apps/ui_demo/)；与 Product 共用 core、运行时和生成 UI | Build 的 `ui_demo`；不能用演示值证明产品数据真实性 |
| 板级诊断 | [apps/bringup](../apps/bringup/)；自有工程 / 板级集成 | [BRINGUP](BRINGUP.md)；Build 的 `bringup`，不能代替 Product 联合验收 |
| 官方 Hello / BLE | 实际路径由 [Build.ps1](../scripts/Build.ps1) 的 `$projects` 定义，位于锁定 SDK 内 | Build 的 `hello` / `ble`；按官方 SCons，不迁移固件到 CMake |
| 平台无关逻辑 | [core](../core/) 的手写模型、协议、配置与状态；具体消费者由 SConscript / CMakeLists 决定 | 对应 [tests/CMakeLists.txt](../tests/CMakeLists.txt) 的主机用例及集成目标 |
| UI 行为适配 | [ui/runtime](../ui/runtime/) 手写数据绑定、路由、交互及生命周期 | [UI-RUNTIME](UI-RUNTIME.md)、[UI-INTERACTION](UI-INTERACTION.md) 的相关契约；受影响目标回归 |
| 倒计时模型 / 服务 | [countdown 模型](../core/countdown.h) 不依赖 LVGL；[port](../core/countdown_port.h) 定义命令 / 副本 / 时钟边界，[Product 服务](../apps/product/src/product_countdown.c) 持有任务与独立 RT timer | [COUNTDOWN](COUNTDOWN.md#round2)；纯模型、服务和实际 UI 三层主机检查。Product 组合注入后端后开放入口，未注入则阻断；板上 PM / 输入另验 |
| 倒计时 UI / 提醒 | [countdown_view](../ui/runtime/countdown_view.c) 具名绑定官方页面；[reminder_host](../ui/runtime/reminder_host.c) 持有一个高优先级覆盖槽，shell 保留原导航 / 显示策略并检查会话 | 同一 XML 用于演示、Product 页面与到期覆盖；不改变生成 C，不把覆盖宿主当作业务 scheduler 或完整多提醒队列 |
| 主机 / 模拟器 | [tests](../tests/) 与 [apps/simulator](../apps/simulator/)；使用 SDK 内 LVGL | [SIMULATOR 启动](SIMULATOR.md#启动)、[Simulate.ps1](../scripts/Simulate.ps1)；主机 CMake 与固件 SCons 分工独立 |

板型、SDK 提交、子模块及工具锁来源以 [sdk.lock.json](../sdk.lock.json) 为准；应用配置源为各 `apps/<目标>/project/` 下实际文件，包括存在时的板型覆盖目录。构建产生的 `.config` / `rtconfig.h` 是结果，不是手工维护配置源。厂商 `vendor/SiFli-SDK` 为固定 gitlink，功能任务不改厂商与子模块；确需进入时先读该目录适用规则。

## XML、资源与生成输出

- 设计源：[ui/xml/project.xml](../ui/xml/project.xml)、`screens/*.xml`、`components/*.xml` 和资源文件。官方 LVGL Pro 编辑器预览、导出及成功信号见 [UI-DEMO 本机免费版操作](UI-DEMO.md#本机免费版操作)。文档中本机路径是原环境记录，换工作树时选择实际 `ui/xml`，不自动打开旧目录导出。
- 官方输出：`*_gen.c/h`、`fonts/*_data.c`、`images/*_data.c` 及导出 CMake 清单，不手改，也不自制 XML 导出器。`wristflow_ui.c/h` 是官方创建的自定义入口，不与生成文件混同；业务放手写适配层。
- [ui/xml/SConscript](../ui/xml/SConscript) 是项目维护的 SCons 集成清单；不能因为位于 XML 目录就当成官方生成物。资源生成辅助脚本与 XML 导出是不同操作，运行前核对其写入范围。
- XML 改动的闭环包括官方实际预览 / 导出、名称与字形 / 资源核对及受影响目标编译；编译旧生成 C 不能证明新 XML 已导出。倒计时第一轮的官方导出、实际渲染与验证范围见 [专题](COUNTDOWN.md#round1)。

## 操作边界与产物

| 操作 | 前置条件 / 入口 | 成功信号与失败处理 |
|---|---|---|
| 环境与固件构建 | 在选定工作树根目录，按 [BUILD](BUILD.md#当前机器直接使用) 调用 Build；[Set-LocalEnvironment](../scripts/Set-LocalEnvironment.ps1) 核对锁与现有环境 | 实际退出码、`BUILD SUCCEEDED`、该次 `result.json` 与镜像校验；锁 / 工具失败先定位，保留改动，不绕校验或擅自重装 |
| 主机测试 / 模拟器 | 按 SIMULATOR 核对已有 CMake、编译器与独立输出目录；Simulate 会构建和测试，`-BuildOnly` 也会执行它们 | 实际 CTest 结果及对应源码，测试数以当前工程为准；旧文档的 3/6 项不能充当现版结果。已有窗口占用先停止本次操作，不强杀用户窗口 |
| 手动云端基线 | [CONTRIBUTING](../CONTRIBUTING.md) 与 [build.yml](../.github/workflows/build.yml)，仅 `workflow_dispatch` | 用实际提交和 Actions 结果；纯文档不 dispatch，无自动 Checks 不是阻断 |
| 下载与设备 | 先读 [HARDWARE](HARDWARE.md) 和 [Product 验收](PRODUCT-ACCEPTANCE.md) 的版本 / 设置边界；按 [FLASHING 当前入口](FLASHING.md#current-flash-entry) 使用 [Flash.ps1](../scripts/Flash.ps1)，旧 sftool 示例仅供历史方法参考 | 先核对实际镜像清单、端口、供电与授权；`-DryRun` 只校验本地清单 / 镜像，不打开设备。传输 / verify 与真实运行分开；身份未知就记录未知 |

**现版下载入口**：`64c7e43` 已包含 `scripts/Flash.ps1`。脚本要求 `-Example` 和实际 `-Port`，读取目标构建目录的 `sftool_param.json`，核对锁定板型、三镜像地址、文件与哈希；`-DryRun` 在调用 sftool 前返回。实际下载会执行 `write_flash --verify` 和软件复位，并写入该工作树的 `artifacts/flash/<example>/<run-id>/`。构建脚本仍不烧录；本轮仅核对源码，没有执行 DryRun 或设备操作。

固件输出位置由 Build 计算为对应工程的 `build_<board>_hcpu`，该目录不存在时改查 `build_<board>`；日志 / 配置 / 源码及产物哈希进入该工作树 `artifacts/<example>/<run-id>/`。`hardware_verified=false` 保持证据边界。主机输出由操作入口选择。`.tools/`、构建目录和日常 artifacts 不入库；提交的精简证据在 `docs/evidence/`。只读隔离工作树不保证本机依赖已可构建，多个任务不能共用可写构建目录、生成目录或设备。

工具入口 `.vscode/tasks.json` 与 `ui/xml/.vscode/tasks.json`（存在时）调用同一项目脚本；环境路径与端口按本机实际核对，不新增私人配置副本。完成变更后按 [文档职责表](DOCUMENTATION-WORKFLOW.md#responsibilities) 更新方法 / 契约 / 证据，进度只更新 STATUS 当前区。
