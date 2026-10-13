# Product 通知 UI B1

2026-09-27。在同一 Product 的 BLE A1 与 PM P1 上继续实现已确认的完整通知 UI；采购已由用户全部下单，待到货。本增量不重置三个月日历、预算或 Gadgetbridge 累计实验上限。

<a id="notification-conversations"></a>
## 2026-10-13 按人展示的数据核验（UI 尚未实施）

用户已确认同一个人先发 A、再发 B：列表显示最新 B，点开按时间查看这个人的已接收消息。因此 A/B 两条不同内容不是同一消息重复；历史应保留 A，不能用同 ID 覆盖丢弃。当前缓存仍是全局十条、按协议 ID 管理，详情只读单条；本轮只读采样和同步决定，没有修改固件／UI，也未新增手机应用工程。

手机关于页为 Gadgetbridge **0.94.0／2ee6b3002 (dirty)**。官方 [构建配置](https://codeberg.org/Freeyourgadget/Gadgetbridge/src/tag/0.94.0/app/build.gradle#L90) 以构建时 Git 状态产生 dirty，它不证明通知业务代码被修改；没有核验安装 APK 与 tag 的逐字节一致性。[NotificationListener](https://codeberg.org/Freeyourgadget/Gadgetbridge/src/tag/0.94.0/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/externalevents/NotificationListener.java#L462) 已保存 Android key 和通知 when，但 [BangleJS 发送路径](https://codeberg.org/Freeyourgadget/Gadgetbridge/src/tag/0.94.0/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/service/devices/banglejs/BangleJSDeviceSupport.java#L1500) 没有把 key／包名／时间转发给固件。源码解释与 10-12 不同 ID 的新增／清通知栏删除样本一致，不把官方源码等同于安装包证明。

用户授权 USB 调试只读核验。官方 Platform-Tools 本机隔离安装，ADB 1.0.41／37.0.1-15733141；手机 V2403A、Android 14／SDK 34。`cmd notification list` 只在内存按完整包名筛选 QQ，再以安全引用的 key 执行 `cmd notification get`；后者不脱敏，因此只保存白名单字段、匿名 key／groupKey、时间和受控标记布尔值，原 key、联系人、正文和设备序列号不提交。未安装 APK、改手机设置、重启或打开手表串口。首次未枚举到手机；用户开启 USB 调试后才取得有效样本。精简记录见 [证据 JSON](evidence/2026-10-13/notification-conversations.json)。

| 样本 | Android 观察 | 支持的结论 |
|---|---|---|
| Q3-A → Q3-B，同一联系人 | 两次只有一条 QQ 通知；ID 517／完整 key 匿名值相同，creation 不变，when／update 改变；复查正文仅含受控 B 标记 | 同一活动通知被更新；没有证明跨清除、重建或不同人的身份稳定性 |
| Q3-C 发送后撤回，同一联系人 | B 超时不能撤回，用户改用 C；原 ID 517 仍含 C，另有 ID 265 的撤回提示；还有未标注 ID 518 与 ID 2147483647。四条 key 不同却共用同一 groupKey | 撤回是另一条提示，未见原消息关联；不能把 Android groupKey 当成按人的分组键。未标注通知不强归到测试联系人 |
| 划掉这组 QQ 手机通知 | 四条活动记录归零，读取失败为 0 | 手机通知记录确已移除；本轮没有同步采手表／Gadgetbridge TX，不新增固件删除验收 |

撤回样本使用修订提取器，主 notification 与 publicNotification 分块，四条格式均识别；shortcut 均为 null／valid=false，主 extras 未见 `android.messages`、`android.messages.historic`、`android.people.list`、`android.conversationTitle` 或 `android.isGroupConversation`。这是该机型／QQ 场景的白名单字段观察，不证明所有私有 extras 都无关联，也不能推广到所有 QQ 版本。早期 A/B 提取器未分块、未解析 shortcut，A/B 的身份和时间对照仍有效，其“未见字段”不单独作为缺失证明。Android [NotificationRecord](https://android.googlesource.com/platform/frameworks/base/+/android-14.0.0_r1/services/core/java/com/android/server/notification/NotificationRecord.java#490) 的字段格式用于核验提取器；原通知不落盘。

**可行性与剩余边界**：固件目前收不到用于归组的 Android 通知 key，也收不到通知时间；按人历史需要分组身份与独立消息条目，保留每条 A/B，而非把整个会话共用一个现有消息 ID。当前 Android key 只证实本次通知生命周期，when 是通知时间，不保证等于 QQ 发送时间；没有逐条消息 ID，不能保证辨别所有重发或精确撤回。第二位测试联系人暂不可用，跨人隔离／群聊／清除重建仍未验证。

下一轮**方案候选，不是另一份活动队列**：先做限定的 Gadgetbridge 元数据扩展，保留现有通知及删除 ID，另传不含联系人原文的通知身份、来源与通知时间；先验证同人更新、不同人、清除重建和单独撤回提示，不直接变更全局 NotificationSpec ID。证据满足后才接固件分组与 XML 详情历史。分组删除、群聊、时间排序方向／初始滚动及十条容量分配尚未决定；不承诺完整 QQ 聊天记录或凭文案删除。是否进入这一轮以 [STATUS 当前区](STATUS.md#current)及用户新指令为准，Gadgetbridge 累计实验上限不重置。

<a id="notification-trace"></a>
## 2026-10-12 通知协议诊断（已闭环历史）

本段诊断经 [PR #51](https://github.com/e-n-ye/wristflow/pull/51) Rebase 合入 `a6255c18`，MERGED 与 fetch 已回读。后续用户明确按人展示目标，见[数据核验](#notification-conversations)；本段逐条缓存和诊断结果仍是当时实现证据，不把不同内容的 A/B 认定为同一消息重复。

用户恢复旧 J4 的重复卡片／撤回残留排查，确认原 iQOO Neo9S Pro+／Gadgetbridge／QQ 链路与 USB-only 条件。旧失败缺操作类型和 ID，不能追溯归因。本轮 `codex/notification-trace` 基于 `ca6ae012`，仅增加元数据观察，不改变按 ID 更新／删除的语义。新同版对照定位：A2、B2、聊天撤回提示是三个不同 ID 的新增 notify；手机通知栏移除才发 notify-，固件命中删除。没有复现同 ID 重复入库或收到删除后漏删，不能为聊天撤回文案猜测应删除哪条消息。

`wf_ble trace on` 开始新窗口，默认关闭；最近 32 条通知解析结果只存 RAM，超出时覆盖最旧项并报告 overwritten。`wf_ble trace dump` 输出操作（notify／notify~／notify-）、有效 ID、结果（add／update／remove／miss／invalid-id／invalid-fields）、缓存数量、RT tick、接收代次／包序号及 dropped／gaps／stale。invalid-id 的 id=0 是占位，valid=0；不代表真实协议 ID。未形成可解析通知对象的帧仍由 rejected／unknown 和队列计数诊断，不能仅凭 trace 中没有 remove 断言手机没有发送。回调不输出原始 JSON、来源、标题或正文。

`wf_ble ids` 只输出当前缓存顺序和 ID；新查询／trace 控制不发送 UI／PM 事件。`wf_ble status` 保留既有计数，`list` 会输出正文，不用于常规采集。关闭窗口用 `wf_ble trace off`，仍可 dump 保留记录；重启清空并关闭。短窗口内保持连接，不主动删手表缓存；记录 A、B、QQ 撤回提示和手机通知栏移除的时间，分别对照操作／ID／计数。手机通知栏移除与聊天消息撤回是两个不同触发，不能用提示文案猜测应删哪条消息。

官方 Gadgetbridge 0.94.0 tag 为 `2ee6b3002f250e24328ac91b948bce0b2243a0db`。[NotificationListener](https://codeberg.org/Freeyourgadget/Gadgetbridge/src/tag/0.94.0/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/externalevents/NotificationListener.java#L462) 新建 `NotificationSpec(-1, notification.when)` 并另存 Android key；[NotificationSpec](https://codeberg.org/Freeyourgadget/Gadgetbridge/src/tag/0.94.0/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/model/NotificationSpec.kt#L30) 负 requestedId 使用自增 ID。[BangleJSDeviceSupport](https://codeberg.org/Freeyourgadget/Gadgetbridge/src/tag/0.94.0/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/service/devices/banglejs/BangleJSDeviceSupport.java#L1500) 发送 notify／notify-，该路径没有发送 notify~。Android 通知移除还受设备 `autoremove_notifications` 设置控制。由此推断重复发布可能产生新 ID，但手机安装包来源、实际操作和本次 ID 尚需现场核对，不能由源码替代真机结论。

最终相关主机 `phone_protocol`／`phone_queue`／`notifications_ui` **3/3** 通过（0.91 秒），包含通知结果观察和原 NOTIFY／REMOVE／REJECT 事件数量断言。Product 官方 SCons 编译退出 0、383 份源码及 29 个产物重哈希匹配；主 BIN 7,712,444 B／`66d5c716…`，最终记录位于隔离工作树 `artifacts/product/20261012-193646-605/result.json`。首次编译期间修订 dump，已重新编译最终源码；dump 在锁内复制静态快照，解锁后输出，避免阻塞 BLE worker。只改 Product 与主机协议测试，UI Demo 不编入该协议／传输源，未新增其编译。锁定 SDK／子模块与工具沿用；有界手机对照见下方，完整联合矩阵继续保留。

同一 CH340 COM5 的三镜像 write_flash --verify 退出 0，记录 `artifacts/flash/product/20261012-193805-270/result.json`。UART 采集打开时 DTR／RTS=false，无自动复位；初次 status 显示未连接、缓存及全部计数为零，show_date／trace dump／ids 没有回执，不能算 RTC 或诊断开关查询成功。当时等待用户常亮与 Gadgetbridge 连接后重新确认，再开启窗口。命令、源码和镜像完整标识见 [精简证据](evidence/2026-10-12/notification-trace.json)。

后续用户确认常亮且连接：19:48:00 连接 generation=1、MTU=131，19:48:01 手机真实校时成功；19:48:26 show_date 与电脑当前 UTC 一致，19:48:27 status 连接／订阅均为 1、缓存和通知／队列计数全零。19:48:28 trace 默认关闭，19:48:51 开启、19:48:52 dump 为 enabled=1／total=0／overwritten=0，ids 查询亦成功。候选提交 `07302bc`。一次空 Value 被采集助手参数校验拒绝，未写入 UART，不是设备命令失败。

第一组被用户报告的“不弹消息窗口”打断；随后澄清为控制中心临时常亮、勿扰开启。源码准入先更新缓存，再按勿扰阻止弹窗；临时常亮本身不在抑制条件内。用户关闭勿扰后确认 C 横幅出现且中心有消息，与既有契约一致，不修固件。探索窗口共 49 条（20 add、0 update、5 remove 命中、24 miss），两个快照的并集覆盖 1–49，32 条环的覆盖计数正常。期间操作未逐条核实且缓存发生未由协议移除解释的下降，不能作受控 J4 结论；解析／队列计数均无错误。

19:55:29 重开窗口，基线缓存 0／累计 added=20、updated=0、removed=5。受控 A2 于 19:57:00 收到 `notify id=1791710043`，结果 add／count=1／generation=1／sequence=258；用户确认一张卡。B2 于 19:58:59 收到 `notify id=1791710044`，count=2；聊天撤回于 19:59:36 收到 `notify id=1791710045`，count=3。19:59:58 完整快照只有三条 add，没有 notify~／notify-；用户确认撤回前两卡、撤回后新增提示且 B2 仍可见，两张照片也显示 A2/B2 与撤回提示并存。照片含联系人显示，只保留在忽略的本机 `artifacts/notification-trace/photos/`，不提交正文、标题或原图。

20:00:44 另收到未标注来源／内容的新 notify `1791710046`，独立记录，不强行归入 A2/B2。用户随后只划掉手机通知栏这一组 QQ 通知，并确认三条测试卡都消失。20:01:54 收到旧 ID `1791710042` 的 remove-miss，以及 `1791710043`–`1791710046` 四个命中删除，count 4→0；20:02:47 累计 added=24／updated=0／removed=9，解析／队列计数全零、连接代次仍为 1。新窗口 9 条完整、无覆盖，明确证明这组有效 ID 的协议移除被执行；未知第四条和探索操作不升级成额外应用语义结论。

20:00:27–30 一组息屏查询没有回执，保留为失败采样；19:59:58 的已确认快照和后续唤醒快照用于判断。20:02:44 trace off 回执成功，20:02:48 只读 UTC 为 12:02:49、与当前真实时间一致；20:04:01 采集正常关闭且进程退出，COM5 已释放。本轮结论来自设备接收端元数据与用户观察，Gadgetbridge 源码提供解释；没有采手机 TX／Android 共同日志、核验 APK 来源，不能进一步区分 QQ 重复发布的具体回调，也没有真实同 ID 更新样本。若以后要求同会话合并或聊天撤回自动替换，需手机端提供稳定 ID／原通知关联；这是后续方案，不在本轮按标题或撤回文案删卡。

## 行为

- 内存最多十条消息，最新在前；同 ID 更新并移到前面，收到手机协议 `notify-` 后按 ID 移除。QQ 聊天撤回提示是否产生此操作取决于手机端转发，不能保证仅凭提示删除原消息。列表打开期间更新不会重建正在触摸的行；按下与松开时 ID 不同则取消点击。
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
