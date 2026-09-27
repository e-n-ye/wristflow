# Product 首个 BLE 实验 A1

日期：2026-09-27。沿用用户已确认的 [下一阶段计划 PR #24](https://github.com/e-n-ye/wristflow/pull/24)：先在同一 Product 上完成手机连接、校时及真实通知链路，再尽早做 PM/事件唤醒，随后接完整通知 UI。手机为用户确认的 iQOO Neo9S Pro+，截图型号代码 V2403A、软件版本 `PD2403B_A_14.0.12.2.W10.V000L1`；用户随后确认 Gadgetbridge 已安装。黄山派用 USB，未接新电池。

## 当前结果与范围

源码已接入标准 Nordic UART Service，广播名 `Bangle.js WristFlow`。Product 的 UI、RTC、设置持久化与 BLE 共存；PM 仍关闭。本轮先通过串口诊断验证数据，不显示通知浮窗或息屏预览，也未实现勿扰、通知亮屏和振动。

主机 13/13 测试通过，Product 官方 SCons 编译与产物校验通过。手机发现问题通过改为通用可发现广播解决；Android 陪伴关联/添加流程曾失败，清除本设备绑定、强行停止并重新打开 Gadgetbridge 后选择“不配对”，14:32 主界面出现已连接设备卡片。板端确认 **connected=1、subscribed=1、MTU=131，手机校时写入/读回 result=0**。随后真实 QQ 中文通知、手机撤回、本地删除不清手机通知、六次手动重连及一次蓝牙开关后的自动恢复均取得证据，用户确认 KEY1 唤醒后界面正常。最终解析拒绝、未知帧和队列丢包均为零。同 ID 替换已通过主机测试，尚无手机同 ID 更新样本；远离返回与后台长期重连未验。历史 UI 真机结果不自动覆盖本固件；见 [机器可读记录](evidence/2026-09-27/ble-a1.json) 和 [脱敏硬件摘录](evidence/2026-09-27/ble-a1-hardware.txt)。

## 协议与实现

- 官方 Gadgetbridge 稳定版 [0.94.0](https://codeberg.org/Freeyourgadget/Gadgetbridge/releases/tag/0.94.0)，安装主线 `gadgetbridge-0.94.0.apk`。源码 `BangleJSDeviceSupport.java` 的 NUS 初始化、分包发送、JSON-ish 编码、校时和通知撤回已核对。中文使用 `\uXXXX`；设备设置中 **Text as Bitmaps 保持关闭**。
- NUS 服务 UUID 为 `6e400001-b5a3-f393-e0a9-e50e24dcca9e`；手机写入 `...0002...`，手机订阅 `...0003...`。名称匹配 Gadgetbridge 的 Bangle.js 识别规则。
- BLE 回调只投递消息队列，独立线程按换行组帧并处理 `GB({...})`。不执行 Espruino 脚本，只提取合法 `setTime` UTC 秒并写入/读回 RTC；显示仍沿用 UTC+8。
- RAM 最多保存十条通知，同 ID 更新并移到最新位置；手机 `notify-` 撤回后移除；本地删除不向手机发送清除指令。重连保留本地消息，但丢弃残帧；队列丢包后舍弃到下一换行，避免损坏帧误入库。
- 帧上限 8191 字节；来源/标题/正文 UTF-8 上限 160/320/1600 字节，超限整条拒绝，不截断半个字符。支持常见 JS 字符转义和 Unicode 代理对；`atob(...)`、位图、任意表达式及嵌入 NUL 拒绝。关闭位图后普通中文有文本路径，但不承诺完整 Bangle.js 协议兼容。
- GATT 使用实验用无认证权限，未验证绑定、加密、白名单及隐私边界；本轮只用测试通知。常规串口日志只列 ID/计数，主动 `list` 才输出正文。

```text
wf_ble status
wf_ble list
wf_ble remove <id>
wf_ble clear
```

## 构建与证据

工作树 `C:/Users/13984/.codex/worktrees/ble-first-link/wristflow`，分支 `codex/ble-first-link`，基础 `e5cdd7b4cef531e6bb59313f66eb0d2f9489c91a`。复用原项目已注册 SDK 环境，SDK junction 指向相同锁定源码，未改 SDK。Python 3.13.15、SCons 4.10.1、Arm GCC 14.2.1。

```text
cmake -S tests -B artifacts/host-ble -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe
cmake --build artifacts/host-ble --parallel 6
ctest --test-dir artifacts/host-ble --output-on-failure
pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product
```

前三项退出 0，13/13 通过，包括所有双分片位置、逐字节分片、中文与代理对、更新/撤回/本地删除、时间边界、畸形/超限/丢包恢复、十条淘汰及既有 UI 回归。日志 `artifacts/ble-validation/host-ble.txt`。本机辅助 PowerShell 只加载已安装环境并调用本工作树公共 `scripts/Build.ps1`；普通检出直接使用公共入口，均不烧录。

首轮 Product 构建因 GATT 属性表缺 `SERIAL_UUID_16` 宏失败，记录 `artifacts/product/20260927-133727-242/result.json`。补宏后重编译成功，记录 `artifacts/product/20260927-134512-469/result.json`；这是广播修正前的历史产物。下方最终修正版记录为 `artifacts/product/20260927-141319-544/result.json`，264 个工程源哈希与最终源码逐项一致。保留 SDK 既有 RWX、newlib/finsh 告警；源码检查与编译不能替代运行验证。

| 镜像 | 字节 | SHA-256 |
|---|---:|---|
| main.bin | 3560760 | `1dbb32dc0ecfb092c78649f2b77d13017ef3bfa759ab8cdfd18d33edd1e180af` |
| bootloader.bin | 58496 | `e59d70db44367d524fbb46a8a014ce0092462e86959d101a8e2e00ad27bb5e21` |
| ftab.bin | 11288 | `56b3681a2c150f755a689cc9acedc48420a583448b967abb2572615e0a78a200` |

镜像目录 `apps/product/project/build_sf32lb52-lchspi-ulp_hcpu/`；生成清单只列 bootloader `0x12010000`、main `0x12020000`、ftab `0x12000000`，无设置区或整片擦除。烧录前再次核对三个 BIN 哈希；CH340 COM5 的设备 ID 为 `USB\VID_1A86&PID_7523\6&A8355E6&3&1`。在上述目录执行：

```text
D:/MY_Desk/project/wristflow/.tools/sifli/tools/sftool/0.2.5/sftool.exe -p COM5 -c SF32LB52 -m nor -b 500000 --connect-attempts 3 --after soft_reset write_flash --verify bootloader/bootloader.bin@0x12010000 main.bin@0x12020000 ftab/ftab.bin@0x12000000
```

退出 0，原始烧录日志 `C:/Users/13984/.fastctx/jobs/j-53y083/output.log`。串口助手 `artifacts/ble-validation/serial_session.py` 以 1000000 baud、8N1、无流控打开 COM5，首段日志 `C:/Users/13984/.fastctx/jobs/j-8jmpmj/output.log`、原始字节 `artifacts/ble-validation/serial-a1.bin`。打开时错过上电广播日志；该段记录息屏、KEY1 唤醒及串口响应，初始堆已用 120504/329508 B、峰值 128220 B、BLE 线程栈峰值 13%，均只是尚未传通知的采样。

用户报告手机扫描不到后，先正常关闭首个串口会话，再用助手 `--reset` 在同一次新连接内发出 150ms RTS 复位并持续读取。第二段日志 `C:/Users/13984/.fastctx/jobs/j-sxztbw/output.log`；启动恢复原有三页布局和亮度 47，RTC 仍需手机重新校时，随后记录广播启动成功。没有修改或重复烧录固件。手机开启“发现未支持的设备”并重启应用用于诊断；不需要开启 Pebble 专属 LE 选项，也暂未更改重连策略。

手机复扫仍未发现。Windows 通过 Bleak 3.0.2 主动扫描 15 秒，在 22 个设备中捕获 `5C:FD:52:80:0F:D1`、`Bangle.js WristFlow`、NUS UUID，RSSI -52dBm；日志 `C:/Users/13984/.fastctx/jobs/j-a3l2c5/output.log`，仅打印目标设备详情。扫描依赖安装在忽略目录 `artifacts/ble-validation/scan-tools`，未修改 SDK Python 环境。SDK `bf0_sibles_advertising.c:158` 的弱函数默认返回 `GAPM_ADV_MODE_NON_DISC`，初始化在 :208 覆盖传入模式；项目改为实现官方扩展点 `sibles_advertising_disc_mode_get()` 返回 `GAPM_ADV_MODE_GEN_DISC`，并补地址日志。

上述修正版构建记录 `artifacts/product/20260927-141319-544/result.json`，退出 0、264 个源哈希一致、SDK 无改动；上表为修正版镜像。使用同一端口/地址/命令重新写入与 verify 退出 0，日志 `C:/Users/13984/.fastctx/jobs/j-qvpsuf/output.log`。新串口会话 `C:/Users/13984/.fastctx/jobs/j-jf0x8c/output.log` 捕获受控复位和通用可发现广播启动。解析器与主机测试源码未改，13/13 结果仍适用其对应模块；GATT 适配层修正用目标重新编译和真机广播核验。

手机端随后出现“陪伴设备”关联，选择后报 `discovery_timeout`，选择否也未完成添加；取消系统中本设备绑定后，选择“不配对”返回空主界面。官方 0.94.0 `BondingUtil.java:485` 的 companion 回调直接显示该超时错误，这个报错本身不能证明 NUS 服务发现失败。板端手机连接仍停在 subscribed=0。

用户临时关闭手机蓝牙后，电脑用 Bleak 独立连接并强制不使用服务缓存：实际枚举 NUS 服务、002 写入特征、003 通知特征及 2902 CCCD，MTU=527；订阅成功收到版本 JSON，再写入 `is_gps_active` 收到 `gps_power/status=false`。日志 `C:/Users/13984/.fastctx/jobs/j-80dppy/output.log`，命令为 SDK Python 执行本地 `artifacts/ble-validation/gatt_probe.py`，退出 0 并断连。这证明板端 GATT 双向通信可用，**不是 Gadgetbridge 初始化、真实中文通知或手机校时通过**。已提示手机恢复蓝牙并强行停止/重启 Gadgetbridge 后再试。

随后手机按上述步骤完成添加，用户截图显示已连接；同一串口连续记录 generation=7、MTU=131、`RTC synchronized: UTC=1790490758`、`phone time ... result=0`，诊断 subscribed=1、rejected=0、dropped=0。generation=5 的连接属于电脑独立检查，不能计作手机重连次数。此时消息数仍为零，后续要单独验证通知权限、中文内容及增删改。

用户随后提供微信通知栏“中文测试123”的截图，但板端仍 messages=0/rejected=0/dropped=0。Gadgetbridge `notifications_preferences.xml:18` 和 `NotificationListener.java:1338` 表明默认不转发亮屏时的一般通知；应用内路径为“设置→通知和来电→通知使用权”，联调可开启“即使屏幕开启时也通知”，或在手机锁屏后产生新通知。设备特殊设置截图已确认“发送通知”开启、“作为位图的文本”关闭、“允许高 MTU”开启。手机能显示 Gadgetbridge 自身通知不代表它已获得读取其他应用通知的使用权。

用户确认通知使用权允许、开启亮屏转发，并在后续确认手机勿扰已关闭。板端收到六条调试 NotificationSpec（正文“通知链路456”）和真实 QQ 通知 `1790491687`（正文 `QQ:中文测试456`）。后续 `1790491688` 正文为 `QQ:中文测试789`，UTF-8 原始串口字节与用户截图一致。对前者执行 `wf_ble remove 1790491687` 后，用户确认手机通知仍在；随后手机发出这两个 ID 的 `notify-`，后者从板端移除，已本地删除的前者重复移除无副作用。QQ 这两条使用不同 ID，不能拿它们证明同 ID 更新去重。未采集 Android 过滤日志，不能把早期未转发唯一归因于勿扰；权限、亮屏选项和勿扰均在实验中变化。

常规日志不显示正文；`wf_ble list` 仅在本次受控测试时主动使用。原始 SDK 串口还含绑定调试信息，仅保留本机忽略目录；公开证据不包含绑定密钥、联系人或完整通知截图。

## 重连与收尾结果

手机关闭/开启蓝牙后，用户手动连接共六轮。板端 generation 为 9、11、13、15、17、19；对应 UTC 校时秒为 1790492285、1790492296、1790492311、1790492323、1790492334、1790492346，每轮 MTU=131、校时 result=0。第六轮后收到 ID `1790491689`、正文 `QQ:中文重连终测`，原始 UTF-8 字节确认完整。generation=19 的状态为 connected=1、subscribed=1、rejected=0、unknown=0、dropped=0。这些不能称为六次自动重连。

设备级“自动重新连接到设备”此前已开启但未解决手机蓝牙开关后的自动恢复。官方 0.94.0 的 `BluetoothStateChangeReceiver.java:57–79` 在蓝牙关闭时主动断开，开启时另检查全局 `general_autoconnectonbluetooth`（默认关闭）。用户在主界面菜单的全局设置开启“当蓝牙打开时连接到 Gadgetbridge 设备”后，再关闭/开启一次蓝牙、不点击卡片，确认自动恢复。板端 generation=21、MTU=131、UTC=1790492657、校时 result=0，最终状态 connected=1、subscribed=1、messages=4、added=14、updated=0、removed=7、rejected=0、unknown=0、dropped=0；总计数含调试消息和后续通知，不等于独立测试案例数量。

用户确认 KEY1 唤醒后界面正常。本轮没有逐条通知与屏幕状态的时间对齐测量，也未完成 30–60 分钟联合压力验收。BLE 线程 8192 B 栈峰值的一次采样为 40%；当时系统堆已用 120780/329508 B、峰值 158976 B，只记作该次样本。PM 未启用，无休眠状态、电流或续航结论。

15:11 通过 stop 文件正常结束串口助手，日志确认 `CLOSED COM5`，后台作业退出 0；未重新打开串口或复位板卡。公开摘录仅保留连接/计数、测试正文和用户确认，隐藏联系人标题并排除 SDK 绑定材料。

收尾只修改文档与精简证据，固件源码仍为 `df97360b43e45b66b492786315bded1c7e6d5694`。264 个构建源哈希、12 个增量源码/测试快照及三个镜像哈希重新核对一致；SDK 无修改。变更文件 UTF-8、JSON、相对链接及 `git diff --check` 通过，本机检查记录为 `artifacts/ble-validation/closeout-check.json`。本轮未再编译相同源码，也未运行云端完整基线；PR #25 保持 draft，未合并。

## 下一次有界实验与预算

首轮连接与真实通知链路已有工作结果，继续沿用 A → 尽早 P → 完整通知 UI B 的顺序。下一实验在同一 Product 中先核对官方黄山派 PM、BLE 与显示/触摸电源流程，设置可回退配置；分别采集空闲、熄屏、BLE 事件到达与 KEY1 唤醒的实际状态，再决定启用方式。取得实际状态证据前，不声称低功耗已通过；完整通知 UI 和振动不在 A1 中追加。

剩余 BLE 样本为手机同 ID 更新去重、远离返回和长期后台恢复；后续按对应场景采样，不重复本次已完成的安装/添加访谈。安全绑定与隐私验证仍属产品化未完成项。

Gadgetbridge 总投入上限仍是累计 8 小时，包括此前协议研究。历史用时未精确登记，不能按零计算。本次会话约北京时间 12:57 起，至 15:11 停止硬件采集约 2 小时 14 分钟，包含采购；本轮暂按 2.5 小时预算记账，包含文档收尾余量。历史另预留 2 小时（预算预留，不是已测量历史工时），目前按 4.5 小时占用、3.5 小时剩余管理。后续若核实历史超出预留或本轮超出记账，应继续扣减，不能重置；遇到完整脚本/位图兼容需求先停在有界结论。
