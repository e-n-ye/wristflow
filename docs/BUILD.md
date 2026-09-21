# 可复现构建基线

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

每次新构建还会生成 `artifacts/<hello|ble|bringup>/<时间>/build.log`、`result.json` 和配置副本；这些本机产物被 Git 忽略。`exit_code` 是 SCons 退出码，脚本还检查主 ELF、主 BIN、分区表和配置存在且非空，元数据命令失败会报错。记录中的 `project_sources` 保存工程入口和应用源文件哈希。增量构建复用未变文件是正常行为，不要求产物时间一定等于本次执行时间。

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

当前采用本地同一构建入口、版本校验、实际产物检查、配置/日志/哈希留档。GitHub 工作流调用现有 Hello、BLE、Bringup 三个构建入口；云端结果以实际 Actions 记录为准，不能把本地构建称作 CI 已通过。出现独立业务逻辑时再加入相应 host tests。流程与套餐限制见 [开发流程](../CONTRIBUTING.md)。

2026-09-21 首次全新 Windows runner 已实际完成官方安装和三项编译，全部通过。云端配置与基线一致，BIN 哈希存在跨机器差异；详细版本、产物与运行链接见 [工作流首轮验证](WORKFLOW-EVIDENCE.md)。
