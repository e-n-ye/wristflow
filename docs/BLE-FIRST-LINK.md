# Product 首个 BLE 实验 A1

日期：2026-09-27。沿用用户已确认的 [下一阶段计划 PR #24](https://github.com/e-n-ye/wristflow/pull/24)：先在同一 Product 上完成手机连接、校时及真实通知链路，再尽早做 PM/事件唤醒，随后接完整通知 UI。手机为用户确认的 iQOO Neo9S Pro+，截图型号代码 V2403A、软件版本 `PD2403B_A_14.0.12.2.W10.V000L1`；用户随后确认 Gadgetbridge 已安装。黄山派用 USB，未接新电池。

## 当前结果与范围

源码已接入标准 Nordic UART Service，广播名 `Bangle.js WristFlow`。Product 的 UI、RTC、设置持久化与 BLE 共存；PM 仍关闭。本轮先通过串口诊断验证数据，不显示通知浮窗或息屏预览，也未实现勿扰、通知亮屏和振动。

主机 13/13 测试通过，Product 官方 SCons 编译与产物校验通过。手机首次扫描未发现；Windows 主动扫描实际收到正确名称和 NUS UUID，随后发现 SDK 默认不可发现模式，改为通用可发现并重新编译、烧录校验。14:16 修正版日志确认地址 `5C:FD:52:80:0F:D1`、`discoverable=general`、广播 status=0；用户 14:18 截图确认正确发现并显示已绑定，板端收到连接事件。随后诊断为 connected=1、subscribed=0，**GATT 订阅、手机校时与通知仍待验证**。历史 UI 真机结果不自动覆盖本固件；精简证据见 [ble-a1.json](evidence/2026-09-27/ble-a1.json)。

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

首轮 Product 构建因 GATT 属性表缺 `SERIAL_UUID_16` 宏失败，记录 `artifacts/product/20260927-133727-242/result.json`。补宏后重编译成功，记录 `artifacts/product/20260927-134512-469/result.json`，264 个工程源哈希与当前源码逐项一致。保留 SDK 既有 RWX、newlib/finsh 告警；源码检查与编译不能替代运行验证。

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

## 下一次有界联调

1. 核对 CH340 端口，烧录并 verify，确认无复位循环、广播启动，保留单次串口连接，避免重新开串口引起复位和 RTC 丢时。
2. 手机安装官方 0.94.0，允许所需蓝牙/附近设备权限，从 Gadgetbridge 扫描并添加 `Bangle.js WristFlow`。开启通知访问，关闭 Text as Bitmaps；先确认 connected/subscribed、MTU 和 RTC 写入读回结果。
3. 用不含私人信息的实际 Android 中文通知，核对来源、标题、正文；同 ID 更新不重复，手机撤回同步移除，本地删除不清手机通知。
4. 重连五次、每次记录重新订阅与校时；普通息屏期间继续收消息，KEY1 唤醒后 UI 保持可操作。这里息屏不等于 PM 或低功耗通过。
5. 留下成功/失败的实际时间、计数和原因；先解决链路阻断，再进入通知 UI 或 PM，不以安装、编译或模拟数据代替首轮手机验收。

Gadgetbridge 总投入上限仍是累计 8 小时，包括此前协议研究。历史用时未精确登记，不能按零计算；本次会话从北京时间约 12:57 起，包含采购与 BLE，暂用全会话墙钟时间作为本次投入上界。历史另预留 2 小时预算（这是预算预留，不是声称已测量历史工时）；继续联调前更新本次累计时间，遇到需扩展完整脚本/位图协议的阻断先停在有界结论。
