# Product 通知 UI B1

2026-09-27。在同一 Product 的 BLE A1 与 PM P1 上继续实现已确认的完整通知 UI；采购已由用户全部下单，待到货。本增量不重置三个月日历、预算或 Gadgetbridge 累计实验上限。

## 行为

- 内存最多十条消息，最新在前；同 ID 更新并移到前面，手机撤回后同步移除。列表打开期间更新不会重建正在触摸的行；按下与松开时 ID 不同则取消点击。
- 表盘下滑进入通知中心，点击卡片查看多行详情；列表和详情可竖直滚动。按用户 2026-09-27 Redmi Watch 4 参考图修订：列表内左滑卡片露出右侧垃圾桶，点击后原位删除；详情页只阅读正文。右滑或点移开的正文收起操作区，同时最多展开一条；消息重排后关闭受影响的操作区，按下/松开 ID 不符不删除。列表底部可清空全部。手表删除仅改本地存储，不向手机发送 DISMISS，断连不清空现存列表，不承诺离线补送。
- 亮屏收到消息显示三秒顶部横幅，点击后进入中心。消息临近普通熄屏或处于变暗态时仍获得完整显示时长。
- 息屏且非勿扰时收到消息，先恢复屏幕并等待首帧完成，再显示五秒独立预览。点击预览进入该条详情；上滑或 KEY1 收起并保持亮屏；无操作超时再次关屏。触摸预览会续五秒。
- 自动提醒不导航、不确认退出、不丢组件草稿、不停止前台秒表。明确点击进入通知时复用原退出确认，取消仍留原页面。预览超时保留原息屏时间，普通页面长睡回表盘规则不被重置；草稿和有数据秒表继续按原豁免规则处理。
- 勿扰仍入库，屏幕不亮、没有横幅或预览；控制中心和设置共用真实状态。设置记录 v4 保存勿扰，兼容读取 v1/v2/v3，旧记录默认关闭。连接状态由 BLE 已连接且已订阅共同决定。
- 没有电机，未实现或验证振动；当前 USB 供电，不伪造电池百分比。图标、动画细节和平台通知兼容范围以实际实现为准。

## PM 与资源边界

B1 启动后允许自动休眠：初始化、亮屏和首帧恢复期间持有应用 idle 请求，屏幕关闭后释放；仍可用 `wf_pm hold` 禁止、`wf_pm allow` 恢复。P1 的最后 hold 是回退实验状态，不是消息亮屏的技术前提。BLE 消息可以先作为后台事件处理，再由非勿扰通知决定是否恢复屏幕。

这不证明整机低电流或续航。已知保存线程仍有 100ms 轮询；PM 计数仅反映 SDK 路径，真实驻留与各电源轨电流另测。无 UART AON 唤醒时，深睡期间串口命令可能无响应，先 KEY1 唤醒再诊断。

消息快照在 BLE 互斥锁下复制，LVGL 仅在 UI 主线程访问。快照放静态区，避免约 20KB 结构压入 8KB main 栈。通知列表与详情在 shell 生命周期内各保留一个实例。

通知字体与静态 UI 字库独立，来自锁定 SDK 的 Noto Sans SC，遵守 OFL 并改名 WristFlow Messages。官方 Editor 转成 22px、2bpp 内嵌字体，包含 29,598 个码点，其中基本汉字 20,976、扩展 A 6,582；不保证 emoji 或 BMP 外汉字扩展。排除两个双行竖排重复符号后实际行高 31px，单行标签至少 34px。Product、UI Demo、主机均启用官方 `LV_FONT_FMT_TXT_LARGE`，不手改生成字体。许可与详细范围见 [字体说明](../ui/xml/fonts/README.md)。

## 构建与验证进度

工作树 `C:/Users/13984/.codex/worktrees/ble-first-link/wristflow`，分支 `codex/notification-ui`，基于 P1 `abebc850e2a337c0f0496420fd3ea18402837ab8`；原目录用户改动保留。SDK/子模块未修改。

首轮编译因大字体超过 20bit bitmap 索引失败；启用 `LV_FONT_FMT_TXT_LARGE` 修复。字体裁切实验发现 U+3031/U+3032 将行高抬到 43px，移除这两个非目标字符后降为 31px。主机手势样本初版每点重复两次采样使速度衰减、无法触发下滑；改用与既有指针测试相同的 16ms 采样。新增字体检查发现源字体没有 U+FFFD，覆盖测试按实际支持的中文/标点/方框执行，不宣称缺失字符已可用。

本机验证命令：

```text
cmake --build artifacts/host-pm --parallel 6
ctest --test-dir artifacts/host-pm --output-on-failure
pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product
pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example ui_demo
```

18:25/18:29 行为源码对应的 15/15 主机测试、Product/UI Demo 构建及产物校验通过，286/276 个工程源哈希匹配。最后仅修正确认图标颜色后，官方 Editor 完整导出成功，`ctest --test-dir artifacts/host-pm -R 'notifications_ui|settings_ui|component_editor' --output-on-failure` 的 3/3 相关测试通过；主机构建及两目标增量编译均退出 0。最终 Product 记录为 `artifacts/product/20260927-185359-910/result.json`，主 BIN 7,427,492 B（SHA-256 `a82054629fdd649d18ae953d04ee660b8c91dbda87da159bb37b22d025b4cc14`）；UI Demo 记录为 `artifacts/ui_demo/20260927-185521-800/result.json`，主 BIN 7,236,968 B（`ee8ef81085bff8a6d4fb23f7a44979e694f0253d4e15fc2df8578f617f330724`）。两目标最终 286/276 个源文件、29/22 个产物的哈希与大小均匹配。

精简本机记录在本工作树忽略目录 `artifacts/notification-validation/final-build-verification.json`，日志为 `host-test-release.log`、`product-build-release.log`、`demo-build-release.log`、`editor-final.txt`，实际 LVGL 截图为同目录 `notify_*.png`。SDK/LVGL/newlib/链接器和已有类型范围警告未抑制。主机测试覆盖三秒横幅、五秒预览、同 ID 更新、撤回、本地删除/清空、勿扰、秒表与草稿确认/取消、十条滚动、长标题/正文、长睡计时、上滑收起及设置返回路径。官方 Editor 2.0.1 Community 完整重建明确报告 `Project compiled successfully`，运行 LVGL 9.4.0；最终草稿确认截图已核对白色按钮图标。

## 真机有界实验与剩余验证

最终官方导出、源码及镜像核对已完成。2026-09-27 19:08 在 COM5（CH340、VID:PID=1A86:7523、LOCATION=1-1.1）独立写入并 verify，sftool 退出 0。在 `apps/product/project/build_sf32lb52-lchspi-ulp_hcpu` 执行：

```text
D:/MY_Desk/project/wristflow/.tools/sifli/tools/sftool/0.2.5/sftool.exe -p COM5 -c SF32LB52 -m nor -b 500000 --connect-attempts 3 --after soft_reset write_flash --verify bootloader/bootloader.bin@0x12010000 main.bin@0x12020000 ftab/ftab.bin@0x12000000
```

三镜像与官方 `sftool_param.json` 一致，未写 settings 分区。第一次启动命令因日志重定向目录写错在 shell 层退出 1，未运行 sftool；改用工作树内绝对日志路径后烧录成功，日志留于 `artifacts/notification-validation/flash-notify-b1.log`。随后单一串口助手 `artifacts/ble-validation/serial_pm.py notify-b1` 受控 RTS 复位并采集；19:08:21 恢复布局 generation=26/pages=3、brightness=47、face=diffusion，19:08:22 报告 `Notify B1`，19:08:32 自动关屏。启动时 `allow=1`。原始串口留在本机，不提交私聊或 SDK 绑定材料。

首组真机结果：19:10:15 手机连接，MTU=131 并校时。19:10:30.778 收到通知并报告 `wake event=notification`，此前 PM enter/exit 均为 2027；19:10:31.081 屏幕恢复，19:10:35.825 再次关闭。用户确认中文预览自动显示、约五秒后熄屏。19:11:10 KEY1 唤醒时 PM enter/exit 均增至 2530，证明预览结束后继续走 SDK 休眠路径；计数不是精确硬件驻留或电流测量。用户随后确认通知中心/中文详情显示和触摸正常、详情删除后手表记录移除且手机 QQ 通知仍在；19:11:57 状态为 connected=1、subscribed=1、messages=0、added=1，rejected/unknown/dropped 均为零。[精简证据](evidence/2026-09-27/notification-b1.json) 持续更新。

用户进一步确认亮屏横幅正常、点击后进入通知中心。勿扰开启后，19:14:38 设置保存返回 0，19:15:00 息屏，19:15:04 新通知入库，没有消息亮屏事件，直到 19:15:09 KEY1 才唤醒；用户确认保持黑屏且通知中心有新消息。上述硬件结果对应主 BIN `a8205462…4cc14`。

随后用户要求按 Redmi Watch 4 改为列表内左滑露出垃圾桶，不沿用初版的删除交互验收。官方 GUI 于 19:20 导出、19:21 完整预览构建完成；主机新增左滑、右滑收回、原位删除、纵向滚动不误删、仅一条展开，以及按住删除时收到新消息不误删的指针测试通过。首轮发现 START 吸附将垃圾桶对齐列表左侧，改为 END 后确认按钮停在右侧；两轮测试日志保留于 `artifacts/notification-validation/swipe-host-test*.log`，实际截图为 `notify_swipe_delete.png` 与 `notify_after_delete.png`。

左滑修正版 Product/UI Demo 编译均退出 0，记录分别为 `artifacts/product/20260927-192846-133/result.json` 与 `artifacts/ui_demo/20260927-192941-334/result.json`；286/276 个源文件及 29/22 个产物哈希、大小逐项匹配，SDK 与子模块符合锁且干净。Product 主 BIN 7,427,628 B、SHA-256 `4467ed89773aecb7584e660a20a3b310246c26df1925b9874e4cebe99611ca75`；UI Demo 主 BIN 7,237,096 B、`e5a042c2837c477b0b2f5eb9fb43dd0496ae569f4bd27562f90439f7c891b331`。[修正版精简证据](evidence/2026-09-27/notification-swipe.json) 与初版硬件证据分开。

左滑修正版三镜像使用上述独立命令写入，verify 退出 0，日志 `artifacts/notification-validation/flash-notify-swipe.log`。旧串口助手已正常关闭，随后 `serial_pm.py notify-swipe` 单独采集；19:39:32 恢复相同的布局、亮度、表盘，19:39:34 启动 B1，19:39:44 自动息屏。用户确认左滑露出/右滑收回垃圾桶和原位删除顺手、另一条消息与手机通知仍在、勿扰跨烧录/重启保留。19:40:42 手机连接、MTU=131 并校时；19:40:50 息屏后收到两条通知均未亮屏，19:41:05 KEY1 才恢复。关闭勿扰后，19:42:58 保存成功，19:43:13.566 通知唤醒、19:43:13.875 亮屏、19:43:18.641 再熄屏；19:43:21.024 KEY1 唤醒时 PM enter/exit 由 1503 增至 1537，用户确认预览、再熄屏、KEY1 与触摸正常。最终保持息屏自动 PM、勿扰关闭，不保留 hold。初版串口末尾出现一次未支持的协议事件 `event=5`，保留未覆盖的 Gadgetbridge 消息类型边界，不能宣称全协议兼容。

30–60 分钟运行、十轮导航、十轮熄屏唤醒与五次断开/连接继续作为后置联合验收线；P1 五轮不能替代新固件联合验收。尚未取得 B1 完整交互证据，不宣称通知 UI、休眠电流或续航全部通过。

提交检查：变更的中文文件均为有效 UTF-8、八个 XML 格式良好、文档相对链接存在。暂存后 `git diff --cached --check` 仅报告官方新增的五个 `*_gen.c` 末尾空行；保留原始导出内容，非生成文件按默认规则检查通过，生成文件使用单次 `git -c core.whitespace=-blank-at-eof diff --cached --check` 检查通过，未修改仓库配置或 SDK。此格式例外不作为新的功能测试结果。
