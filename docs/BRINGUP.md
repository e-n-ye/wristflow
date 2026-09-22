# 上板基础固件实验

更新：2026-09-22。用户确认 Nano 退货完成、黄山派已下单。本实验的默认目标已改为 `sf32lb52-lchspi-ulp`，在固定 SDK 下实际编译通过；应用 C 源码不变，项目 Kconfig 补齐官方板级内存映射选项。实际屏幕版本、排线、电源、触摸映射和按键丝印仍须按到货清单确认。下方 2026-09-21 的 Nano 哈希保留为历史记录。

## 范围与入口

源码在 `apps/bringup/src/main.c`，配置在 `apps/bringup/project/proj.conf`。SDK 源码和板级内存分区不改。这个应用用于到货后的显示、触摸和休眠路径验收，不是产品表盘，也没有接入 BLE、RTC 应用、传感器、振动或持久化。

VS Code：`F1` → `任务: 运行任务` → `WristFlow: Build Bringup`。`Ctrl+Shift+B` 仍编译 Hello，避免改变已有快捷键含义。编辑本项目的 `apps/bringup/` 后可以直接重编译，不会触发 SDK 源码完整性保护。

命令行等价入口（项目根目录，PowerShell 7）：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example bringup -Jobs 4
```

最终调用为官方 `scons --board=sf32lb52-lchspi-ulp -j4`，工作目录为 `apps/bringup/project`。版本和离线环境复用方式与 [构建基线](BUILD.md) 一致。任务不执行烧录。

## 预期行为，待真机验证

| 场景 | 预期结果 |
|---|---|
| 启动 | 390×450 全屏边框，红/绿/蓝/白色块，亮度滑块，触点坐标与按键计数 |
| 滑块 | 测试范围 10%～100%，默认 60%；暂不保存到 Flash |
| 亮屏时触摸或按 KEY1 | 刷新空闲计时，暗屏恢复用户亮度；KEY1 计数加一 |
| 无操作 10s | 下发 10% 亮度，日志 `DIM` |
| 无操作 15s 或点击电源图标 | 停止 LVGL timers，等待显示不忙，再下发触摸/显示 POWEROFF |
| 屏灭 | 应用释放自己持有的一次 IDLE 请求，阻塞等待 KEY1；不以 10ms 循环轮询 |
| 屏灭后按 KEY1 | 重新申请 IDLE，恢复显示/触摸、用户亮度和 LVGL timers，全屏重绘 |
| 显示持续忙超过 1s | 取消本次熄屏，恢复 timers 和空闲计时，输出 `OFF aborted` |

这是测试参数，不是最终产品设置。触摸在屏灭期间关闭，因此本版不能触摸唤醒。按键只唤醒/延长亮屏，不实现关机或复杂长按行为。界面英文用于先验证字形与像素布局，后续产品本地化另做。

## 代码依据与取舍

- 工程骨架沿用官方 Hello 的 SCons 流程，使用 `PrepareEnv`、`AddBootLoader`、`PrepareBuilding`、`AddFTAB`，保留 board 自带分区。
- 显示和输入通过官方 `littlevgl2rtt_init("lcd")` 与 LVGL v9；帧缓冲交给 SDK 管理，不复用 PM 示例中尺寸与全屏写入范围不一致的自建缓冲。
- `example/pm/gui_pm` 的源文件分组依赖六轴传感器配置，其项目还带 LSM6DSL/CWM。当前应用不复制这些依赖，使用官方 AMOLED 示例中直接控制设备和申请/释放 IDLE 的方式，不启用 `GUI_APP_PM` 框架。
- 黄山派与历史 Nano 的 `hcpu/board.conf` 均指定 KEY1=PA34、高电平有效。应用使用官方 button 库消抖，通过 `HAL_HPAON_QueryWakeupPin` 查询索引，再启用双边沿 AON 唤醒，不硬编码唤醒索引。
- 按键回调只发送 RT-Thread 事件。LVGL 操作与应用的 PM 请求都在主线程，避免从回调操作 GUI；关屏期间到来的事件保留在事件位中，等待时可以立即收到。
- `drv_touch.c` 的 `RTGRAPHIC_CTRL_POWEROFF` 会启动临时线程，调用当前驱动 `deinit`；FT6146 的 `deinit` 停止并删除其 100ms 检查定时器。POWERON 再走 init。SDK 操作是异步的，返回成功只代表请求提交，实际完成时间和反复开关的稳定性待硬件验证。
- 黄山派 `BSP_TP_PowerDown` 拉低 TP reset 并重配触控引脚，不能据此声称触摸电源轨完全断电。`BSP_LCD_PowerDown` 操作 reset/VADD、3.3V/音频和充电 I2C 引脚，PowerUp 会开启音频 PA；共享电源的影响必须实测，不能直接继承 Nano 的功耗假设。
- 熄屏释放应用的 IDLE 请求只允许 SDK 在其他条件满足时选择更深睡眠，不证明已经进入 Deep Sleep。日志 `OFF requested` 也不是休眠或功耗通过的证据。
- 本版没有 BLE，采用官方 PM 示例的 `HAL_LPAON_Sleep()`；后续加 BLE 时必须重新核对双核电源与唤醒流程，不能直接保留此假设。

## 构建核验

黄山派已实际完成应用编译、链接和产物检查，退出码 0。生成配置已核对为 `BSP_USING_PM=y`、`RT_USING_PM=y`、`PM_DEEP_ENABLE=y`、`LVGL_V9=y`、CO5300/FT6146 和 390×450；`BLUETOOTH`、`GUI_APP_PM`、`TOUCH_WAKEUP_SUPPORT` 均关闭，板级 AW32001 充电驱动开启。源码已出现在生成的编译数据库中。当前产物目录为：

```text
apps/bringup/project/build_sf32lb52-lchspi-ulp_hcpu/
```

该目录包含 `main.elf`、`main.bin`、`ftab/ftab.bin`、bootloader、`.config`、`rtconfig.h` 和 `compile_commands.json`。每次构建证据在 `artifacts/bringup/<时间>/`，`result.json` 记录应用源文件 SHA-256、实际固件路径及产物校验结果。

黄山派成功记录：`artifacts/bringup/20260922-130142-290/`，`main.bin` 为 614176 B，SHA-256 为 `deadadab94b802be43bbd3eeab9580de1828c28fafa679c9254b9334984895fb`。此前缺少 `CUSTOM_MEM_MAP` 的链接失败及修复见 [构建说明](BUILD.md)。

Nano 历史记录：`artifacts/bringup/20260921-220706-642/`，`main.bin` 为 610096 B，SHA-256 为 `8a26366aaef4b8a0c3f1a7bddd2f4c844177cabefb400a052fbb7056a1094371`；归档 `docs/evidence/2026-09-21/bringup-*` 保留，不用于黄山派。

应用代码编译无警告。SDK 的 `external/lvgl_v9/src/core/lv_obj_tree.c:274` 有潜在空指针解引用警告，main/bootloader 保留已有 RWX LOAD segment 链接警告；没有抑制或修改这些 SDK 代码，不能据此认为运行风险已排除。没有执行板端测试或模拟器运行，画面、消抖响应、异步驱动恢复和实际 Deep Sleep 都待实测。

初次集成已遇到并处理：项目必须提供 `Kconfig.proj`；蓝牙选项名为 `CONFIG_BLUETOOTH`；SDK `DefineGroup` 的局部编译选项使用字符串；LVGL 的 `app_mem.c` 需要 `CONFIG_RT_USING_MEMHEAP=y`。失败日志保留。

## 到货后执行

先完成 [通电前核对](ACCEPTANCE.md#通电前)，确认实物、电压、排线及下载接口。当前文档不指定端口、不执行生成的下载脚本。

1. 先运行 Hello，确认启动、串口和复位，再准备 Bringup 的烧录步骤。每次记录实际固件哈希、板/屏版本、供电和串口日志。
2. 确认四边完整、色块正确。移动触点到四角和边缘，检查 X/Y 坐标范围、方向及滑块端点，观察文字与控件是否完整。
3. 将亮度设为 30%，无操作约 10s 应变暗，15s 应熄屏；KEY1 唤醒后应恢复 30%。复位应回到临时默认值 60%，因为尚未实现持久化。
4. 在第 9.9s、第 14.9s 附近触摸或按键，检查计时重置；在熄屏转换瞬间按键，确认不会丢失唤醒。快速点击熄屏并按键恢复、短按、长按和松开分别记录。
5. 连续执行至少 50 次熄屏/唤醒，每次检查触摸、亮度、全屏重绘和 KEY1 计数。日志应有 touch `Power off done` / `Power up done`；没有这些日志或出现断言时先定位驱动，不判定功耗成功。
6. 屏灭时单独触摸应不唤醒，这是本实验的配置；按 KEY1 应唤醒。触摸唤醒留待后续功耗取舍实验。
7. 固定供电、画面和亮度，先比较亮屏、变暗、屏灭三个稳定电流，记录表的量程与压降。另核对 PM 状态和触摸定时器，不把日志或万用表读数单独等同于七天续航证据。

这次保留 UART 日志和板载外设默认配置，未做整机静态电流优化；调试器、指示灯、稳压器和异步驱动都可能影响测量。BLE 连接负载未包含，不能用于完整日常负载的续航推算。
