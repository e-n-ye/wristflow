# 产品固件框架

现行系统的职责图、已实现低功耗机制及测量边界见 [2026-10-14 系统／PM 说明](PM-EVENT-WAKE.md#current-power-system)。下文保留各版本的接入和验收记录；2026-09 的“BLE／PM 关闭”是历史状态，不能据此判断当前 Product。

2026-10-15后续设置保存已改为[事件worker与有限重试](PRODUCT-SETTINGS.md#save-reliability-20261015)：偏好设置／布局按请求代次反馈，写后回读才确认成功；Product在现有界面采集点与息屏前提交，UI不调用Flash。旧设置记录格式、闹钟存储及下述充电／电源显示边界保留；本轮不操作设备。

2026-10-16新增[手动限时电机服务](MOTOR-PULSE.md)：main在消费者启动前请求PA20默认OFF，之后由独立worker处理显式1–250ms脉冲、截止和取消；关闭未确认保留IDLE并拒绝新脉冲。没有自动振动绑定或UI变更，状态读回只表示GPIO锁存器。软件候选未下载，设备仍为设置补验的`310ad9fe…`。

<a id="power-display-20261015"></a>
## 2026-10-15 真实电源检测与电压显示

按 `541d841` 核对：当前 Product 已开启 BLE／基础 PM，接入真实手机通知／归组历史、天气／AQI、倒计时／闹钟和呼吸训练。模块及分版本验证见[现状矩阵](PRODUCT-ACCEPTANCE.md#status-20261015)，活动项仍只见 STATUS；下方“入口和边界”“已实现行为”保留 2026-09-25 初版语境。

原 `541d841` 的 `battery_unavailable=true` 被两种表盘／电量卡片映射为 `USB`，电量入口还写“未接入电池”；控制中心的 USB 文案只随手机连接标志改变。用户报告已接电池仍显示 USB 后，本轮从 `0cfd857` 接入独立的 `power` 快照，不再从缺失百分比推断供电。

`product_power.c` 只读 AW32001E 的 ID／REG08 PG 位；`USB OK` 表示芯片检测到有效外部输入，`USB --` 表示没有有效外部输入，`USB ?` 表示检测失败／未知，不能据此证明线缆在位、独占供电来源或电池在位。位定义来自 [AW32001E V1.4 第39页](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1157/AW32001E.pdf)。不启用含自动启充路径的 SDK charger，也不写充电寄存器。

Product 显式开启 ADC1，读取固定 SDK `bat1` 的 VBAT 专用通道 7。使用有错误返回的 `rt_device_read`，不能使用丢弃转换错误的 `rt_adc_read`；普通路径已应用驱动校准与 VBAT 因子，0.1mV 除以10得到 mV，不再乘2。官方 EPRO 中模组 VBATS 与充电器 BAT 同接 VBAT 网络，详见[硬件依据](HARDWARE.md#power-sampling-20261015)。图纸路径、编译和实物电压准确度分别验收。

首候选 `d4f17491…` 的同步采样在真机恢复首帧前阻塞：KEY1事件到PG失败／ADC成功约5.025秒，再约0.3秒亮屏。它提前消耗显示活动期限，用户报告几乎每次亮屏迟缓、亮时已暗；该候选睡醒回归失败。固定SDK的I2C2默认5000ms超时与此吻合，失败后的驱动reset能解释下一轮成功，但首次恢复传输失败的底层原因未证明。

修正版用事件驱动的Product采样线程：UI只请求／复制短临界区快照，不执行总线I/O；启动、亮屏每5秒及首帧完成后请求，事件合并、不积压样本，不新增周期timer。后台持有IDLE，先发布ADC结果，再读取PG；PG慢路径不会阻塞UI或延后独立电压。息屏清除旧快照、停止新请求，epoch拒绝睡醒前的迟到结果；进行中的总线调用可完成后释放IDLE，不声称能中断SDK调用或保证绝对硬件截止。SDK ADC含线程等待，I²C两次读取也是顺序观察。各字段失败降为未知，后续有效采样恢复。

`a9918aa2…` 修正版36/36主机及Product编译／哈希检查、同版三镜像verify／启动通过；三次KEY1事件到亮屏为306／307／308ms，最后一次allow=1、软件休眠路径计数已增长，用户确认亮屏速度／亮度／电压／触摸正常。串口关闭后，用户确认仅电池下约3.96V、USB-／USB --及KEY1／触摸正常。本轮设备回归有界通过，具体版本和首版失败均保留在[证据](evidence/2026-10-15/power-status.json)。

表盘／卡片沿用原位置，百分比未知时显示有效电压（两位小数）或 `--`；电量页额外显示 USB 状态和未知百分比，关于页／控制中心同步使用快照。控制中心为保持原单行宽度使用 `USB+`／`USB-`／`USB?` 对应有效／无有效输入／未知，后面的手机连接状态仍单独表示 BLE。UI Demo 继续显示其确定性百分比。VBAT 有电压不能证明电池存在，USB-only／断电池时也只显示该节点读数；不估算 SOC、不把电压当成容量或续航。`wf_charger status` 保持只读诊断，名义4170mV／80mA不是本轮采样值。

当前源码、构建与设备边界见 [STATUS](STATUS.md#current)；软件测试不能替代插拔／睡醒显示回归或电压表同点比对。随后独立实现的[手动充电软件候选](CHARGER-PREFLIGHT.md#manual-charge-20261015)新增显式启停和独立短会话监测，ADC完整序列在两worker间由项目mutex同步；不改变上述电源显示语义。候选`81e594e3…`只完成软件验证，尚未下载；设备仍为`a9918aa2…`禁充版。

<a id="rtc-reset-contrast"></a>
## 2026-10-12 RTC 软件重启与 RTS 对照

用户授权排查复位丢时，并提出未接电池的可能原因。先查用户提供的模组规格书 V0.2 §7.1：供电输入为 VSYS，VBATS 是电池电压检测接口；再查 [立创复位设计](https://wiki.lckfb.com/zh-hans/hspi-sf32lb52/lckfb-open-sf32lb52-mod/design-guide-reset.html)与[思澈串口自动下载设计](https://wiki.sifli.com/hardware/SF32-auto-download.html)：52 系列串口 RTS 通过电源开关实现掉电复位。USB 线仍连接，不能证明模组供电在 RTS 期间连续。

本轮从 `origin/main@0bd071d` 进行源码核对，沿用已写入的呼吸旋转固件 `9f3e285`，主 BIN 为 `ba815750…`，三镜像重哈希一致；没有改固件、SDK、构建配置或再次下载。用户确认同一黄山派／屏幕、USB-only、无新增接线。COM5、1000000 8N1、DTR／RTS=false，以同一 UART 连接只做一次 SDK `reboot` 和一次 RTS 150ms：

| 路径 | 日志与只读 `show_date`（UTC） | 有界结论 |
|---|---|---|
| SDK 软件 `reboot` | `11:05:03` → `11:05:16`，日期均为 2026-10-12；`Init RTC, wake = 1`，Product 启动为 `RTC=in range` | 此次软件重启保时通过；两次读时之间没有校时命令，日志未见 BLE 连接／校时 |
| 外部 RTS 150ms | 软件重启后最后有效读时为 `11:05:16`；RTS 后为 `2000-01-01 00:00:10`，`wake = 0`、`RTC=not synchronized` | 复现丢时，符合官方掉电复位机制；RTS 前临近一次查询未获回执，不将其算作有效读时 |

Product 启动只读取 RTC，没有无条件 SET_TIME。锁定 SDK 的 RTC 初始化按冷启动和备份标记选择 NORMAL／SKIP；`HAL_RTC_Init` 不写日期／时间寄存器，备份标记本身在 RTC 寄存器中。`wake` 是初始化模式，不是电压测量或直接的复位原因寄存器。现有证据支持区分软件重启与 RTS 掉电，不需要用 Flash 上次时间替代当前时间。

本机已有 EPRO 文件名为 `huangshan-v1.2.epro`，但相关 sheet 内部标题为 V1.3；其中 U19（LP5240HVF）由 `CH_RTS#` 控制、串在 VSYS 与模组 pin27 之间，仅作为参考拓扑。实物 PCB 版本、该开关实装与电压波形未核实，不能据此承诺接电池后跨 RTS 保时。未接电池意味着拔掉 USB 后无法持续供电；本轮没有接电池、测电源轨或验证断电保时。

19:08:46 成功以电脑当前真实 UTC `1791803326` 校时，随后 `show_date` 为 `11:08:48`（当地 19:08:48）。此前息屏发送无回执，用户保持呼吸训练常亮后的首条命令有乱码并报 command not found，重发新取的真实 UTC 后成功；失败记录保留。没有修改闹钟配置，自动调度保存 generation=64–66；串口于 19:09:09 正常关闭，采集进程已退出。精简版本、原文、哈希及边界见 [RTC 对照证据](evidence/2026-10-12/rtc-reset-contrast.json)，原始日志在本机复用的 `breathing-product/wristflow` 忽略目录 `artifacts/rtc-investigation/20261012-190422-953/`。

以后普通保时重启使用现有 SDK `reboot`；下载或 RTS 掉电后核对 RTC 有效性，必要时同步当前真实时间。此项关闭的是“软件重启是否同样丢时”的有界排查；接电池、长按复位、电压波形、长期漂移和全掉电保时仍未验。未新增编译／主机测试，不将此前呼吸测试计为本轮 RTC 验收。

## 2026-10-12 呼吸训练前台接入

[呼吸第二轮](BREATHING.md#round2)开放 Product 菜单，训练由 UI 线程的前台模型／view 拥有，离页和重启不后台恢复。滚轮确认值纳入原 settings 采集与 FlashDB 保存 worker，24 字节编码升级 v6，旧 v1–v4 的时长默认为 1；v5 留给独立 C2 草稿，仍拒绝并回默认，未合入抬腕功能。

训练申请独立 owner／reason／期限的亮屏保持，按用户亮度，完成／取消／暂停／离页／销毁释放；显示期限先让 owner 安全结束再释放，不覆盖用户全局常亮。普通消息更新缓存后抑制提示；统一提醒宿主首次覆盖暂停训练，末条关闭后从吸气继续，已存在的退出确认另行保留，不自动确认。调用上下文、到期边界、恢复与版本／证据见上述专题；没有新增后台训练线程或 RTC 服务。

主机 32/32、严格配置／显示模型与 Product／UI Demo 编译及源码／产物重哈希通过。物理 Flash 恢复、真实显示／触摸、消息与提醒覆盖留同版 USB 第三轮，不能由软件接线扩大为硬件、PPG、功耗或续航通过。下方 2026-09 的 24 字节历史版本及旧 BLE／PM 状态只描述当时基线。

## 2026-10-10 倒计时服务接入

本节覆盖下方 2026-09 首轮的相关实现状态；历史证据继续按原版本使用。现版已启用 BLE 与息屏自动 PM，配置源仍为 Product `proj.conf` / 锁定 SDK；本轮不修改 SDK、BLE 协议或存储 worker。

[product_countdown](../apps/product/src/product_countdown.c) 在 PM 事件邮箱后启动，独占易失任务、mutex 与一次性 RT 软 timer；通过 [平台无关 port](../core/countdown_port.h) 注入 Product UI。只有具备后端的组合才开放倒计时；演示和未注入后端的视觉构造继续分别保留本地模型 / 阻断入口。服务不读写 FlashDB，启动 IDLE，所以固件重启取消活动任务。

`WF_EVENT_COUNTDOWN` 唤醒主线程；息屏等待每次先对照服务副本，再处理手机通知 / KEY1。UI 在 LVGL timer 停止时创建提醒，恢复首帧同时重画顶层覆盖，帧完成后才提亮。确认交付后 30 秒无输入息屏，后续 KEY1 保留原提醒；关闭 / 重来恢复有效原会话。具体时钟、分段等待、输入 / 确认和测试边界见 [第二轮证据](COUNTDOWN.md#round2)。这是源码 / 主机与编译接入，不是同版硬件验收，也不关闭 C2 / 通知联合矩阵或功耗。

更新：2026-09-26。本文保留首个 USB 产品框架及 2026-09-25 真机证据。应用入口 `feabc1d` 曾上板；此后的 [反馈修复](PRODUCT-UI-FEEDBACK.md) 与 [组件编辑保存](COMPONENT-EDITOR.md) 版本 `2dbc782` 已通过主机测试和两目标编译，Product 也已烧录、校验和启动；菜单/编辑交互尚待实测。完整需求见 [产品应用与组件编辑](PRODUCT-UI-SCOPE.md)；设置扩展及亮灭屏仍待接入，不能沿用下列旧硬件结果宣称新源码通过。

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

**当时未通过：RTC 跨 RTS 复位保时（解释由 2026-10-12 对照补充）。** 为排除串口开关干扰，在同一次串口连接、USB 持续供电下，用 `wf_time` 校时后通过 SDK 的无参数 `show_date` 只读查询，得到 `2026-09-25 12:34:46`、`12:34:53`（UTC），确认 RTC 正常走时；RTS 复位后返回 `2000-01-01 00:00:02`，产品正确显示 `--:--`。末尾重新校时到 UTC `1790339700`，读到 `2026-09-25 12:35:02`。证据位于 `artifacts/hardware/product-20260925-rtc-reset-experiment/`，脚本退出 0；没有修改固件。

当时确认不是单纯 UI 显示错误，尚未区分供电与初始化原因。原定软件复位／RTS 对照已由[上方 2026-10-12 记录](#rtc-reset-contrast)完成；NORMAL 初始化模式不能直接等同于软件写入默认日期。旧原始观察保留，新结果只对应所列固件与 USB 条件，不把 Flash 中的上次时间当作当前正确时间。

证据摘要已提交为 [product-usb.json](evidence/2026-09-25/product-usb.json)。启动横幅仍显示构建时的 `690009c1`，这是提交前构建的版本字符串；173 个源码哈希与提交 `2885f00` 一致，以主固件 SHA-256 识别此次实际固件。初次探测只列出蓝牙 COM3/4，用户接线后出现 COM5；普通 Python 缺少 pyserial，改用现有 SDK Python 3.13.15/pyserial 3.5。早期 `date` 查询没有得到日期、且捕获了启动输出，不计为读时证据；查明 SDK 导出名为 `show_date` 后用同一连接的受控实验取代。

## 后续验收

首轮烧录、启动、RTC 校时/走时及亮度/表盘保存已有上述证据。RTS 复位保时失败；软件复位保时、长时间 RTC 漂移、连续 30–60 分钟联合运行、BLE 连接/通知、PM/熄屏、电流、续航和传感器仍未覆盖。同日用户后续决定优先应用/组件/设置框架，RTC 排查并行，休眠功耗与长测后置。以下保留为该版本的历史验收流程，不要求文档更新后重复执行，也不作为当前框架增量的前置门槛：

1. 核对黄山派、USB 供电、串口身份与上述产品固件哈希，再用独立烧录流程写入，保留启动日志；首次应看到存储状态及 RTC 状态。
2. 电脑用 `[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()` 获取 UTC 秒数，通过串口发送 `wf_time <秒数>`。核对读回成功、UTC+8 显示以及跨分钟更新；未校时时必须显示缺失状态。
3. 修改亮度和表盘，停止操作至少 3 秒并确认 `[product] settings saved`，再复位和重新上电，核对 `restore=yes` 与设置恢复。打开/关闭手电筒后再次确认用户亮度未被保存成 100。
4. 完成 10 轮“表盘 → 菜单 → 应用 → 返回”，观察无重启、栈损坏和设置丢失。BLE 和熄屏/唤醒接入后，再执行 BASELINE 的完整 30–60 分钟联合验收。

启动日志、校时结果和重启前后的设置须另记硬件证据。无电池断电后的时间与 Flash 设置分开判断；断电保时不在本轮承诺内。

该目标会随手动 `Build baselines` 一起构建，但工作流只响应 `workflow_dispatch`。本轮未触发云端构建，仅有本机证据；干净 runner 的 Product 路径尚未执行。历史四目标云端完整构建约 11 分钟，新增 Product 后的总耗时未测量。后续需要 SDK、工具链、公共入口或阶段版本的干净环境证据时再手动运行并记录对应提交。
