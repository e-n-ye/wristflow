# 使用 VS Code 编译

版本提交、分支、GitHub PR 与云端检查见 [开发流程](../CONTRIBUTING.md)。本页任务和默认快捷键不变。

2026-09-22 主板改为黄山派，任务统一读取 `sdk.lock.json` 中的 `sf32lb52-lchspi-ulp`；下面 2026-09-21 的日志和哈希仍属于 Nano 历史验证。黄山派构建结果见 [构建说明](BUILD.md)。

SDK 和工具链已经装好，Hello World 与 BLE 外设示例已实际编译通过。VS Code 可以作为日常编辑和编译入口，不必每次手动输入命令。编译不需要开发板；烧录、断点调试和实际运行验证需要硬件。

项目自有的上板基础工程也已编译通过：`F1` → `任务: 运行任务` → **WristFlow: Build Bringup**。成功标志是 `BUILD SUCCEEDED: bringup (...)`，行为和到货验证见 [Bringup 实验](BRINGUP.md)。原有默认快捷键 `Ctrl+Shift+B` 仍是 Hello。

产品固件使用同一入口：`F1` → `任务: 运行任务` → **WristFlow: Build Product**。它复用 UI Demo 的页面栈并接入 RTC/设置状态；当前仍是 USB 阶段，编译通过不代表已烧录或真机功能通过，范围见 [产品固件框架](PRODUCT-RUNTIME.md)。

## 打开与编译

1. 在 VS Code 选择“文件 → 打开文件夹”，打开 `D:\MY_Desk\project\wristflow`，不要只打开其中一个 C 文件。
2. 选择“终端 → 运行生成任务”（英文界面为 Terminal → Run Build Task）。默认任务是 `WristFlow: Build Hello World`；也可按 `Ctrl+Shift+B`。
3. 编译 BLE 时，按 **F1** 打开命令面板，输入“运行任务”，选择 **任务: 运行任务**（英文为 `Tasks: Run Task`），然后输入 `WristFlow` 并选择 **WristFlow: Build BLE Peripheral**。它是任务列表中的名称，不是一直显示的按钮。菜单方式是“终端 → 运行任务”；顶部菜单空间不足时，“终端”可能收在 `…` 内。
4. 查看窗口下半部分选中“终端”的黑色区域；这就是任务输出，不需要再打开一个窗口。成功时末尾会出现 `BUILD SUCCEEDED: hello (...)` 或 `BUILD SUCCEEDED: ble (...)`、固件路径，且任务进程正常结束。编译错误会显示在终端，符合 GCC 格式的诊断也会出现在“问题”面板。

产品任务成功时末尾为 `BUILD SUCCEEDED: product (...)`；它与 UI Demo、Bringup 一样只构建，不执行烧录。

任务自动激活项目的官方 SDK 环境，并调用 `scripts/Build.ps1`。无需重新安装工具链，也不用每次先运行 export。编译任务不包含烧录动作。VS Code 顶部的“运行当前 C 文件”和 CMake 扩展的 Build 入口不是本项目的构建入口。

`Done! SiFli-SDK profile default is active.` 表示环境已自动激活，不是还要点击的操作。末尾“终端将被任务重用，按任意键关闭”表示任务已结束，不是在等你输入编译命令。

## 激活时访问 PyPI 失败的处理

2026-09-21 两个任务出现 `Failed to fetch: https://pypi.org/simple/rich/` / `tls handshake eof`，失败位置在环境激活，还没进入代码编译。之前的包装器使用官方 `export.ps1`，它每次会经 uv 临时解析依赖；即使完整工具链已经安装，也可能因此受网络故障影响。

包装器已改为复用已安装的 SDK Python，离线执行官方环境检查和导出。首次安装仍需要网络；当前机器无需重装、换镜像或修改 VS Code 配置，重新运行原来的 Hello/BLE 任务即可。正常激活会先显示 `Checking environment compatibility ... OK`，末尾仍以 `BUILD SUCCEEDED: hello (...)` 或 `BUILD SUCCEEDED: ble (...)` 判断构建成功。

修复后用离线参数和不可用代理执行了两个任务的相同命令，均增量构建成功、退出码 0，固件哈希不变。记录位于 `artifacts/hello/20260921-213529-493/` 和 `artifacts/ble/20260921-213556-601/`。后续若提示已安装环境缺失或损坏，应按提示修复安装；构建入口不会自动联网修复。

## 表格乱码的处理

2026-09-21 用户首次从 VS Code 运行 Hello 已成功，退出码为 0，`main.bin` 为 296832 字节，哈希与基线相同。当时内存用量表边框乱码来自 Python 的 UTF-8 输出被 PowerShell 按代码页 936 解码，原始故障日志保留在 `artifacts/hello/20260921-211531-139/`，没有重写。

包装脚本现已统一 Python 与 PowerShell 的 UTF-8 输入输出，并为表格设置 120 列，避免管道默认 80 列裁掉信息。修改只影响当前构建进程，没有修改 SDK 源码或 Windows 全局区域设置。再次按 `Ctrl+Shift+B` 会使用修复后的脚本，历史终端文字不会自动修正。

修复验证：主动从代码页 936 启动 Hello 构建，退出码 0，完整表格和成功提示正常，固件哈希保持不变。证据在 `artifacts/hello/20260921-212312-744/`。

## 看代码

- 基础程序入口：`vendor/SiFli-SDK/example/get-started/hello_world/rtt/src/main.c`。
- BLE 示例源码：`vendor/SiFli-SDK/example/ble/peripheral/src/`。
- 自有应用入口：`apps/bringup/src/main.c`，可修改测试超时和界面；配置在 `apps/bringup/project/proj.conf`。
- `.vscode/c_cpp_properties.json` 使用实际生成的 `compile_commands.json`，供已安装的微软 C/C++ 扩展识别宏、头文件和编译参数。命令面板的 `C/C++: Select a Configuration` 可选择 `Huangshan - Bringup`、Hello 或 BLE 索引配置；此操作不改变编译任务目标。

Hello/BLE 任务用于复编译固定版本的官方示例，Bringup 用于编译项目自有应用。SDK 有完整性保护，直接修改 `vendor/SiFli-SDK` 会使脚本拒绝构建；修改 `apps/bringup/` 不受此限制。

## 产物和限制

当前固件位于各工程的 `project/build_sf32lb52-lchspi-ulp_hcpu/`，日志和哈希位于 `artifacts/<hello|ble|bringup|ui_demo|product>/<时间>/`，具体见 [构建说明](BUILD.md)。旧 Nano 构建目录保留但不用于黄山派烧录。PC 模拟器入口与范围见 [模拟器说明](SIMULATOR.md)，不能代替硬件运行验证。

本机已有 VS Code 与微软 C/C++ 扩展。思澈官方另有 [SiFli CodeKit](https://marketplace.visualstudio.com/items?itemName=SiFli.sifli-sdk-codekit)，可后续评估其工程和调试界面；本轮未安装，也未假设它会自动识别本项目的独立工具目录。

本机任务显式使用已经存在的 PowerShell 7：`%USERPROFILE%/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe`，并补充现有 uv 与 Git 的 PATH。这样从桌面独立启动 VS Code 时也不依赖 Codex 终端临时注入的 PATH。此配置针对当前电脑；迁移电脑或此运行时路径失效时，将任务 command 改为已安装的 PowerShell 7 路径，并同步 uv/Git 路径。

验证范围：任务 JSON、编译数据库和可执行路径检查，以及按任务定义实际调用构建命令；未通过桌面 UI 自动点击验证菜单、代码跳转或断点调试。
