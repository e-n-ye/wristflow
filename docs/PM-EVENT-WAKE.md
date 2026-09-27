# Product PM／事件唤醒实验 P1

2026-09-27。用户确认第二批采购全部下单，并继续昨天的 A → P → B 顺序。A1 已取得真实手机链路；本轮只在同一 Product 核验 PM 和事件恢复，完整通知 UI 随后接入。沿用 PR #24 中 P 首轮最多 4h 的有界定位范围，USB 供电，未接电池，无电流或续航结论。

## 实现与可回退边界

- 工作树复用 `C:/Users/13984/.codex/worktrees/ble-first-link/wristflow`，新分支 `codex/product-pm-wake` 基于 BLE 记录 `8df22c3`；原目录用户改动保留。SDK 及子模块仍锁定，源码零修改。
- Product 开启 `BSP_USING_PM` 与 `PM_DEEP_ENABLE`，保留 BLE；不开启 Standby、触摸唤醒、GUI_APP_PM 或高频 PM_DEBUG 输出。锁定 SDK 的 HCPU Deep 策略门限为 30ms。
- 新增 `product_pm.c/.h` 统一 KEY1、手机事件及一次性采样事件。启动默认禁止深睡；亮屏和恢复首帧期间持有应用的 `PM_SLEEP_MODE_IDLE` 请求。`wf_pm allow` 只在屏幕已关闭时放开该请求，`wf_pm hold` 可撤回，重启再次默认禁止。其他 SDK 驱动自己的请求保持原样。
- KEY1 配置 PA34 对应的 AON 双边沿唤醒。手机事件由 BLE worker 完成解析，再通知主线程；息屏主线程处理后台事件后继续等待，不因此亮屏或导航。通知 UI 的预览／浮窗／勿扰将在 B 增量接入，不在 P1 中伪造。
- LCD 关闭前等待 busy 清除，关闭 API 为同步；触摸关闭为异步，但驱动持有自身 idle 请求并在 deinit 时删除 FT6146 100ms timer。唤醒恢复显示前应用重新持锁，仍等待首帧完成才提亮。
- `wf_store` 无工作时每 100ms 醒一次，本轮先保留这项已知睡眠扰动条件，不同时改持久化协议。BLE worker 阻塞等待消息队列。单次测量不能代表连续长睡或长期稳定性。

```text
wf_pm status
wf_pm allow
wf_pm hold
wf_pm sample
wf_pm wake
wf_ble status
```

`allow` 自动安排 5 秒后的一次采样，`sample` 也只安排一次；没有常驻采样线程。`wake` 是诊断唤醒，必须与真实 KEY1 的证据分开。串口本身可能扰动运行，原始日志只留本机忽略目录。P1 初版放开 PM 后，串口 `sample/status` 命令曾未收到响应；未启用 UART AON 唤醒，不保证深睡时输入串口能被接收。诊断先按 KEY1 让界面亮屏、应用持锁，再执行命令；不要把串口不响应直接当作 BLE 失联或复位。

## 证据含义

`rt_pm_notify_set` 在关中断上下文中仅更新计数，线程上下文才打印。`enter/exit` 记录尝试和返回；`deep_elapsed/deep_ticks/max_ticks` 记录 SDK Deep 路径返回后的补偿 tick，可能包含驱动 suspend、WFI 前后准备与恢复耗时。待处理中断导致 WFI 立即返回也可能出现正 tick，因此这些值不等于精确硬件驻留时间或功耗。结合 SDK 路径、唤醒源和事件功能验证描述软件休眠路径，硬件功耗另测。

独立审查未发现确定的应用 idle 请求泄漏或同批 PHONE/KEY 丢键；不覆盖真实线程压力、驱动异步时序或 WFI 驻留。主机测试直接编译实际 PM 适配器，使用 RTOS/PM mock 核对重复 allow/hold、十轮开关屏请求配对、混合事件、tick 回绕及回调无 I/O。

## 构建与当前验证

主机配置、Ninja 构建和 CTest 退出 0，14/14 通过；记录 `artifacts/ble-validation/pm-host-result.json`，日志 `pm-host.txt`，产物 `artifacts/host-pm/`。Product 命令仍为 `pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product`，底层官方 `scons --board=sf32lb52-lchspi-ulp -j6`；构建不烧录。

首版 Product 构建记录 `artifacts/product/20260927-155232-915/result.json`：退出 0、产物校验通过，266 个工程源哈希一致。首版主 BIN 3571600 B，SHA-256 `ae000f347bfae6f5f43889be92b9e4700fc92207c51dd968599450bc77ffd7d0`。上板发现诊断行超出 SDK `RT_CONSOLEBUF_SIZE=128`，尾部截断；现拆成短行，并在主机测试用最大 uint32 计数验证每次打印长度小于 128 字节。

修正版主机 14/14 再次通过，日志 `artifacts/ble-validation/pm-host-final.txt`。最终构建 `artifacts/product/20260927-161624-537/result.json` 退出 0、校验通过、266 个源哈希一致。main.bin 3571648 B，SHA-256 `60d2ad2d050ff19c909af307bdd10406f6c5a4eb0172ba27870ca770e9dc72e4`；bootloader 58496 B，`e59d70db44367d524fbb46a8a014ce0092462e86959d101a8e2e00ad27bb5e21`；ftab 11288 B，`7aa8fe564a0adf3025c718f28319a64d7b1e933cc9e5a5365e7fe5fc5c22bec0`。版本沿用 Python 3.13.15、SCons 4.10.1、Arm GCC 14.2.1。SDK 既有告警未抑制。

## 初版硬件发现（日志修正前）

COM5 CH340 的三镜像烧录/verify 退出 0，作业 `j-uqql98`；地址仍为 ftab `0x12000000`、bootloader `0x12010000`、main `0x12020000`，未写设置区或整片擦除。串口作业 `j-hb2djo` 从 16:10:49 至 16:17:55，正常关闭退出 0；一次受控 RTS 复位 150ms，1Mbps 8N1。启动恢复原有三页布局、亮度 47，手机自动连接／订阅、MTU=131、校时 result=0。

用户确认默认禁止深睡时显示／触摸正常，并关闭手机蓝牙。16:13:31 未连接、屏幕关闭时 hold 状态 enter/exit=0；`allow` 后 own_idle/sdk_idle=0，5 秒采样 enter/exit=54、deep_elapsed=49、deep_ticks=4997、max_ticks=104。尾部 wake 字段被日志缓冲截断，不能补猜。16:15:39 KEY1 唤醒，先重新持锁、随后完成首帧并亮屏；用户确认显示／触摸正常，再恢复蓝牙。重新连接 MTU=131、校时 result=0。后续串口 sample/status 无响应，SDK Deep 路径内 UART 行为按上方限制处理。

这些未连接结果仅对应首版；修正版对应结果如下。日志修正未改变 PM 决策，但不把旧记录自动当作新固件全部验收。

## 修正版硬件结果

最终三镜像写入／verify 作业 `j-2bs3v5` 退出 0，地址与首版相同。串口作业 `j-y5nyta` 于 16:20:33 打开 COM5，受控 RTS 复位 150ms；16:20:50 手机自动连接、MTU=131、RTC 校时 UTC=1790497249/result=0，重新订阅成功。

- **已连接、息屏对照**：16:21:22 hold 状态 enter/exit=0、own_idle/sdk_idle=1、off=1。放开后 5 秒采样 enter/exit=67、deep_elapsed=48、deep_ticks=4956、max_ticks=118、wake=0x4，own_idle/sdk_idle=0，屏幕仍关闭。
- **息屏事件**：16:22:20 和 16:22:38 接收 QQ ID `1790491718/1790491719`；用户确认手机通知到达、板屏仍黑。主动 list 的原始字节均包含指定文本 `PM中文测试456`，公开只保存该测试标记，不保存标题或正文的其他部分。恢复前 phone_off/background 由 2 增至 4，中间没有 screen-on 日志。该计数还包括初始校时／查询，不能全算作通知条数。
- **KEY1 与界面**：用户明确确认五轮自然熄屏／KEY1 唤醒及触摸正常；串口实际记录七轮 allow=1 下的 KEY1 恢复，均有先 own_idle=1、后 screen-on 的顺序。恢复样本 wake=0x40002/0x40006，其中 PIN10 位为 `0x40000`，与板端 KEY1 AON index=10 对应；5 秒采样的 `0x4` 为 LPTIM1。位定义来自 SDK `drivers/cmsis/sf32lb52x/hpsys_aon.h:285–329`。不把这些样本扩大为长期压力验收。
- **撤回**：16:24:40 已执行 hold；用户随后又按 KEY1 配合确认重复 hold 无副作用。16:27:09 再次息屏、allow=0、own_idle/sdk_idle=1，enter/exit 仍为 1947、deep_elapsed 仍为 1261，与撤回时相同，证明运行时门锁阻止后续 Deep 尝试。最终 BLE connected/subscribed=1，messages=2、added=2、updated=0、removed=0、rejected=0、unknown=0、dropped=0。

最后保留运行时 hold 状态，手机连接不主动断开，串口助手通过 stop 文件正常退出并关闭 COM5。本轮通过的是 USB 下 SDK 休眠路径、息屏后台事件和 KEY1 恢复；没有证明精确硬件驻留、休眠电流、电池续航、触摸唤醒或 30–60 分钟联合压力。已知 100ms 保存线程轮询后续可按数据优化。

脱敏硬件摘录和源码／镜像清单见 [pm-p1-hardware.txt](evidence/2026-09-27/pm-p1-hardware.txt) 与 [pm-p1.json](evidence/2026-09-27/pm-p1.json)。下一增量按已确认契约接完整通知中心、详情、亮屏浮窗、息屏预览和勿扰；底层保留后台处理与点亮屏幕的区分，继续使用可回退 PM gate。P 首轮在 4h 时间盒内完成，未扩展传感器负载或无界调功耗。
