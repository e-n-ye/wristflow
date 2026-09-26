# 可复现构建基线

本页各节结果仅适用于注明的日期与源码版本。当前 [设置与显示状态](PRODUCT-SETTINGS.md) 已通过 12/12 主机测试、官方 XML 导出和 Product/UI Demo 编译；Product 后续两次烧录与真机发现见 [上板记录](evidence/2026-09-26/product-settings-hardware.md)。前一版 [反馈修复](PRODUCT-UI-FEEDBACK.md) 与 [组件编辑](COMPONENT-EDITOR.md) 的真机范围见 [历史烧录证据](evidence/2026-09-26/product-ui-flash.json)。当前状态见 [STATUS](STATUS.md)，历史记录不代表最新源码验收。

## 设置与显示增量（2026-09-26）

工作树 `C:/Users/13984/.codex/worktrees/product-settings/wristflow`，分支 `codex/product-settings`，基于 `0edf7cc`。复用锁定 SDK junction 和原已注册环境；本机 `pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product` / `ui_demo` 调用本工作树实际包装器，最终均执行 `scons --board=sf32lb52-lchspi-ulp -j6`。本机辅助脚本留在忽略的 artifacts；普通检出仍用公共 `scripts/Build.ps1`。

主机命令为 `cmake -S tests -B artifacts/host-settings -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe`、`cmake --build artifacts/host-settings -j6`、`ctest --test-dir artifacts/host-settings --output-on-failure`；12/12 通过。实际 LVGL 指针测试和 26 张 390×450 快照在 `artifacts/host-settings/`；图片与导出日志在本工作树 `artifacts/settings-evidence/`。Editor 2.0.1 Community GUI 导出成功，预览、主机及固件 LVGL 均 9.4.0。

Product 记录为 `artifacts/product/20260926-164447-048/result.json`，主 BIN 3,388,888 B；UI Demo 记录为 `artifacts/ui_demo/20260926-164732-999/result.json`，主 BIN 3,380,564 B；退出 0 且产物验证通过。源码哈希、产物哈希和版本见 [精简证据](evidence/2026-09-26/product-settings.json)。SDK/子模块、分区和公共构建入口未改，无烧录或手动云端运行。修复过 XML flex 属性、字体缺字和无对应字形的图标；既有 SDK 编译/链接警告保留，详见专题记录。

上板发现应用列表左缘返回和持续亮屏后 KEY1 唤醒问题后，Product 修复版通过原项目已安装的 SDK v2.5.1 环境直接执行官方 `scons --board=sf32lb52-lchspi-ulp -j6`，退出 0，主 BIN 3,388,992 B（SHA-256 `5b9b00eea16dd20fb47600b564bee633042fa10382e426aa63a7c586ae9f2d11`）。工作树缺本地 `.tools`，直接运行公共 `Build.ps1` 在环境加载前退出 1；未改公共脚本。修复版已烧录及校验，用户确认左缘返回、KEY1 黑屏唤醒和持续亮屏 5 分钟通过；其余交互未验。命令、原始日志、三镜像哈希和未覆盖项见 [上板记录](evidence/2026-09-26/product-settings-hardware.md)。

## 反馈修复与组件编辑（2026-09-26）

工作树 `C:/Users/13984/.codex/worktrees/product-ui-feedback/wristflow`，分支 `codex/product-ui-feedback`，基于未合入的产品入口提交 `feabc1d`。本机使用相同锁定 SDK 的只读 junction 与已注册环境，`artifacts/build-isolated.ps1 -Example product` / `ui_demo` 调用当前工作树的 `scripts/Build.ps1`，最终仍为官方 `scons --board=sf32lb52-lchspi-ulp -j6`；不烧录。

修复真机编辑页边缘触控后，Product 记录为 `artifacts/product/20260926-151911-153/result.json`，UI Demo 为 `artifacts/ui_demo/20260926-152137-458/result.json`；两项退出 0、产物校验通过、SDK 无改动。对应主 BIN 为 3,330,088 B 和 3,322,908 B，完整哈希、源清单摘要、版本与本机日志位置见 [精简证据](evidence/2026-09-26/product-ui-feedback.json)。最终源文件逐项与两份构建记录哈希一致；Product 修正版已再次烧录、校验和启动。

主机用 `cmake -S tests -B artifacts/host-feedback -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe`、`cmake --build artifacts/host-feedback -j6`、`ctest --test-dir artifacts/host-feedback --output-on-failure`；修正版 10/10 通过，额外覆盖左右 `+` 与垃圾桶的边缘坐标和靠近按钮的空白防误退。Editor 2.0.1 Community 的 GUI `Ctrl+B` 导出成功，预览/主机/固件均为 LVGL 9.4.0；本次触控修复未修改 XML 或生成 C。XML 格式、变更中文 UTF-8、生成清单及实际 SCons 编译来源已核对。SDK、分区与公共构建入口未改，未重跑手动云端基线。

## 手动 CI 基线（2026-09-25）

工作流只响应手动 `workflow_dispatch`，执行完整 SDK 安装、Hello/BLE/Bringup/UI Demo/Product 和 PC UI 测试。PR 与 push 不触发云端构建，也不要求 `Build baselines` 状态检查；日常使用本地命令和真机证据。历史四目标完整运行约 11 分钟，新增 Product 后的总耗时未测量。在 SDK、工具链、公共构建入口变更或阶段验收需要干净环境证据时手动启动，并记录运行对应的 HEAD、命令、产物和失败位置。

PR #18 调整触发策略时，在 `C:/Users/13984/.codex/worktrees/product-runtime/wristflow` 核对唯一事件为 `workflow_dispatch`，当时 Windows 完整构建步骤与 `40e7658` 一致；变更 Markdown 的 UTF-8/相对文件链接和 `git diff --check` 通过。GitHub API 删除 main 的 `required_status_checks` 后读回为 null，PR 要求、管理员约束和禁止强推/删除仍保留。没有为该次触发策略调整再次执行完整构建；下面的产品增量随后增加了手动 Product 构建步骤，尚未执行云端验证。

## USB 产品目标（2026-09-25）

产品目标使用与其他黄山派目标相同的 SDK 和工具链，入口为：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example product -Jobs 4
```

本机最新构建在隔离工作树 `C:/Users/13984/.codex/worktrees/product-runtime/wristflow` 完成，实际 SCons 命令为 `scons --board=sf32lb52-lchspi-ulp -j6`。记录位于 `artifacts/product/20260925-193809-281/`，`exit_code=0`、`artifact_validation_passed=true`，`main.bin` 为 3,081,600 字节，SHA-256 为 `2f0ea7e583f5d8efe265d8c305c7623ba057cc45a32b9f319d58e4e05c5f0a17`。配置打开板载 RTC 和 FlashDB FAL；BLE/PM 仍关闭。产品分区在 `apps/product/project/sf32lb52-lchspi-ulp_hcpu/ptab.json` 增加 `settings`（`0x12DA8000`/`0x4000`），DFU、BLE、文件系统和代码区保持原值。

产品主机测试与行为说明见 [产品固件框架](PRODUCT-RUNTIME.md)。构建阶段完成源码检查、8 项主机测试和交叉编译；随后在用户授权下完成 COM5 写入/校验、启动和 RTC 即时校时，亮度 27/简洁表盘跨 RTS 复位恢复已有日志与用户确认。RTS 复位后时间回到未校时，需单独核查。完整真机范围和剩余项目见该文档的 USB 真机首轮，不把构建成功扩大为全部功能通过。

## 触摸修正增量（2026-09-25）

6/6 主机测试、官方 XML GUI 导出及 UI Demo 本机 SCons 编译通过。14:55 使用独立烧录助手向已确认的 COM5 写入并校验成功，启动串口已有 UI 耗时样本；构建脚本未增加烧录。命令、固件哈希、调度证据、测量限制及剩余验收见 [触摸修正与调度核验](UI-TOUCH.md)。下方菜单/应用与早期记录按各自版本理解。

## 菜单与应用增量（2026-09-25）

白灰资源、蜂窝菜单、秒表、表盘选择和亮度/按键接入已完成官方 GUI 导出；6/6 主机测试及四目标本机固件编译通过。最终模拟器在本次隔离工作区 `artifacts/acceptance-build/`；命令、版本、固件哈希、失败修复和硬件边界见 [菜单与应用](UI-APPS.md)。本次不执行烧录。

## UI 框架增量（2026-09-24）

新增 `core/` 与 `ui/runtime/`，由 PC 测试和官方 SCons UI Demo 共用。`Build.ps1` 的 UI Demo 源哈希记录增加这两个目录，SDK 锁和其他构建目标不变。5 项主机测试已通过；本轮隔离工作区的命令、固件结果、失败记录与硬件边界见 [UI 运行框架](UI-RUNTIME.md)。下方历史结果仍按各自日期理解。

## 当前黄山派基线（2026-09-22）

用户已确认退 Nano 完成、黄山派已下单。`sdk.lock.json` 的默认板型已改为 `sf32lb52-lchspi-ulp`；SDK v2.5.1 及两个子模块提交与下方历史版本表一致，SDK 源码零修改。Hello、BLE、Bringup 在本机实际编译链接成功，包装器产物检查通过；没有硬件运行验证。

项目根目录的 PowerShell 7 命令保持不变：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example hello -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example ble -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example bringup -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example product -Jobs 4
```

底层均为官方 `scons --board=sf32lb52-lchspi-ulp -j4`。工作目录分别是 `vendor/SiFli-SDK/example/get-started/hello_world/rtt/project`、`vendor/SiFli-SDK/example/ble/peripheral/project`、`apps/bringup/project`。VS Code 三个任务及 CI 继续使用同一入口。

黄山派沿用 SDK 自带的旧版 `ptab.json`。各工程产物在 `build_sf32lb52-lchspi-ulp_hcpu/`，主固件为 `main.bin`，分区表为 `ftab/ftab.bin`，bootloader 为 `bootloader/bootloader.bin`，并有 ELF、map、HEX、配置及编译数据库。脚本读取 SDK 生成的 `sftool_param.json` 定位并检查所有镜像，不执行其烧录指令。旧 Nano 的 `output/main.bin` 和根 `ftab.bin` 布局也可由同一检查识别。

| 目标 | 本机记录目录 | main.bin 字节 | SHA-256 |
|---|---|---:|---|
| Hello | `artifacts/hello/20260922-124620-144/` | 300336 | `5d1e396f2e9cd6edd35f0586d2c30dbd7f3dd4cf0f42a5bcf17c9a976e4173c0` |
| BLE | `artifacts/ble/20260922-124628-615/` | 490064 | `09d17264a1822a45731664091785dea1b0a0324f5be2af46f32c154500d396d2` |
| Bringup | `artifacts/bringup/20260922-130142-290/` | 614176 | `deadadab94b802be43bbd3eeab9580de1828c28fafa679c9254b9334984895fb` |

三项 SCons 退出码均为 0，`artifact_validation_passed=true`；记录现在把编译退出码与产物校验结果分开，并给出 `firmware_path`。版本为本机 Python 3.13.15、SCons 4.10.1、Arm GNU 14.2.Rel1/GCC 14.2.1。版本、完整产物哈希和生成配置摘录已归档到 [机器可读证据](evidence/2026-09-22/huangshan-baselines.json)。原始构建日志保留在上表各目录。

包装器另外通过 PowerShell AST 解析并提取实际产物校验块执行：现有 Nano/黄山派两种清单均通过，缺失镜像与重复 `main.bin` 清单均被拒绝。负例保留在 `artifacts/manifest-validation-ef6a6da8179d4e5ebcaca9c1a571c9cc/`，未删除真实固件。JSON 配置、SDK 锁定提交及生成编译数据库路径另行检查。

生成配置确认三项均为黄山派、MPI1 mode 3、CO5300/FT6146、390x450、AW32001 充电、KEY1=PA34/高电平有效及板级分区。Hello/BLE 的 PM 关闭；Bringup 的 PM/Deep Sleep/LVGL v9 开启且 BLE 关闭；三项触摸唤醒关闭。仍未实现同一固件 UI + BLE + PM 联合验证。

本轮失败和处理：

1. `artifacts/hello/20260922-123247-160/`：SCons 已成功，但旧包装器只检查 `output/main.bin` 而失败。该历史 JSON 的 `exit_code=0` 只代表编译器成功，不能当包装器成功。现改为读取官方镜像清单，检查必需镜像唯一且所有列出文件存在、非空。
2. `artifacts/bringup/20260922-124728-694/`：链接报 `CODE_START_ADDR` 未定义。项目未启用官方 Hello 已使用的 `CUSTOM_MEM_MAP`，旧版分区头未进入链接预处理；现在 `Kconfig.proj` 按官方模式启用板级内存映射，不修改 SDK 分区。重编译通过。
3. 保留 SDK 的 RWX LOAD segment 警告、LVGL `lv_obj_tree.c:274` 潜在空指针警告，以及旧版 ftab 子工程的 newlib syscall/`entry` 警告和 `img "dfu" not found`。本轮未启用 DFU，也未抑制警告；上板启动、地址布局和运行稳定性仍待验证。

云端检查由本次分支 PR 的最新 `Firmware / Build baselines` 验证，不能用历史 Nano CI 或本机成功代替。没有烧录、触摸、BLE 连接、休眠电流或续航证据。

## XML UI Demo（2026-09-23）

新增独立 `apps/ui_demo/`，用于编译官方 Editor 2.0.1 导出的 LVGL 9.4.0 页面。运行 `pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example ui_demo -Jobs 4`，或 VS Code 的 `WristFlow: Build UI Demo`。构建生成 C、字体和图片，不需要运行时 XML 或 Pro CLI；XML 修改后须先 GUI 导出。

本机四目标编译与产物检查均通过；最新 UI Demo 主固件 3,025,908 字节，PM/BLE 关闭、`LV_USE_XML=0`、`LV_USE_OBJ_NAME=1`。生成源/资源哈希纳入 `artifacts/ui_demo/20260923-092758-687/result.json`。12 张 342×282 RGB565 小时图的未压缩像素数据约 2.21 MiB；旧的无引用 `hour_seven_data.c` 已从 SCons 图片源收集中排除。原三个目标哈希不变；CI 增加第四项生成 UI 编译，Bringup 验收代码没有改动。截图、失败处理、原始日志位置和哈希见 [UI 演示证据](UI-DEMO.md)。

## PC 交互模拟器（2026-09-23）

运行 `pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Simulate.ps1`，或任务 `WristFlow: Run PC Simulator`。主机使用固定 SDK 内的 LVGL 9.4.0 官方 Win32 驱动，与黄山派 UI Demo 共用 C 适配层；XML 仍只负责外观。`-BuildOnly` 仅构建并执行三项主机测试，不打开窗口，不烧录。依赖、操作与最终四项固件构建记录见 [PC 交互验收](SIMULATOR.md)。本轮 CI 增加 MSVC 主机编译与 CTest，结果以最新 PR 检查为准。

## Nano 历史基线（2026-09-21）

以下版本、直接 SCons 板名、目录与哈希是切换前的 Nano 记录；日常构建以上方黄山派入口为准，不把下方产物用于新板。

验证日期：2026-09-21，Windows x64 / PowerShell 7。两项均实际完成编译和链接，SDK 源码工作树保持干净。没有连接、枚举或刷写设备。

同日新增第三个目标 `bringup`，已实际完成编译和链接。它位于项目自有目录 `apps/bringup/`，开启显示/触摸与 PM，详情及独立证据见 [上板基础固件](BRINGUP.md)。本页的 Hello/BLE 配置和首次固件哈希仍作为原始基线。

## 版本

| 项目 | 本次固定/使用的版本 |
|---|---|
| SDK | v2.5.1 / `421126d9f476ed8e2a6f0b0ca28a9f241c182e65` |
| Bluetooth 子模块 | `4d081f8fc4bfc0199bdccc1bb36b9fdc40e7240c` |
| SiliconSchema 子模块 | `304658c6edd5497211b7d152932327ce6e4450ff` |
| 编译板型 | `sf32lb52-nano_n16r16`，SCons 解析为 `sf32lb52-nano_n16r16_hcpu` |
| 编译器 | Arm GNU Toolchain 14.2.Rel1，GCC 14.2.1 20241119 |
| Python / SCons | CPython 3.13.15 / SCons 4.10.1 |
| CMake / Ninja | 官方安装器锁定 3.30.2 / 1.13.2；本轮入口为 SCons |
| sftool / sdk-exe | 0.2.5 / 0.1.1；安装但未用于烧录 |
| Conan | 2.27.0，配置 `sdk.conan-config.v2.4` |
| 现有引导工具 | Git 2.42.0.windows.1 / uv 0.11.21 |

根 `sdk.lock.json` 和 SDK gitlink 固定源码。官方 `tools/locks/default/lock.json`、`pyproject.toml` 和 `uv.lock` 固定 SDK 工具/依赖。官方 Python 约束是 3.13 系列，本次解析到 3.13.15；后续从零安装应记录实际补丁版本。此处承诺可重建过程，不承诺跨日期、路径和机器逐字节相同的固件。

## 当前机器直接使用

在项目根目录打开 PowerShell 7，Git 需在 PATH 中；首次安装还需要 uv 和网络，日常构建复用已安装环境：

```powershell
Set-Location D:\MY_Desk\project\wristflow
# 仅首次安装或修复环境时运行；当前机器已安装
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Install-Sdk.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example hello -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example ble -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example bringup -Jobs 4
```

工具已安装时无需每次重复安装。`Install-Sdk.ps1` 已实测复用现有环境；`Build.ps1` 已实测首次构建及 Hello/BLE 增量构建。脚本拒绝版本失配或有改动的 SDK，不会清理/覆盖用户改动。SDK 环境检查失败时先解决安装问题。

`Set-LocalEnvironment.ps1` 设置进程环境，工具、虚拟环境、uv 缓存放在 `.tools/`。构建激活从官方状态文件 `sifli-sdk-env.json` 读取此 checkout 的 default 环境，用其中已安装的 Python 依次执行官方 `tools/sdk_env.py check --profile default --offline` 和 `export --shell powershell --profile default --toolchain gcc --offline`，再加载其生成的 PowerShell 脚本。当前锁文件的兼容性仍由官方管理器检查；检查不通过即停止，不在构建中自动安装或联网修复。没有硬编码环境哈希或自行拼装编译器 PATH。

环境仅作用于当前进程，未永久改系统 PATH 或执行策略。安装仍调用官方 `install.ps1` 并使用已有 uv；首次官方 `uv python install` 也生成了用户目录下的便利入口，当前包装脚本已将后续 Python 入口限定到 `.tools/python-bin`。

包装脚本最终调用的官方命令：

```powershell
# 在项目根目录加载包装器，内部用已安装的 Python 离线检查并导出官方环境
. .\scripts\Set-LocalEnvironment.ps1
Enable-SdkEnvironment

# 基础程序
Push-Location .\vendor\SiFli-SDK\example\get-started\hello_world\rtt\project
scons --board=sf32lb52-nano_n16r16 -j4
Pop-Location

# BLE 外设
Push-Location .\vendor\SiFli-SDK\example\ble\peripheral\project
scons --board=sf32lb52-nano_n16r16 -j4
Pop-Location
```

这些命令只构建。生成的 `download.bat`、`uart_download.bat`、`sftool_param.json` 是产物，不能在未确认板卡、电源和端口时执行。

## 重新取得源码

项目的 `.gitmodules` 使用官方 GitHub 地址。已有项目 checkout 时执行：

```powershell
git submodule update --init --recursive
git -C vendor/SiFli-SDK rev-parse HEAD
git -C vendor/SiFli-SDK submodule status --recursive
```

项目现有公开仓库 `https://github.com/e-n-ye/wristflow` 和首次基线 `27ee4d1`；新电脑克隆方式见 [开发流程](../CONTRIBUTING.md)。初次获取 SDK 时本轮实际使用：

```powershell
git -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=30 clone --branch v2.5.1 --depth 1 https://gitee.com/SiFli/sifli-sdk.git vendor/SiFli-SDK
git -C vendor/SiFli-SDK submodule update --init --recursive --depth 1
```

仅在目标目录不存在时使用 clone 命令。GitHub 与官方 Gitee 镜像的 v2.5.1 标签已独立核对为同一提交。当前本地 SDK origin 是 Gitee，递归子模块也从 Gitee 取得；根项目对外记录的 URL 为 GitHub，提交一致。

## 产物与实际结果

每个示例 `project/build_sf32lb52-nano_n16r16_hcpu/` 下有：

- `main.elf`、`main.map`、`output/main.bin`、HEX。
- `bootloader/bootloader.elf`、`bootloader/output/bootloader.bin`。
- `ftab.bin`、`sftool_param.json` 和下载脚本。
- `.config`、`rtconfig.h`、编译数据库。
- BLE 额外包含官方 LCPU 子工程产物；这不构成任意自定义 LCPU 算法受支持的证明。

完整根路径：

```text
D:/MY_Desk/project/wristflow/vendor/SiFli-SDK/example/get-started/hello_world/rtt/project/build_sf32lb52-nano_n16r16_hcpu/
D:/MY_Desk/project/wristflow/vendor/SiFli-SDK/example/ble/peripheral/project/build_sf32lb52-nano_n16r16_hcpu/
```

| 示例 | 首次完整构建 | main.bin 大小 | SHA-256 |
|---|---|---:|---|
| Hello World | 退出码 0，链接成功 | 296832 B | `5d8e0fc8ce5e81fb2d4ab392786cd9176bd8855d2993d2e2b3d020732798ae7a` |
| BLE peripheral | 退出码 0，链接成功 | 486552 B | `9764d328867fad99989125b7e26bfa43cdfa291581a96c4acbfc9d9dbcef5d0f` |

初次完整构建证据保存在 [evidence/2026-09-21](evidence/2026-09-21)：两个 `*-first-build.json` 记录版本、命令、产物大小与哈希，`*.config` 为配置快照，`*-build.txt` 为实际完整 SCons 日志。最初记录器的 SCons 字段仅捕获了横幅，版本由官方 Python 锁文件与后续增量记录核实为 4.10.1；现已保存完整版本输出。

每次新构建还会生成 `artifacts/<hello|ble|bringup|ui_demo|product>/<时间>/build.log`、`result.json` 和配置副本；这些本机产物被 Git 忽略。`exit_code` 是 SCons 退出码，脚本还检查主 ELF、主 BIN、分区表和配置存在且非空，元数据命令失败会报错。记录中的 `project_sources` 保存工程入口和应用源文件哈希，UI Demo/Product 还记录 XML、字体/图片、共享核心与 UI 源码，产品目标另记录分区覆盖文件。增量构建复用未变文件是正常行为，不要求产物时间一定等于本次执行时间。

## 配置核验

| 配置 | Hello | BLE |
|---|---|---|
| CO5300 / FT6146 | 已启用并编译 | 已启用并编译 |
| 显示分辨率 | 390×450 | 390×450 |
| Bluetooth | 关闭 | 开启 |
| `BSP_USING_PM` / `RT_USING_PM` | **关闭** | **关闭** |
| `TOUCH_WAKEUP_SUPPORT` | 关闭 | 关闭 |

因此当前固件是官方基础功能构建基线，不是低功耗测量固件，也没有完成产品 UI 或 Gadgetbridge 协议。

源码依据（均在固定 SDK 提交）：

- `customer/boards/sf32lb52-nano_n16r16/hcpu/board.conf:31` 选择显示与触摸；`customer/boards/Kconfig_lcd:372` 进一步选择 CO5300、FT6146；同文件 557/611 行给出 390/450。
- `customer/peripherals/touch_panel/ft6146/ft6146.c:252` 创建 100ms 软定时器，初始化时启动，`:337` 的 `deinit` 停止并删除；真实熄屏流程是否走到 deinit、是否需要触摸唤醒需实测。
- `customer/peripherals/display/co5300/co5300.c:328` 的 DisplayOff 仅发送关显示命令；`:497` 把百分比写入亮度寄存器。视觉亮度和实际屏幕电源状态另验。
- `customer/boards/sf32lb52-nano_base/bsp_lcd_tp.c:20` 提供显示 reset/VADD 关断路径；TP PowerDown 拉 reset，不据此假设触控电源轨断电。
- `customer/boards/sf32lb52-nano_n16r16/ptab.yaml:64` 的 `psram_data` 默认只有 8MB；板配置 `BSP_QSPI1_MEM_SIZE=16`。实际容量测试与应用分区预算是两件事，暂不改分区。
- `docs/source/en/app_note/low_power.md:1096` 明确写着 `LCPU code is not open for modification on 52 series.`；BLE 构建生成 LCPU 镜像并不消除此文档边界，仍不预设可部署自定义传感器算法。

支持范围：Hello README 声明 any board，BLE 英文 README 声明 all platforms，其命令示范为另一板型；本次补充了 Nano 的实际编译证据。`example/pm/AMOLED` 官方列 Core N16R16，`gui_pm` 列黄山派，LVGL watch 列表未明确 Nano。本轮没有将这些示例的文档支持或其他板测量当作 Nano 成功运行。

## 问题及处理

1. GitHub clone 停在约 29MB。取消后保留目录 `vendor/SiFli-SDK.github-partial/`，从官方 Gitee 镜像完成同一提交及子模块下载；未执行删除。
2. 官方中国镜像预设的 Python 元数据地址报 `tls handshake eof`。关闭整组预设，保留官方 GitHub assets 镜像，Python 使用 uv 默认路径；安装成功。失败日志 `artifacts/logs/install.log`，成功日志 `install-default.log`。
3. Conan 配置包在 assets 镜像未取得，官方脚本回退 `downloads.sifli.com/dl/sifli-sdk` 后成功。
4. 链接器对 bootloader/main（BLE 另含 LCPU）发出 `LOAD segment with RWX permissions` 警告。构建成功但未隐藏警告；本轮未修改官方链接脚本，也未验证运行时内存保护。
5. 没有硬件，烧录、串口、触摸、显示、BLE、休眠和功耗均未验证。后续已新增 GUI + PM 的 Bringup 构建证据，但仍无其运行证据或 GUI + BLE + PM 联合固件。
6. VS Code 两个任务曾在激活阶段访问 `https://pypi.org/simple/rich/` 时遇到 `tls handshake eof`，尚未进入 SCons。原因是官方 `export.ps1` 每次执行 `uv run --with rich --with tomli_w`，会临时解析依赖。现由项目包装器直接使用已安装的 SDK Python 调用官方环境管理器，SDK 源码不变。修复后按 VS Code 任务相同命令，设置 `UV_OFFLINE=1`、`HTTP_PROXY`/`HTTPS_PROXY`/`ALL_PROXY=http://127.0.0.1:9`、空 `NO_PROXY`，Hello 和 BLE 增量构建均退出 0，BIN 哈希与上表一致。证据分别在 `artifacts/hello/20260921-213529-493/` 和 `artifacts/ble/20260921-213556-601/`；这是离线参数加不可用代理的验证，未物理断网。另用不存在的工具状态目录验证了缺失环境会立即报安装提示，没有改动真实安装。测试环境变量仅设置在测试进程中。

## 工作流范围

旧项目 `.github/workflows/ci.yml` 有 F411 固件、host CTest、模拟器 smoke、manifest 签名测试和汇总 Gate。它们依赖 STM32 链接布局、F411 产品目录和旧 LVGL，不复制到此项目。

当前采用本地同一构建入口、版本校验、实际产物检查、配置/日志/哈希留档。GitHub 工作流调用 Hello、BLE、Bringup 和 UI Demo 四个构建入口；云端结果以实际 Actions 记录为准，不能把本地构建称作 CI 已通过。出现独立业务逻辑时再加入相应 host tests。流程与套餐限制见 [开发流程](../CONTRIBUTING.md)。

2026-09-21 首次全新 Windows runner 已实际完成官方安装和三项编译，全部通过。云端配置与基线一致，BIN 哈希存在跨机器差异；详细版本、产物与运行链接见 [工作流首轮验证](WORKFLOW-EVIDENCE.md)。

## 2026-09-25 页面栈增量

隔离工作区 `C:/Users/13984/.codex/worktrees/ui-shell/wristflow` 的 `codex/page-stack-lifecycle` 使用现有主机构建：`cmake --build artifacts/acceptance-build --parallel 6`、`ctest --test-dir artifacts/acceptance-build --output-on-failure`，最终 6/6 通过。`pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example ui_demo` 完成官方 SCons UI Demo 编译，记录 `artifacts/ui_demo/20260925-153021-450/result.json`，主 BIN 为 3,072,252 字节、SHA-256 `e841ab797bb80c8c088134f0eac06a9c88c133a16e4e5617503ddb12d32a4c71`。未改构建入口、SDK 锁或公共配置；Hello/BLE/Bringup 沿用基线并由本次 PR 云端工作流复编译。

构建与烧录仍分开。独立助手先校验源文件、镜像、地址和 COM5 CH340 身份，再按此前上板授权写入并 verify；上板证据与用户反馈边界见 [页面栈验证](UI-NAVIGATION.md)。编译和自动启动采样不代替真机交互验收。
