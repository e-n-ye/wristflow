# 天气 UI 与同步分支记录

<a id="weather-regression"></a>
## 2026-10-09 同版天气相关交互验收

同一 `83a6bd2d8954b69e3e8536b0f249efb169381e7e` 完成天气 PR 的必要有界 USB 回归；下方各版证据按原日期保留，其中“等待本版交互 / 保持 Draft”的状态由本节验收判定覆盖。当前活动项只见 [STATUS](STATUS.md#current)，最终交付须以 [集成 PR #38](https://github.com/e-n-ye/wristflow/pull/38) 的 MERGED/merge commit 回读及主线核对为准。

**交付历史阻碍**：[原 PR #37](https://github.com/e-n-ye/wristflow/pull/37) 最终审阅候选 `28e8be5` 为 MERGEABLE/CLEAN，但指定该 head 的 Rebase 返回 `This branch can't be rebased`，未合并。原分支及证据保留；从主线 `7d11849` 建立线性集成 `67bb00f`，完整 tree `1ed69fa7670b9312a3a8c914af9fc93c2c4a1d49` 与原候选一致，此后仅调整交接文档，固件/测试/SDK 不变。服务器未报告具体失败提交，复杂合并历史/中间重放冲突仅为推断，不记为最终代码冲突。#38 实际交付后将 #37 标为被替代，不声称 #37 自身 MERGED。

- **源码与编译**：生产实现/相关测试相对已审 `83a6bd2` 未改；生命周期、请求/缓存、逐槽目标及默认 registry 抽查未发现明确新阻碍。沿用 [D8 构建与主机证据](BUILD.md#2026-10-09-d8-队列契约构建)，没有新构建/烧录。三镜像重哈希一致，Product 主 BIN 7,620,916 B / `35397876…`。源码检查、既有成功编译和本节新硬件观察分别记录。
- **设备与采集**：同一黄山派板屏、COM5 CH340、仅 USB/无电池。初次枚举未找到板，用户接回 USB 后 pyserial/PnP 确认同一设备；采集捕获一次冷启动，布局恢复 generation=37/pages=3。工具未操作复位，不开电脑 BLE peer、不注入天气，仅用有回执的 status/PM 诊断。页面段 642.641 秒 / UART 13,517 B，连接段 170.250 秒 / 2,446 B，均退出 0、无未完成计划，串口已关闭。原始长日志仅留 `D:/MY_Desk/project/wristflow/artifacts/weather-regression/`；命令、哈希与精简时间轴见 [证据](evidence/2026-10-09/weather-regression.json)。

| 分组与次数 | 用户反馈与实际范围 |
|---|---|
| 五页显示/纵向到达 | 明确询问当前、预测、未来、指数、日升日落的字段合理/缺字/遮挡/卡住，用户回复“均正常”；AQI/UV 缺失占位按已有规则验收 |
| 横向分页与边缘返回 | 6 组小时（每组 4 项）、2 组多日（5+2），末端再划、回首组、正文不误退出，以及预测/未来标题左边缘返回后重入，用户回复“均正常” |
| 3 轮返回重入、1 次亮屏 KEY1 | 分别从预测/未来/日落退出后重入；确认首屏、缓存及无残留加载/错误，亮屏 KEY1 回表盘再进入，用户回复“均正常” |
| 2 轮自然短睡 | 用户报告约 20 秒后恢复熄屏前页面，另确认“都正常，已准备好”（触摸/数据）；日志实际两段 off→KEY1 为 30.734 / 24.031 秒 |
| 1 次手机蓝牙关闭/重开 | 断开后缓存/滑动正常；重连收到新天气，用户回复“显示刚刚接收，均正常”，包括返回重入及无卡住/异常重启 |

**短睡预期纠正**：助手最初要求约 20 秒后回表盘，用户指出恢复原页。经 [既有契约](UI-INTERACTION.md#状态机核心契约细则)和 `ui_shell.c:330–351` 核对，少于 2 分钟恢复原页才正确；达到 2 分钟的普通页面才回表盘，计时/编辑页另有豁免。按正确规则验收，未改固件或产品决定。两轮对应 431.922→462.656、472.672→496.703；准备阶段的 8.093 秒不计 20 秒轮次。本轮没有天气页 ≥2 分钟真机结果。

**运行与连接证据**：手机最初在第 1 代连接后校时/接收 UTC=1791524396，真实乐清市 20°C、湿度 71%、24 小时/7 日预报与太阳时间；五页、分页及一次唤醒后的确认状态均 READY/IDLE。连接段 41.875 断开 reason=19；64.969 为 connected/subscribed=0、generation=2，但缓存/received_utc 保留。93.766 重连第 3 代，重新订阅、MTU=131，94.453 收到真实天气；127/135 秒为 cached=1、READY/IDLE、received_utc=1791525094。本次上电 8 个完整 status 的 rejected/unknown/dropped/gaps/stale 均为 0；不更改上轮 D2 的 unknown=3 累计基线。串口不能独立证明每个手势轨迹或所有板发 TX，显示/操作次数来自逐组明确问题的用户反馈。

**验收处置**：相关同版交互/连接阻碍已解除，支持在最终 head 审阅和文档检查后转 Ready、Rebase 合并。三小时/RTC 跳变仅主机边界，精确到点自主唤醒、目标并发压力/IRQ 延迟、物理掉电/GC 中断、完整 N2 10 轮导航/10 轮睡眠/5 次重连及 30–60 分钟、功耗/续航仍未覆盖；保留这些限制，不写成设备通过。本次有界回归与既有源码/主机/编译证据共同支撑日常交付，不把整套联合验收或 C2 独立问题附加为天气 PR 新闸门。用户最后确认本次持续亮屏“已关闭”，手机正常连接；该确认来自用户，不从关闭串口推断背光状态。

<a id="weather-deadline"></a>
## 2026-10-09 D2 请求截止真机补验

本轮验证同一 `83a6bd2d8954b69e3e8536b0f249efb169381e7e` 固件，只补 30 秒无回复、重复请求、息屏跨截止、重试和真实手机恢复。下方 `767a67b` 的源码/主机/编译和接收反馈按原版保留；本节覆盖这些边界的未验状态，不把不同版本的分项结果合成完整联合通过。完整时间轴、命令、原始文件大小/哈希和失败见 [精简证据](evidence/2026-10-09/weather-deadline.json)。本机长日志位于 `D:/MY_Desk/project/wristflow/artifacts/weather-deadline/`，不随仓库提交。

- **源码与编译身份**：未修改固件、XML/生成物/字体、SDK 或子模块，未新增构建/烧录/复位。沿用 [D8 构建](BUILD.md#2026-10-09-d8-队列契约构建)及其已写镜像；本轮重新核对三镜像哈希，主 BIN 为 7,620,916 B / `35397876…`。源码检查确认请求截止由服务/worker 持有，getter 也能推进状态；编译成功和本节真机结果分别记录。
- **条件与方法**：同一黄山派板屏、COM5 CH340，仅 USB、无电池。手机蓝牙暂时关闭，电脑真实 BLE peer 只订阅 NUS TX 并接收板发请求，不发送天气或校时应用载荷；UART 1M、DTR/RTS 为 false。测试依赖 Python 3.12.14、Bleak 3.0.2、pyserial 3.5 只装在忽略的本轮目录。亮屏阶段使用既有本次 5 分钟持续亮屏；息屏阶段关闭，最后手机/Gadgetbridge 和正常亮屏策略均已恢复。
- **亮屏截止与合并请求**：首个请求执行回执 `host_s=30.281`，板发 weather 在 30.437；约 10 秒后的重复请求有 queued=1 回执，但 30.437–62.312 内只发一条 weather。两次请求后的 remaining 分别为 28,988 / 18,978ms；58.265（首回执后约 28 秒）仍 PENDING、余 1,994ms，62.312（约 32 秒）为 TIMEOUT、余 0。用户确认“显示重试，已点击显示同步，”及“然后显示同步超时”，日志另记录到两条后续 UI 请求；重复 pending 请求没有续期，超时后可重新请求。
- **息屏跨截止**：修正版同一第 5 代连接的两次请求，先有完整 pending 回执，再自然熄屏；没有断连、重订阅或电脑天气回包。下表时间均相对该次采集开始，截止由 pending 状态的 remaining 估算。

| 请求回执 host_s | Pending remaining | Screen-off host_s | 估算截止 host_s | KEY1 host_s | 首次唤醒后状态 |
|---|---|---|---|---|---|
| 63.093 | 29,032ms（64.046） | 71.187 | 93.078 | 243.765 | 244.625：request=3 / remaining=0 |
| 246.656 | 29,009ms（247.640） | 253.812 | 276.649 | 320.703 | 321.531：request=3 / remaining=0 |

两次均为 `cached=0 state=3`，中间新请求重新取得约 29 秒截止。KEY1 自动 PM 报告先于 UI/getter，background 从 11 到 12、再到 13，phone/phone_off 始终 7/3，符合 worker 后台收尾路径。但 background 不是专属超时计数，PHONE 可与 KEY 合并，getter 也可推进状态；未取得息屏下到点转移的独立时间戳，不能据此宣称精确 30 秒自主系统唤醒或连续深睡。五秒 sample 在 71.046 仍 `off=0`，263.468 的息屏 sample 没有报告，均不能充当已确认的息屏计数基线。

- **真实手机恢复**：电脑 peer 正常退出并关闭串口后，恢复 Gadgetbridge 第 7 代连接/订阅、MTU=131。校时 UTC=1791523417；主动重试 queued=1 后收到真实乐清市 20°C、湿度 71%、24 小时/7 日预报和太阳时间。35/42/50 秒状态均 READY/IDLE、cached=1，received_utc=1791523420，比校时晚 3 秒。用户确认“恢复温度/预报及‘刚刚接收’，返回再进入正常”，随后确认本次持续亮屏“已关闭”。这是一次正常返回重入，不能抵扣完整矩阵。
- **失败与采集边界**：首段亮屏证据已完整，但之后带乱码前缀的 status 未执行，输出 U+FFFD 触发 GBK 编码错误，助手退出 1。修正为 ASCII JSON 输出并按 KEY1 日志自动发命令后，补测退出 0（UART 6,410 B）；手机恢复采集退出 0（2,562 B），全部工具连接/串口已关闭。首段无回执的 clear/status/PM、过期的主机等待、息屏 sample 均保留为失败/未确认，不计通过。各确认 status 的 rejected/dropped/gaps/stale 为 0，unknown 为不变的累计基线 3，不写成 0。

本轮关闭上述有界请求截止与恢复验收。天气 PR #37 仍为 Draft，剩余具体阻碍是选定同版的必要连接/页面交互及完整返回重入验收；D3/D8 已修结果见各自专题，不沿用下方旧段的未修状态。三小时/RTC 跳变仍仅主机边界证据，目标并发压力/IRQ 延迟、完整联合矩阵和功耗未覆盖。当前活动项与唯一下一步只见 [STATUS 当前区](STATUS.md#current)。

<a id="weather-d2"></a>
## 2026-10-08 D2 接收时间、过期与请求状态

本轮验证源码为 `767a67b51c1cd8d29e6df29a76d3d88f80c30688`，基于天气候选 `bb5bb39`，仅处理 D2。下方 v2 记录保留其原版本的接收和用户反馈；其中旧的在途标记、借用校时时间及未完成 D2 的描述由本节覆盖，不把旧真机结果算作本版通过。

- **时间**：接受有效天气时由服务采样实际 RTC 接收 UTC；未校时时为 0。原版报文没有来源观测时间，因此页面改称“接收”，不把手机缓存重发写成来源刚刚更新。数据年龄和 30 秒请求截止使用单调时间，手机校时前跳/后跳不改变年龄。适配层在共同天气锁内先扩展原始 uint32 tick 再换算；当前配置为 1000Hz，BLE worker 无请求时最多等待三小时，保证采样间隔小于 tick 回绕周期。SDK tickless 补 tick 仅有源码依据，本版睡眠跨截止/过期边界的实际计龄仍待 USB 核查。
- **独立年龄**：当前天气、小时/多日预报、太阳时间各自记录接收时刻。解析器标明本包新交付的 extras；同源 v1 和 v2 当前头继承预报时不刷新其年龄。三小时边界进入 STALE，当前温度仍可查看并标记“天气已过期”；过期的预报/太阳时间恢复占位，卡片说明也显示过期。无时钟适配时明确“接收时间未知”。
- **请求**：服务持有 request ID、连接代次、状态和原始截止。重复请求/返回重入合并且不续期；入队失败、完整 TX 失败、断连/代次变化和无回复超时收尾，只有有效天气结束同步。晚到旧 ID 的发送结果不结束新请求。BLE 回调继续只改连接字段/入队；普通通知、校时、REJECT 和页面刷新不覆盖天气请求。每次连接最多一次自动 v2 补请求，显式重试仍可再次发送，避免 v1 fallback 循环。
- **显示**：空缓存超时显示“天气同步超时”并恢复重试；已有缓存失败时保留旧值，过期页显示“已过期 · 同步中 / 同步超时 / 更新失败”。复用现有重试按钮，在当前页范围与空气质量之间显示；pending 时隐藏。UI 只读快照，未变化时跳过重绘。XML、生成 C、字体资源未修改，也未重新 Pro 导出。
- **主机与编译**：`artifacts/weather-d2-host` 的 CMake/Ninja/CTest 17/17 通过，覆盖成功入队与回包、29999/30000ms、失败/断连/快速重连、旧 ID、返回重入、非天气刷新、未校时/RTC 跳变、三小时边界及继承 extras 老化。实际 LVGL 9.4.0 390×450 的超时/过期重试快照、逐字字形与按钮几何检查通过；快照为合成数据。Product/UI Demo 官方编译与 310/299 个源码、29/22 个产物重哈希通过；完整命令和哈希见 [D2 精简证据](evidence/2026-10-08/weather-d2.json)。
- **USB 写入与接收**：同一 COM5 CH340，用户确认“已接好，仍仅 USB 供电”，不接电池。dry-run 和三镜像写入/verify 均退出 0，记录 `artifacts/flash/product/20261008-230016-845/result.json`；主 BIN 与本版 Product 构建一致，未写 settings 或整片擦除。95 秒启动和 36 秒补采分别保存 `artifacts/weather-d2-host/startup.bin`（4,836 B）、`receipt.bin`（5,257 B），退出 0、串口已关闭。确认 CO5300/FT6146、连接/订阅、MTU=131、KEY1 恢复，以及真实乐清市 20°C、湿度 71%、24 条小时/7 条多日预报和太阳时间；两段中的主动请求均 queued=1 后收到完整回包。
- **D2 真机状态与反馈**：补采的未连接状态为 `cached=0 state=3 request=2`（失败）；连接后手机校时 `1791472090`，主动请求回包后的两次 status 为 `cached=1 state=1 expired=0 age_known=1 received_utc=1791472093 request=0 remaining_ms=0`，证明实际接收时间与旧校时分离、成功请求结束。两次 status 的 rejected/unknown/dropped 均为 0，只覆盖这一短窗口。用户先反馈“显示刚刚接收”，随后反馈“现在显示一分钟前接收”；这是本版年龄显示反馈，不替代完整返回重入矩阵。首段熄屏后的 status 无有效回执，末次出现带乱码前缀的 command not found；保留该采集限制，补采亮屏命令正常。30 秒无回复、三小时、RTC 跳变与睡眠跨截止的真机边界，以及目标板并发压力尚未验证；不把短时 KEY1 恢复当作这些边界通过。

天气 PR #37 保持 Draft；本轮完成 D2 实现与上述有界验证，不关闭其真机边界或天气整体。D3 是主线已有的配置缺项风险，当前完整默认 registry/layout 不触发；D8 在 BLE、UI/串口多个生产者之间存在分配序号与实际入队交错的源码风险，尚无真机复现。两项与相关同版交互验收独立保留，本轮未修复，也没有功耗结论。活动项与下一闭环只见 [STATUS 当前区](STATUS.md#current)。

<a id="weather-v2"></a>
## 2026-10-08 原版 Gadgetbridge v2 补齐

用户确认先补原版 Gadgetbridge 已提供的能力，AQI 等手机端扩展后置。本轮固件在订阅和手动同步时请求 `{"t":"weather","v":2,"f":true}`，独立解码 `l/c/d`；继续接收 v1 自动推送，同城保留扩展数据并有界补请求 v2。来源身份使用完整城市字符串，不能用截断后的显示名判断同源；重连、重复订阅和手动清空覆盖 parser 与共享缓存。

- **协议**：依据归档 Gadgetbridge 0.94.0 `BangleJSDeviceSupport.java:2016–2162`，38 字节当前头、最多 236 字节完整数据，小端和六组并列预报数组。复用锁定 SDK 的 mbedTLS Base64，额外检查编码字符/分组/padding、精确结构长度与计数；坏帧不发布半份更新。
- **显示**：使用前 24 条小时预报、最多 7 条多日预报及独立有效的日出/日落；时间沿用 Product UTC+8。多日报文没有日期或观测时间，按“第1天”至“第7天”显示来源顺序，不推测星期/日期。缺少小时基准时间或偏移异常时只隐藏该组；无预报头保留同源预报，明确空数组清空。太阳位置点保持隐藏，不用静态标点充当实时位置。
- **缺失**：AQI 无出站字段，继续 `--`；UV 非零且合理时可显示，零与缺失不可区分，继续占位。v2 零风速/湿度也不能区分缺失；小时 AQI 保持占位。现有图标只显示可准确表达的晴/云/雨，其他码不伪造图标。
- **同步**：服务启动时注册天气 mutex；缓存写入、清空、存在性查询和完整快照读取共用锁，格式化在锁外。v1 自动更新后的 v2 补请求带在途标记，避免 fallback 循环；任意 v2 回复结束该在途标记，不含预报的回复不证明预报已同步。时效、真实接收时间和请求超时仍属未完成 D2。
- **验证**：主机 16/16 通过，新增全部分片、逐字节截断、最大计数、255 偏移、错误 Base64/版本/计数、同源保留/换城/清空/重连和真实协议到 LVGL 标签回归。390×450 实际 LVGL 9.4.0 快照检查通过；风图标和值分两行，最长 `255 km/h` 可放下。XML/生成资源未变，本轮未重新 Pro 导出。最终两目标编译、镜像和 USB 结果另行绑定到 [v2 证据](evidence/2026-10-08/weather-v2.json)。
- **USB 接收**：验证源码 `10331bf7eac9d51c5ee04d43b849ae3bd817d6cc`，同一 COM5 CH340、USB-only、未接电池。三镜像写入/verify 退出 0，记录 `artifacts/flash/product/20261008-215106-805/result.json`；主 BIN 与本轮 Product 构建哈希一致，未写 settings 或整片擦除。90 秒复位启动采集 `artifacts/weather-v2/startup.bin`（4,540 B）确认 CO5300/FT6146、display on、一次 KEY1 恢复、连接/订阅、MTU=131 和真实手机校时。初始未连接时主动请求返回 queued=0；随后收到乐清市 20°C/晴朗/湿度 68% 及 `hours=24 days=7 sunrise=1791409920 sunset=1791452100`，太阳时间按 UTC+8 为当天 05:52/17:35。采集退出 0、串口关闭；这段解码日志本身不证明屏幕交互，后续显示反馈见下一项。
- **重发与用户反馈**：另一次 60 秒非复位采集 `artifacts/weather-v2/resend.bin`（1,408 B）确认 KEY1 恢复，`wf_ble weather` 返回 queued=1，随后再收到真实 v2 的 24 小时/7 日/同一太阳时间，当前湿度更新为 71%。两个 status 命令没有回执，不记录最终解析/丢包计数；采集退出 0、串口关闭。用户随后明确反馈“24 条小时预报、7 条多日预报和日出日落时间均已显示”，因此本轮已有屏幕显示确认；没有逐项回复返回重入和完整交互矩阵，不扩大验收范围。

本轮是天气候选的一个闭环；完整天气仍待 D2/D3/D8 及相关交互验收，不因接通预报关闭整份 PR。

<a id="weather-v1-range"></a>
## 2026-10-08 真实来源反馈与 v1 高低温补丁

用户在配置天气来源后提供七张截图/照片：Gadgetbridge Cache 出现两条“乐清市”，Breezy 显示 Open-Meteo、20°C、晴朗、湿度 68%、风速 2 m/s，手表显示相同城市、温度、天气和湿度及 7 km/h（与 2 m/s 换算、取整一致）。这些是 `02d4647` CRLF 版后的现场反馈，确认基础天气已到达屏幕；没有新的手机原始报文采集，不能据照片关闭完整返回/重入/KEY1 回归或更新时效问题。两个同名缓存条目的成因未确认。

定点核对 Breezy Weather v6.2.2 和归档 Gadgetbridge 0.94.0 的字段路径：

| 字段 | Breezy 到 Gadgetbridge | Gadgetbridge 到设备 / 当前固件 |
|---|---|---|
| 当天高低温 | `todayMaxTemp/todayMinTemp`，整数 K | v1 已发送 `hi/lo`，固件此前未解析。本轮 `5d286c7` 补解析及快照范围；缺失的 0 K、非数字、越界和倒置保留占位，真实 273 K 显示 0°C |
| 紫外线 | `uvIndex` | v1 发送 `uv`，但 Receiver 把缺失值填成 0，发送端还使用整数除法。真实 0、缺失和低于 1 的指数不可区分；本轮继续占位，不把默认 0 当已验证的来源数据 |
| 逐小时 / 多日预报、日出 / 日落 | `hourly/forecasts/sunRise/sunSet` | v1 不发送；请求 `{"t":"weather","v":2,"f":true}` 可获得 v2 Base64 二进制数据，当前固件尚无解码。独立后续闭环处理，不能仅换请求后把 v2 交给现有 v1 parser |
| AQI | `airQuality.aqi` | Bangle.js v1/v2 均未编码 AQI；需另行扩展发送契约，当前继续占位 |

本轮只补已有 v1 高低温，不改 UI XML、请求版本、预报协议或紫外线策略。明确 `v:1` 的当前温按 K 解码，无版本的既有温度兼容路径保留。每份天气覆盖范围有效位，后续报文缺少范围时不会残留上次值。

源码为 `5d286c7df5c5e0630b15413d78a9c9c79e043910`；主机 16/16、Product/UI Demo 编译及 307/299 个源、29/22 个产物重哈希通过。真实 LVGL 390×450 新快照显示 `26°/18°`，来源取值和缺失/撤销占位有主机回归。COM5 CH340 重新枚举、dry-run 与三镜像写入/verify 退出 0；90 秒启动采集确认 CO5300/FT6146、自动重连/订阅、MTU=131、真实校时、乐清市 20°C 晴朗/湿度 68% 的天气更新及 KEY1 唤醒。串口已关闭。用户对 KEY1 返回重入答“应该是正常的”，随后新照片显示手表高低温 `25°/18°`，与 Breezy“今天 10/08”的 daily 预报 `25°/18°` 一致；本次仍不扩大成完整交互或时效通过。Breezy v6.2.2 的 `Weather.kt:94–124` 明确 18:00 后顶部范围使用 today.night + tomorrow.day，所以顶部“白天 26°”是明日白天，不是今天最高温。手机系统天气显示 `18～26°C`，来源/更新时间未核实，不能将区别直接归因为固件错误。命令、产物及边界见 [精简证据](evidence/2026-10-08/weather-v1-range.json)。下方 D1 初版和 CRLF 构建记录保留为历史证据。

此处高低温版之后，用户追问 AQI 和手机端开发含义，随后确认“先补已有的，要在 Gadgetbridge 基础上开发的之后再做”。AQI 是空气质量指数；现有手机接收模型已有 `airQuality`，缺的是 Bangle.js 出站字段。若后续扩展，需要修改 Gadgetbridge 及固件发送/解析契约、编译安装修改版 APK。本轮范围与进展以上方 [v2 补齐](#weather-v2) 为准，旧固件验证不覆盖新增能力。

## 2026-10-08 D1 生产数据真实性修复候选

工作目录 `D:/MY_Desk/project/wristflow`，分支 `codex/weather-d1`，基于 `main@7d11849` 接入保留天气成果 `f68d64f`；D1 初版为 `3e1a9ce`，CRLF 修正版验证代码为 `02d46475af6c4dc7cd06b361e9153bfb75c6529a`，其后的高低温补丁另见上节。尚未合入 main，天气整体保持 Draft；[精简证据](evidence/2026-10-08/weather-d1.json)绑定初版/CRLF 两版结果，不覆盖后续或下方历史版本。

- **源码修复**：`core/weather.c` 删除 fixture provider 和无缓存回退；`product_ble.c` 删除 `mock_weather` 命令及假报文。旧完整演示数据移到 `apps/ui_demo/src/weather_fixture.c`，由 `weather_ui` 测试显式链接；Product 清单、map 与 ELF 符号均无该 provider 或 mock 命令。Demo 清单编译该文件，当前 Demo 固件无调用者，因此链接不保留其 factory；主机专用测试仍实际渲染完整演示天气。
- **数据表达**：当前温度与高低温、预报、太阳位置各有有效位；湿度区分缺失和真实 0%。缺天气文本、AQI、日出/日落及预报显示占位，不填多云、假温度或固定时间。协议窄化前检查温度/code 范围，风值保留单位。
- **界面修复**：Product 冷启动 EMPTY、卡片显示 `-- / 暂无天气数据`；清空刷新和重入恢复 EMPTY。重试失败为 ERROR，成功入队只到 LOADING；每次刷新覆盖全部 24 小时/7 天标签和页面背景，无真实太阳位置时隐藏 marker。挂载生成日升日落页时覆盖样例时间；指数长值换行、小字号保持单位可读。
- **主机验证**：主机/UI Demo 锚点 `3e1a9ce` 增量构建与 CTest 16/16 通过，覆盖无缓存、缺湿度/真实 0%、部分及越界报文、卡片占位、清空后刷新/重入、Demo 转真实数据清除预报与既有手势。真实 LVGL 390×450 快照保留在本机 `artifacts/weather-d1-host/renders/`，其中 `weather_product_{empty,partial,current,indices,sun}` 检查了占位、风速单位与 marker；本轮 XML 无语义修改，生成 C 只做末尾空行规范化，未重新 Pro 导出。
- **初版固件编译**：Product `artifacts/product/20261008-193403-027/result.json`、UI Demo `artifacts/ui_demo/20261008-193403-861/result.json` 均退出 0、产物校验通过；初版源码哈希分别 307/307、299/299，产物分别 29/29、22/22 匹配。最终 Product CRLF 镜像另见下方补构建；版本、镜像与命令见[构建记录](BUILD.md#2026-10-08-天气-d1-候选构建)。
- **初版 USB 回归**：用户确认接入同一板屏、USB-only、不接电池；枚举 COM5 的 CH340 / `1A86:7523` / `1-1.1`，三镜像 dry-run 及写入/verify 退出 0，记录 `artifacts/flash/product/20261008-193935-833/result.json`。启动采集确认 CO5300、FT6146、display on、Product、BLE 广播；随后自动重连、订阅、MTU=131、真实手机校时。用户只确认点击重试一直处在“正在同步天气”，其他占位/手势尚未确认。一次非复位采集的三个串口命令没有回执，不能记为清空或请求诊断执行成功；日志保留于 `artifacts/weather-d1/20261008/` 和 FastCtx jobs `j-7tusk5` / `j-u4ty8l`。

### 手机日志触发的 CRLF 修复

用户随后提供本机 `gadgetbridge(1).log`（28,851 行，版本 `0.94.0-2ee6b3002 (dirty)`）。天气相关五次请求中，原始接收字节包含完整 JSON，但 `UART RX LINE` 缺失末尾 `}`，随后 `Unterminated object`；最近窗口为 28713–28721、28792–28800、28815–28823 行。未见天气 `UART TX`，不能归因为未配置数据源；日志只有时分秒，不推断全部记录的日期或固件版本。

归档 Gadgetbridge 0.94.0 的 `BangleJSDeviceSupport.java:1225–1230` 在 LF 位置无条件截去前一字符，要求 CRLF；`:630–632` 支持天气请求，`:1972–1977` 数据为空时另有明确告警。Product 原发 LF，所以完整 `}` 被截掉。本轮只将 `product_ble.c` 的版本、订阅天气、主动天气、GPS 回复四个 JSON 帧统一改为 CRLF；无手机端修改。按实际四个 C 字面量、同一拆行算法和 JSON parser 对每个两片切分核对，旧版 121/121 失败，修正版 129/129 通过；只证明分行兼容，不证明来源有效。

CRLF 补构建 `artifacts/product/20261008-194850-771/result.json` 已成功，307 个源/29 个产物哈希匹配；新主 BIN 7,614,892 B / `ae291fa22acca8ff6a2a9e5af3434c8dc4007d9907a780cc9aac6f23039a6bcb`。源哈希与最终代码 `02d4647` 匹配，UI Demo/主机目标不编译改动的 Product 平台文件，其既有通过记录继续有效。

修正版再次枚举同一 COM5 CH340、dry-run 通过，三镜像写入/verify 退出 0，记录 `artifacts/flash/product/20261008-195045-752/result.json`；不写 settings、不整片擦除。90 秒复位启动采集 `artifacts/weather-d1/20261008/crlf-startup.bin` / FastCtx job `j-agfimy`：CO5300、FT6146 与 display on 正常出现；亮屏窗口内 `clear_weather` 有回执，未连接时 `weather request queued=0`；随后自动重连、订阅、MTU=131 和真实手机校时，未观察到天气更新。采集完成、串口关闭。完整天气报文和屏幕值仍待核实，不能把“请求已发送”或校时成功当作天气同步成功。SDK 横幅仍显示 Oct 5 编译日期，是增量构建保留的 SDK 对象时间，不用它识别 Product 源码。

### 下一处来源核查

归档 Gadgetbridge 0.94.0 的简中设置文案说明：LineageOS 可使用系统天气服务，其它 Android 需类似 Breezy Weather 的应用提供天气；`Weather.kt` 维护缓存，`GenericWeatherReceiver` 接收外部天气广播。用户随后确认 `Debug → Weather → Cache` 显示 `No cached weather`，截图中“缓存天气信息”已开启，证明当前没有可供重发的天气条目；不能进一步断言未安装来源或 Android 广播投递失败。本轮没有添加测试天气或向缓存注入。

[Gadgetbridge 官方指南](https://gadgetbridge.org/basics/integrations/weather/) 的 Breezy Weather 路径为 `Settings → External modules → Send Gadgetbridge data → Gadgetbridge`。v6.2.2 源码及简中资源核对的入口为 `设置 → 微件与动态壁纸 → 数据共享 → 发送天气数据到 Gadgetbridge`，需要选择已安装的 Gadgetbridge 接收包并保存；`ModulesSettingsScreen.kt` 保存所选包后立即触发发送，`GadgetbridgeService.kt` 在全部位置都无当前天气时不生成发送内容。推荐从 [官方 v6.2.2 发布页](https://github.com/breezy-weather/breezy-weather/releases/tag/v6.2.2) 安装 standard APK，先添加真实所在地并成功取得天气，再选择 Gadgetbridge 输出并刷新。多位置时本设备使用第一项。随后回到 Gadgetbridge 查看真实 Cache 条目；已有条目且设备连接时点 `Send weather to devices`，核对手表城市、当前温度和缺失字段占位。归档接收器为 exported、没有声明 receiver permission，处理流程未检查额外外部来源开关或发送方白名单；这只是源码检查，用户手机的实际投递仍待验。来源配置、真实缓存、设备天气报文与现场屏幕分别记录，不把来源安装当作链路通过。

**后续反馈与仍待处理**：来源配置后的照片已确认基础天气显示，高低温补丁已有主机/编译/USB 证据及用户总体正常反馈，见上节。D2 的共同同步、接收时间/老化及非天气事件覆盖请求状态未改，D3/D8 未核查，完整交互矩阵仍待。主机成功请求只直接设置 LOADING，未覆盖 retry 的 `queued=true` 分支；需要在 D2 补真实请求结果的可测试边界。完整天气 PR 不能因局部修复而转 Ready 或合并。当前活动项与唯一下一步见 [STATUS](STATUS.md#current)。

## 2026-10-07 静态审查更正

以下保留天气分支的历史开发/验证记录，**不表示天气代码已合入主线**。本次审查对象为 `codex/weather-sync@0606f3b`，文档整理起点主线为 `9ce5b3e`；没有新增构建、烧录或实验。

已存在天气报文解析、请求与 UI 更新代码，也有旧版本 mock 注入和用户视觉反馈；**真实所在地手机天气推送仍未闭环**。Product 无数据会回落 fixture；最高/最低温、日出日落与预测存在虚构/演示填充值。天气跨线程读写/重置没有共同同步；生产读取均传 `now_utc=0`，三小时老化条件不可达，因此“三小时软过期已完成”撤回为未完成。源路径与关闭条件见 [架构债务 D1/D2](ARCHITECTURE-REVIEW.md)。

既有 CTest、构建、烧录和局部显示反馈只说明各自记录的范围；串口 mock 注入不能证明手机真实来源、时效或线程安全。未来移除 Product fixture/mock 的决定不抹除当时诊断证据。本轮先记录债务，后续按 N0/N1 处理。

## 早期五页 UI 阶段（历史）

更新：2026-10-05。本增量把 Product 的天气入口从占位页改为可浏览的五页天气界面：当前天气、逐小时预测、每日预测、天气指数和日升日落。该早期阶段数据来自 `core/weather.c` 的确定性本地 fixture，页面预留加载中、暂无数据、更新失败和重试状态；后续同步代码见下方 17:45 记录，当前缺陷以上方 2026-10-07 更正为准。

本轮按 Redmi Watch 4 日升日落页实拍重做了曲线视觉。日升日落页使用一张 336×128 的透明 ARGB8888 贴图：白色日间弧线、虚线地平线和地平线下较暗的延伸线在贴图中固定；太阳点、标题、日出/日落文字和天气背景仍由页面控件与运行时负责。贴图没有嵌入整张手表照片，也没有把相机曝光、屏幕扫描线或透视差异当作设计目标。晴天使用明亮蓝色，阴天/多云使用明显偏灰的灰蓝色，背景由 fixture 状态驱动。

## XML、Pro 导出和运行时的准确关系

- 视觉源是 `ui/xml/screens/screen_weather_sun.xml`；贴图源是 `ui/xml/images/weather_sun_track.png`，其生成脚本为 `scripts/Generate-Weather-Sun.py`。`ui/xml/globals.xml` 以 `argb8888` 注册 `weather_sun_track`。
- 使用 LVGL Pro Editor 2.0.1 Community、LVGL 9.4.0 打开 `ui/xml/project.xml`，在目标 `huangshan_390x450` 中预览日升日落页并执行 `Ctrl+B`（Export Code and Recompile）。输出区记录 `Project compiled successfully`、`Initializing custom C code using LVGL v9.4.0`；原始输出保留在 `artifacts/lvgl-pro-weather/20261004/pro_output.txt`。
- Pro 生成物包括 `ui/xml/screens/screen_weather_sun_gen.c/.h`、`ui/xml/wristflow_ui_gen.c/.h`、`ui/xml/images/weather_sun_track_data.c` 和 `ui/xml/file_list_gen.cmake`。资源清单实际编译 `weather_sun_track_data.c`，生成代码把 `weather_sun_track_image` 绑定到 `weather_sun_track`。
- `ui/runtime/weather_screen.c:create_sun_page()` 挂载 `screen_weather_sun_create()`，再覆盖生成页面根背景色，所以日升日落页的结构和贴图来自 XML/Pro 生成页面，天气状态背景仍由运行时传入。
- 目前只有日升日落页由生成页面挂载。`screen_weather_current_gen.c/.h`、`screen_weather_hourly_gen.c/.h`、`screen_weather_daily_gen.c/.h` 和 `screen_weather_indices_gen.c/.h` 虽在 Pro 生成清单中，但五页外层与前四页内容仍由 `ui/runtime/weather_screen.c` 运行时 C 创建；不能把五页都称为 XML 驱动。

## Redmi Watch 4 视觉基线

用户提供的实机照片确认天气页面的层级：顶部标题与时间固定，当前天气突出显示温度和天气状态，逐小时/未来天气使用横向分页，天气指数与日升日落作为独立信息页，底部分页点持续可见。照片拍摄造成的色差、透视和屏幕纹理只用于判断层级，不复制进 UI。

背景色随天气状态变化：晴天使用明亮的高饱和蓝色；阴天切换为明显偏灰的灰蓝色。五页在同一天气状态下保持一致的页面底色，白色文字与图标在两种底色上保持可读。

日升日落贴图位于 XML `(x=27, y=157)`，尺寸 `336×128`；全局地平线约为 `y=240`，弧顶约为 `y=160`，左右交点约为 `(72,240)` 与 `(318,240)`。太阳点中心为 `(272,199)`，处于下降段且高于地平线；暗色延伸线位于地平线下方。

## 行为与实现

- 当前页显示城市、更新时间、温度、天气状况、最高/最低温和空气质量。
- 逐小时页支持 24 小时数据横向分页；每日页支持 7 天数据横向分页。外层五页纵向滚动，预测页内容区保留水平手势，边缘返回仍可用。
- 指数页显示空气质量、湿度、风力和紫外线；日升日落页显示时间和太阳轨迹。
- `ui/runtime/weather_screen.c` 负责五页容器、滚动状态、动态背景和日升日落页挂载；早期 `core/weather.c` 只提供 fixture 和状态文案；注册表仍由 `ui/runtime/app_registry.c` 统一提供天气卡片摘要和页面入口。

## 可复核证据

工作树为 `C:/Users/13984/.codex/worktrees/weather-ui/wristflow`，分支 `codex/weather-ui`，HEAD `9ce5b3e32e338a279741464709f11b8bc3213931`。SDK `421126d9f476ed8e2a6f0b0ca28a9f241c182e65` 及两个子模块未修改。

- 主机命令：`cmake --build artifacts/weather-host-build --parallel 6`；`ctest --test-dir artifacts/weather-host-build --output-on-failure --timeout 60`，最终 **16/16** 通过。`tests/weather_ui.c` 覆盖五页、水平/垂直分页、加载中/暂无数据/错误/重试、返回边界、晴天/多云底色，以及贴图尺寸、ARGB8888、弧顶、虚线间隔、暗色延伸、地平线和太阳点关系。
- 快照：`artifacts/weather-host-build/renders/weather_current.ppm`、`weather_current_sunny.ppm`、`weather_hourly.ppm`、`weather_daily.ppm`、`weather_indices.ppm`、`weather_sun.ppm`；`weather_sun.png` 是同一真实 LVGL 快照的 PNG 预览。当前页两种底色可由像素复核为灰蓝 `#687f91` 与亮蓝 `#0bb9f2`。
- Product 命令：`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 6`，退出码 0，官方 SCons 和产物校验通过。最新记录为 `artifacts/product/20261005-141441-099/result.json`；`main.bin` 为 7,612,188 B，SHA-256 `ee2ec45da3a93f02d47259a067c83c8d65f1ec27e78eae77f6b830e8447198b6`，`hardware_verified=false`。
- 烧录前确认 USB-only、未接电池，并枚举到 `USB-SERIAL CH340 (COM5)`、VID:PID `1A86:7523`、实例 `USB\VID_1A86&PID_7523\6&A8355E6&3&1`。使用 sftool 0.2.5 对 bootloader/main/ftab 执行三镜像 `write_flash --verify`，退出码 0；未整片擦除、未写 settings。启动串口记录到 CO5300、FT6146、`display on` 和 Product runtime 启动。原始证据见 `artifacts/flash/weather-20261005/evidence.json`；屏幕曲线视觉和触摸仍待用户现场观察确认。

这些结果分别证明源码/资源可读、Pro 导出成功、主机行为与视觉快照通过、Product 可编译、三镜像烧录校验通过并正常启动。

2026-10-05 16:25 手势修复真机验证：固件（`main.bin` 7,612,588 B）重新烧录并软复位后，用户现场观察确认：
- 天气预测与未来天气页面中，横向单页滑动正常，已消除因惯性连跳多页的现象。
- 页面中部内容区域直接上下拖动可顺畅带动外层五页垂直滚动，垂直滚动链放行生效。
- 用户同时观察到滑动中存在轻微掉帧（不够丝滑），该现象为既有已知表现，当前不作为关键路径阻塞项，留作后续性能阶段专项优化。
- 当前下一步转入手机天气数据同步（Gadgetbridge BLE 协议）需求对齐与落地。

2026-10-05 17:45 Gadgetbridge BLE 天气解析/请求与注入验证（历史）：
- 协议解析与事件联动：实现 Gadgetbridge `GB({"t":"weather",...})` 严格 JSON 解析，提取城市 `loc`、温度 `temp`（开尔文自适应减 273.15 转摄氏度）、天气文本 `txt`、OWM 天气代码 `code`、湿度 `hum`、风力 `wind`。
- 双向触发与时效：进入页面无数据或点击重试时主动通过 BLE 发送 `\n{"t":"weather"}\n` 请求；代码处理接收报文并更新 RAM；当时记录的三小时软过期未在生产调用路径生效，见顶部更正。
- 视觉与主题联动：由 OWM code / txt 关键字判定晴天明亮蓝（`#0bb9f2`）或多云灰蓝（`#687f91`），并在数据更新时自动刷新前台活动页面。
- 自动化与主机测试：`phone_protocol` 单元测试覆盖各类有效/非法/边界天气报文；CTest 16/16 全部通过。
- 硬件构建与烧录：`scripts/Flash.ps1` 扩展支持 `product` 目标；SCons 编译通过（`main.bin` 7,613,924 B），三镜像成功烧录至 COM5 并完成 `--verify`。
- 真机诊断验证：通过串口受控 RTS 脉冲复位后系统正常启动；执行 `wf_ble mock_weather` 注入测试报文，真机串口回读 `[product] weather updated: 杭州市 25 C, condition=晴 code=800 hum=65%`，该注入路径的解析与业务分发有日志，不证明真实手机天气端到端闭环。

2026-10-05 18:05 乱码与时间同步修复真机闭环：
- 中文乱码根因修复：`apps/product/src/phone_protocol.c` 原 `normalize` 对 `c >= 128` 单字节盲目转为 `\u00xx`，导致 UTF-8 多字节中文（如“杭州市”、“晴”）被拆散为 Latin-1 符号（`æ · å`）。新增多字节 UTF-8 序列探测与直通逻辑，兼顾 ISO-8859-1 回退兼容。修复后真机直接正确显示“杭州市”、“晴”。
- 指数页缺字方块修复：`ui/runtime/weather_screen.c` 指数数值采用纯 ASCII `metric_56` 字体，无法显示全角破折号 `—` 产生缺失字块 `[]`。将空值改用 ASCII `--`，并将风力提取纯数值，避免 88px 容器字符溢出截断。
- 右上角时间联动：`ui/runtime/apps.c` 与 `weather_screen.c` 接入系统 RTC 快照动态联动；未校时显示 `--:--`，校时后各预测与指数页右上角时间随系统实时更新。
- 硬件复验：编译生成 `main.bin`（7,614,332 B）并烧录通过；受控 RTS 脉冲复位后注入 `wf_time 1791223500` 与 `wf_ble mock_weather`，串口准确输出 `[product] weather updated: 杭州市 25 C, condition=晴 code=800 hum=65%`，屏幕时间同步与中文字符正常。

2026-10-05 18:30 卡片缺字方块修复与 BLE 主动同步落地：
- **架构与设计决策记录（可追溯性备查）**：
  - **背景与问题**：表盘主页天气卡片（`metric_half` / `metric_full`）副标题展示动态天气状况（如“晴”、“阴”、“雨”、“雪”）时，原 XML 声明使用的 `body_20` 为静态页面精简字库，未收录这些汉字，导致实机出现缺字方块 `[]`。
  - **决策对比与采纳方案**：用户确认采纳**定向运行时重定向方案**（在 `ui/runtime/components.c:paint()` 仅针对天气组件副标题动态挂载 `notification_22` 完整字库），放弃重新导出 XML 字库的冗长重度方案。
  - **决策依据与协调性论证**：
    1. **字源 100% 同源**：在 `scripts/Generate-Ui-Assets.py` 中，`body_20` 与 `notification_22` 均派生自同一套字体源文件（开源思源黑体 `NotoSansSC-Regular.ttf`），保留同源字体风格；不同字号的排版仍需逐场景复核。
    2. **排版安全性**：`notification_22`（22px）相比原 `body_20`（20px）仅增加 2 个像素。卡片副标题区域（`metric_half.xml`）宽度预留 280px，卡片行高 174px，显示“晴 25°/18°”或“等待手机同步”在当时观察的文案下排版正常；不能排除更长动态文本的截断/溢出风险。
    3. **组件隔离安全性**：仅对 `app->id == "weather"` 的天气卡片动态挂载，心率、活力指标、系统状态等其他组件继续保留 `body_20`，该分支没有对其他组件主动改字体；完整回归和后续解耦仍需验证。
    4. **全局视觉统一**：与天气应用内部页面（`weather_screen.c` 中“刚刚更新”、“空气质量”、“多云”等）统一使用的 `notification_22` 使用同一字库；其余字号、间距和动态内容分别检查。
  - **追溯说明**：该次选择已记录；不将运行时按应用 ID 分支当作最终通用组件方案。若后续组件系统整体重构字号层次或引入新字库，可直接定位并调整 `ui/runtime/components.c` 中的动态挂载逻辑。
- **BLE 订阅主动同步与缓存重置**：
  - 在 `apps/product/src/product_ble.c` 的 BLE 订阅事件（`PACKET_SUBSCRIBE && subscribed`）中，在发送版本号之后自动向手机 NUS 队列追加发送 `\n{"t":"weather"}\n` 报文，并在建立连接时自动调用 `wristflow_weather_reset()` 清空此前调试遗留的 Mock 数据，以便及时接收并呈现手机真实下发的实时天气。
  - 在串口调试命令 `wf_ble` 中新增 `clear_weather` 子命令，支持随时手动重置天气缓存并触发主界面刷新。
  - 在 `apps/product/src/main.c` 的 `WF_EVENT_PHONE` 事件中补充即时刷新 shell 快照（`wristflow_ui_shell_update`），使天气更新或重置时触发主页卡片刷新；没有新增延迟量化证据。
- **验证结论与真机确认（2026-10-05 18:43）**：
  - 主机 CTest 16/16 全部 PASS；
  - Product 固件编译成功（`main.bin` 7,614,596 B，构建产物见 `artifacts/product/20261005-182943-624`）；
  - **真机验收通过**：用户现场实机观察复核确认，主页天气卡片副标题中的缺字方块 `[]` 已彻底解决，动态汉字显示完整，排版无异常；
  - **当前待闭环项**：真实所在地天气同步联调（排查 Gadgetbridge 手机端天气数据源下发链路）。
