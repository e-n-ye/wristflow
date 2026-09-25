# 产品固件框架

更新：2026-09-26。本文保留首个 USB 产品框架及 2026-09-25 真机证据。最新 [应用入口增量](PRODUCT-APP-ENTRY.md) 已实现注册表、默认三页和占位应用跳转，并完成主机测试及两目标编译，尚未烧录。完整需求见 [产品应用与组件编辑](PRODUCT-UI-SCOPE.md)；编辑器、设置扩展及亮灭屏仍未实现，不能沿用下列硬件结果宣称通过。

## 入口和边界

产品目标位于 `apps/product/`，与 `apps/ui_demo/` 共用 `core/`、`ui/runtime/` 和 `ui/xml/`。`ui_demo` 仍是固定数据的确定性 UI 回归目标，`bringup` 仍是显示、触摸、亮度和电源诊断目标。产品目标的入口是：

```text
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example product -Jobs 4
```

产品配置复用黄山派 `sf32lb52-lchspi-ulp`，开启板载 RTC 和 FlashDB FAL；BLE 与 PM 目前仍关闭，避免在未接入业务生命周期前把诊断状态混入产品状态。构建脚本不包含烧录。

## 已实现行为

- 启动读取 RTC 秒数，接受 2026–2099 的有效范围；显示统一按 UTC+8。未读到有效时间时表盘显示 `--:--`，USB 串口可用 `wf_time <UTC Unix seconds>` 校时，写入后立即读回校验。
- 亮度默认 60，表盘默认 `diffusion`。设置记录使用 24 字节的版本化数据和 FNV 校验，不写入 C 结构体原始内存；记录存放在单独的 `settings` FlashDB 分区（`0x12DA8000`，`0x4000`）。设置变化空闲 1.5 秒后由后台线程写入，FlashDB 使用独立互斥锁。
- 当前黄山派没有电池，因此表盘和设置页显示 `USB`；心率、活动和系统指标显示缺失/未接入状态，不使用演示百分比或虚构传感器值。
- 产品 UI 使用启动恢复的亮度和表盘 ID，继续支持表盘、蜂窝菜单、控制中心、应用、返回和 KEY1 路径。手电筒临时亮度不会覆盖保存的用户亮度。

时间范围只是异常值筛查，不保证 RTC 已与手机或电脑同步；当前未接电池，USB 断电后不能依赖 RTC 保时。这里的 `USB` 表示已确认的开发供电条件，没有实现电源来源自动探测。设置写入失败会每隔至少 5 秒重试；存储不可用或记录损坏时使用默认值。写入完成前断电可能丢失最近的修改。

## 证据

源码检查、主机测试、交叉编译和硬件验证分开记录：

本机工作树为 `C:/Users/13984/.codex/worktrees/product-runtime/wristflow`，分支 `codex/product-runtime`，基于 `690009c1eaf98e777177c295b6d12bea7fae36fe` 的本次差异。下列 `artifacts/` 路径均相对此工作树，未提交到 Git；每份 `result.json` 包含源文件与产物 SHA-256。产品与 UI Demo 使用独立工程输出目录；SDK 和两个子模块保持锁定、源码无修改。

实际固件命令为 `pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example <product|ui_demo|hello|ble|bringup>`，底层为官方 `scons --board=sf32lb52-lchspi-ulp -j6`。本机助手只将当前分支 `Build.ps1` 的环境激活接到已注册的原工程 SDK 环境，工程仍指向本工作树；助手不提交。新 checkout 按 BUILD 安装自己的环境后直接使用正式入口。

主机使用已有 CMake Ninja/Debug 配置和 `D:/msys64/ucrt64/bin/gcc.exe`，增量命令为 `cmake --build artifacts/host-product --parallel 6`，随后执行以下 CTest。

- 主机 `ctest --test-dir artifacts/host-product --output-on-failure`：8/8 通过，包括 `product_state` 和 `product_ui`。测试覆盖记录编码/校验、时间边界、缺失状态、设置恢复、手电筒临时亮度和 `ui_demo` 同进程回归。
- 受影响的五个黄山派目标均本机退出码 0 且产物校验通过：

  | 目标 | 记录 | `main.bin` | SHA-256 |
  |---|---|---:|---|
  | Product | `artifacts/product/20260925-193809-281/` | 3,081,600 B | `2f0ea7e583f5d8efe265d8c305c7623ba057cc45a32b9f319d58e4e05c5f0a17` |
  | UI Demo | `artifacts/ui_demo/20260925-195635-089/` | 3,072,868 B | `79b3575d08d1b3e6d95922734cb890b9ab26ad10c21eefd170ae79b32a8911a4` |
  | Hello | `artifacts/hello/20260925-193001-089/` | 300,336 B | `5d1e396f2e9cd6edd35f0586d2c30dbd7f3dd4cf0f42a5bcf17c9a976e4173c0` |
  | BLE | `artifacts/ble/20260925-193011-129/` | 490,064 B | `09d17264a1822a45731664091785dea1b0a0324f5be2af46f32c154500d396d2` |
  | Bringup | `artifacts/bringup/20260925-193026-949/` | 614,176 B | `c241d699feb2220eb23d30b353296ed2b56aadd2073655f1fa6a62843c8080a8` |

- 共同环境为 SDK `421126d9f476ed8e2a6f0b0ca28a9f241c182e65`，Bluetooth 子模块 `4d081f8fc4bfc0199bdccc1bb36b9fdc40e7240c`，SiliconSchema 子模块 `304658c6edd5497211b7d152932327ce6e4450ff`，Python 3.13.15、SCons 4.10.1、Arm GNU 14.2.1。
- 本机产品预览为 `artifacts/product-preview.png`，包含未校时、有效时间和扩散表盘三种状态。日志仍保留 SDK 的 RWX LOAD、ftab `entry` 和 newlib syscall 警告；这些警告没有被隐藏。

最终检查逐项比对五个目标 `result.json` 的源文件与主 BIN 哈希，均匹配当前文件；主机测试输入自最后 8/8 通过后未修改。结构化比较产品与锁定板级分区表，除新增 16 KiB 设置区外完全一致。曾发现较早 UI Demo 记录中的共享 `apps.c` 哈希已过期，已增量重编并以上表最新结果替换；保留旧记录，不沿用过期证据。

## 2026-09-25 USB 真机首轮

用户本轮授权烧录、复位及验收。目标仍为上述 Product 产物（源码 `2885f00`），USB 供电、无电池；当前 Windows 识别 `USB-SERIAL CH340 (COM5)` / VID `1A86`、PID `7523`。离线复核 173 个源文件、三镜像哈希及地址均与构建记录一致，结果在 `artifacts/product-preflash-check.json`。

用 sftool 0.2.5 执行 `-p COM5 -c SF32LB52 -m nor -b 500000 --connect-attempts 3 --after soft_reset write_flash --verify`，写入同一构建目录中的 bootloader/main/ftab，地址依次为 `0x12010000`、`0x12020000`、`0x12000000`；退出码 0。没有整片擦除，没有向设置区发送镜像。烧录日志及镜像清单在 `artifacts/flash/product/20260925-product-01/`。

启动采样为 1000000 baud、8N1、无流控，RTS 脉冲 0.15 秒复位后只收 12 秒。`artifacts/hardware/product-20260925-01/uart.txt` 记录：

```text
[I/FAL] The FAL MTD NOR device (settings) created successfully
[product] storage=ready restore=defaults brightness=60 face=diffusion rtc=available
[product] USB stage: RTC=not synchronized, battery absent, BLE/PM pending
```

CO5300/FT6146 初始化完成。串口发送 `wf_time 1790338564` 后返回 `[product] RTC synchronized: UTC=1790338564; display UTC+8`，证明该次校时写入与即时读回通过；证据为同目录 `time-sync-uart.txt` / `time-sync.json`。用户将亮度改为 27、选择简洁表盘并等待保存，随后 RTS 复位：

```text
[product] storage=ready restore=yes brightness=27 face=simple rtc=available
[I/DBG] set brightness 27
[product] USB stage: RTC=not synchronized, battery absent, BLE/PM pending
```

复位采样在 `artifacts/hardware/product-20260925-settings-reboot/`。用户确认简洁表盘、约 27% 亮度及 `--:--` 与屏幕一致；手电筒打开变亮、退出恢复，以及 10 轮导航/逐级返回/KEY1 均正常。随后用户确认秒表离开页面约 10 秒仍继续计时、暂停/清零正常，电池显示 USB、传感器不显示虚构值、蓝牙显示 OFF；等待保存后拔插 USB，简洁表盘和约 27% 亮度仍保留。设置跨 RTS 复位有日志及屏幕证据，跨 USB 断电有用户观察证据，后续启动日志仍为 `restore=yes brightness=27 face=simple`。

**未通过：RTC 跨 RTS 复位保时。** 为排除串口开关干扰，在同一次串口连接、USB 持续供电下，用 `wf_time` 校时后通过 SDK 的无参数 `show_date` 只读查询，得到 `2026-09-25 12:34:46`、`12:34:53`（UTC），确认 RTC 正常走时；RTS 复位后返回 `2000-01-01 00:00:02`，产品正确显示 `--:--`。末尾重新校时到 UTC `1790339700`，读到 `2026-09-25 12:35:02`。证据位于 `artifacts/hardware/product-20260925-rtc-reset-experiment/`，脚本退出 0；没有修改固件。

这不是单纯的 UI 显示错误，也不能只归因于没有电池。SDK `drv_rtc.c` 在冷启动且备份标志为 0 时走 NORMAL 初始化；现有 `wake=0` 日志与该路径一致，但不能据此确定备份域复位、板级供电或启动初始化哪个是根因。下一项有界排查是在同一 UART 连接比较软件复位与 RTS 复位，必要时增加启动模式、备份标志和原始 RTC 秒数的只读诊断。尚未加入替代保时策略，不把 Flash 中的上次时间当作当前正确时间。

证据摘要已提交为 [product-usb.json](evidence/2026-09-25/product-usb.json)。启动横幅仍显示构建时的 `690009c1`，这是提交前构建的版本字符串；173 个源码哈希与提交 `2885f00` 一致，以主固件 SHA-256 识别此次实际固件。初次探测只列出蓝牙 COM3/4，用户接线后出现 COM5；普通 Python 缺少 pyserial，改用现有 SDK Python 3.13.15/pyserial 3.5。早期 `date` 查询没有得到日期、且捕获了启动输出，不计为读时证据；查明 SDK 导出名为 `show_date` 后用同一连接的受控实验取代。

## 后续验收

首轮烧录、启动、RTC 校时/走时及亮度/表盘保存已有上述证据。RTS 复位保时失败；软件复位保时、长时间 RTC 漂移、连续 30–60 分钟联合运行、BLE 连接/通知、PM/熄屏、电流、续航和传感器仍未覆盖。同日用户后续决定优先应用/组件/设置框架，RTC 排查并行，休眠功耗与长测后置。以下保留为该版本的历史验收流程，不要求文档更新后重复执行，也不作为当前框架增量的前置门槛：

1. 核对黄山派、USB 供电、串口身份与上述产品固件哈希，再用独立烧录流程写入，保留启动日志；首次应看到存储状态及 RTC 状态。
2. 电脑用 `[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()` 获取 UTC 秒数，通过串口发送 `wf_time <秒数>`。核对读回成功、UTC+8 显示以及跨分钟更新；未校时时必须显示缺失状态。
3. 修改亮度和表盘，停止操作至少 3 秒并确认 `[product] settings saved`，再复位和重新上电，核对 `restore=yes` 与设置恢复。打开/关闭手电筒后再次确认用户亮度未被保存成 100。
4. 完成 10 轮“表盘 → 菜单 → 应用 → 返回”，观察无重启、栈损坏和设置丢失。BLE 和熄屏/唤醒接入后，再执行 BASELINE 的完整 30–60 分钟联合验收。

启动日志、校时结果和重启前后的设置须另记硬件证据。无电池断电后的时间与 Flash 设置分开判断；断电保时不在本轮承诺内。

该目标会随手动 `Build baselines` 一起构建，但工作流只响应 `workflow_dispatch`。本轮未触发云端构建，仅有本机证据；干净 runner 的 Product 路径尚未执行。历史四目标云端完整构建约 11 分钟，新增 Product 后的总耗时未测量。后续需要 SDK、工具链、公共入口或阶段版本的干净环境证据时再手动运行并记录对应提交。
