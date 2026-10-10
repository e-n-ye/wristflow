# 倒计时交互与分轮证据

2026-10-10 用户决定与五张 Redmi 实拍作为视觉 / 交互参考。参考照片不是实现证据，原附件保留在用户会话；实际渲染、源码和构建分开记录。活动项与唯一下一步见 [STATUS 当前区](STATUS.md#current)。

## 已确认的产品契约

- 单个任务，快捷预设 1/2/3/5/10/30 分钟点击直接开始；这组最新实拍覆盖先前 Q3 的 1/3/5/10/15/30。自定义三列时 / 分 / 秒循环滚轮，范围 1 秒至 23:59:59，点勾开始；00:00:00 禁用开始。
- 运行页大号 HH:MM:SS，向上取整显示剩余秒数。左键立即取消并回时长选择，右键暂停 / 继续；暂停时标明已暂停。离页不取消，左边缘右滑返回上级，KEY1 沿用既有导航；重新进入恢复同一任务。自定义页返回先回预设。
- 运行或暂停时表盘顶部显示蓝色沙漏，单击回到当前任务；取消 / 关闭后隐藏。沿用现有表盘，不用参考图替换整张表盘。
- 到期显示“计时结束”和本次原始时长；左键关闭，右键按原始时长重新开始。不得把暂停后的剩余时长当作重来时长。
- Product 到期应从息屏唤醒，勿扰也提醒；30 秒无操作息屏并保留待处理状态，下次唤醒仍可处理。关闭后恢复有效原会话，包含待答确认的有效性重查。USB-only，不接新物料，本阶段没有声音或振动。
- 重启取消活动倒计时；不保存活动任务，不做断电保时或跨重启恢复。

## 实施与所有权

| 层 | 入口 / 所有权 | 职责与边界 |
|---|---|---|
| 纯 C 模型 | [core/countdown.h](../core/countdown.h)、[实现](../core/countdown.c) | IDLE / RUNNING / PAUSED / EXPIRED，保存原时长、剩余量、单调时钟锚点和回调代次。到期转移与待交付标记分开；取消 / 暂停 / 继续拒绝旧代次回调，无 LVGL / RTOS / RTC 依赖 |
| XML 视觉 | [screen_countdown.xml](../ui/xml/screens/screen_countdown.xml)、[预设组件](../ui/xml/components/countdown_preset.xml)、[沙漏组件](../ui/xml/components/countdown_indicator.xml) | 布局、样式、控件名称与样例值；生成 C / 清单 / 字体由官方 Pro 导出，业务不写入生成物 |
| UI 适配 | [countdown_view.c](../ui/runtime/countdown_view.c) | 具名绑定同一官方页面；离页 detach 后销毁页面，回跳重建。演示保留本地模型；Product 通过 [countdown_port](../core/countdown_port.h) 发命令 / 读副本，250ms LVGL timer 仅更新视图，不驱动 Product 截止 |
| Product 服务 | [product_countdown.c](../apps/product/src/product_countdown.c) | 独占任务与一次性 RT 软 timer，全部命令 / 回调 / 副本共用 mutex；睡眠补偿的 RT tick 是权威时钟，最长 5 分钟分段等待，实际剩余量决定到期 |
| 提醒宿主 | [reminder_host.c](../ui/runtime/reminder_host.c)、[shell 接缝](../ui/runtime/ui_shell.c) | 一个高优先级覆盖槽；宿主只处理内容、输入与 30 秒显示活动，shell 保留原导航 / 显示策略并执行会话有效性检查，业务仍属于适配 / 服务 |
| 入口与生命周期 | [app_registry.c](../ui/runtime/app_registry.c)、[apps.c](../ui/runtime/apps.c)、[Product 组合](../apps/product/src/product_ui.c) | Product 提供有效后端后才开放菜单、shell 与工厂；未注入后端的视觉构造仍阻断。surface 追加在旧值之后，编辑器类别比较有明确上下界 |

单调时钟单位为毫秒，32 位无符号相减覆盖一次回绕；活动任务必须在一个完整时钟周期内被观察。第一轮验证暂停的亚秒精度、最大时长和一次回绕，不给硬件时间误差承诺。第二轮保持相同领域模型，但 Product 服务及独立调度源拥有任务；不能把演示 LVGL timer 当作系统后台闭环。

<a id="round1"></a>
## 第一轮验证（2026-10-10）

基线 `56b4ceb`（入口修正 PR #40 已合入）；候选源码用 [精简证据](evidence/2026-10-10/countdown-round1.json) 的文件哈希绑定，详细产物留在本工作树 `D:/MY_Desk/project/wristflow/artifacts/`，不提交长日志或参考照片。

- 官方 LVGL Pro Editor 2.0.1 Community，预览运行时和锁定 SDK 均为 LVGL 9.4.0。通过 GUI `Ctrl+B` 导出并编译，日志为 `Built runtime for Preview` / `Project compiled successfully`；不是自制转换器，也不是仅检查 XML 格式。
- 主机 `pwsh -NoProfile -File scripts/Simulate.ps1 -BuildOnly -Jobs 4`：最终 CTest **20/20**，0 失败，10.59 秒。新增 `countdown` 覆盖非法 / 最大输入、暂停继续、迟到到期、旧代次、重复消费、取消、重来与时钟回绕；`countdown_ui` 使用实际 LVGL 指针输入验证六预设、真实滚轮拖动、零时长禁用、最大输入、返回重建、离页继续、沙漏单击、到期一次及原时长重来、销毁 timer 回收和 Product 阻断。
- 官方生成 C 由同一主机 LVGL 渲染八份 390×450 快照，覆盖预设、自定义 / 最大输入、运行、暂停、表盘运行 / 暂停、到期；截图仅使用样例时间与演示快照。实际几何断言检查滚轮与冒号中心、居中样式、沙漏不覆盖电池区，字体断言覆盖新增中文与六个控制图标。最终六页拼图在本机 `artifacts/countdown/preview.png`，原始 PPM 在 `artifacts/simulator-build/renders/countdown_*.ppm`。
- 构建命令分别为 `pwsh -NoProfile -File scripts/Build.ps1 -Example ui_demo -Jobs 4` 和 `... -Example product -Jobs 4`。两目标的退出码、三镜像、map / 源码哈希及工具版本见精简证据和对应本地 `result.json`；构建脚本不烧录。
- 整项目重导出给六份未改 XML 的天气生成物追加了 EOF 空行；逐路径恢复先前官方生成文件，未手改生成逻辑。精简证据保留编译时 / 恢复后哈希，并验证差异仅为该空行及 Git 换行规则，不据此重跑无关功能测试。
- 文档验证覆盖八份 Markdown 的 173 个相对链接 / 锚点及本轮 43 份文本的严格 UTF-8。新生成的三个 C 文件沿用官方末尾空行；默认 `git diff --cached --check` 仅报告这三处，`git -c core.whitespace=-blank-at-eof diff --cached --check` 通过。未手工格式化生成文件。
- 本轮没有串口、下载、设备输入、BLE 或电流测量；硬件验收为 **未执行**。演示离页到期通过不证明 Product 息屏到期、系统提醒或有效会话恢复。

## 本轮失败与已核实原因

| 现象 | 证据与处置 |
|---|---|
| Pro 解析预设组件失败 | `style_line_width:items` 等属性不被本次 GUI 接受；改成具名 style 与控件内 selector 后导出成功，不手改生成 C |
| 滚轮只有一行，区域拖动无效 | 生成 C 先计算 `visible_row_count`，后设置字体 / 行距。调整 XML 属性次序、显式居中，最终导出顺序、三行渲染与指针拖动通过 |
| 图标 / 中文缺字，初次主机 18/20 | 源 TTF 是裁剪子集，新码点不在 cmap；补源裁剪清单与静态 TTF，再官方导出，最终新增字形与全部 20 项通过。`--static-fonts-only` 不改通知 TTF 或图片 |
| `UNKNOWN ... open '.../file_list_gen.cmake'` | 部分生成物已更新，保留清单备份；具体文件独占读写打开检查成功，重试后官方编译成功。根因未知，没有证据认定权限、杀软或占用；不批量清理 |
| UI Demo 固件编译时报 `open` 类型冲突 | 新事件回调与 SDK / Newlib 文件接口同名，主机环境未暴露；改名 `open_countdown` 后 UI Demo 编译成功。与 XML 解析问题分开记录 |

可复现的 XML 排错经验已同步用户本机 `lvgl-xml-visual` skill；skill 不属于本仓库产物。字体生成沿用源许可证与更名，方法见 [fonts README](../ui/xml/fonts/README.md)。

## 后续轮的验收契约

第二轮限定为 Product 服务持有任务、独立后台截止与事件重建、息屏到期、30 秒待处理提醒、有效会话 / 确认恢复及重启取消；达成后才开放 Product 入口。第三轮绑定同一固件做 USB 真机显示 / 输入 / 到期与恢复验收。第一轮交付不关闭 [D5 / N5](ARCHITECTURE-REVIEW.md#3-债务清单全部未关闭)，不完成稳定 ID / 模块裁剪框架，也不自动启动 C2、通知联合验收或功耗测量。执行顺序和授权只见 STATUS 当前区。

<a id="round2"></a>
## 第二轮服务与提醒（2026-10-10）

从第一轮合并提交 `e6e58be` 进入 `codex/countdown-product`，工作树为 `C:/Users/13984/.codex/worktrees/countdown-product/wristflow`。本轮沿用已导出的 XML / C / 字体，没有新 XML 修改或 Pro 导出；主目录后续 Pro 生成改动保留。实际产物与日志留该工作树 `artifacts/`，源码 / 测试 / 构建绑定见 [精简证据](evidence/2026-10-10/countdown-round2.json)。

- **时钟与调度源码事实**：锁定 SDK 的 `lv_drivers_v9/lvgl_drv.c` 注册 `rt_tick_get_millisecond()`；1000 Hz 下与服务的 `rt_tick_get()` 同源，停 LVGL handler 不停时钟。SDK PM 醒后补偿 RT tick；软 timer 线程的等待进入内核最近截止。当前 SDK `drv_hwtimer.c` 的 `timeout * timer->freq` 是 32 位乘法，因此单次等待限定 300000 tick（8192 Hz 时乘积 2457600000），每段重新检查任务；不改 SDK、不把 24 小时任务一次送入 LPTIM。
- **同步与交付**：软回调运行在线程上下文，只做锁内状态转移 / 重排与锁外 `WF_EVENT_COUNTDOWN`。`stop` 不能撤销已选中的回调，所以回调依据当前任务 / 截止，提前则重排、取消 / 暂停则无动作；不能读取“当前 generation”冒充旧回调身份。getter 只复制状态，不推进到期。事件位可合并，主线程每次息屏事件醒来从服务副本重建；提醒成功挂载后才 ACK 待交付标记。ACK 后提醒宿主继续持有待处理状态，不因旧事件再亮屏。
- **显示与会话**：提醒使用顶层完整覆盖，勿扰不抑制；30 秒无输入息屏，KEY1 仅重新展示同一提醒，不导航或代答。普通输入与通知展示被覆盖拦截，通知缓存仍更新且不新增补播队列。原 screen、导航、编辑 draft 和秒表模型保留；组件顶层确认仅隐藏，设置 / 秒表确认被覆盖。关闭 / 重来按原始 off 时刻或覆盖前活动 / 持续亮屏的逻辑截止检查 120 秒，普通失效页回表盘并取消确认，编辑 / 前台计时保留豁免。恢复确认仍沿既有业务能力 / 目标检查；本轮未建立通用确认请求框架或多提醒队列。
- **取消边界**：“立即取消”也覆盖截止交界：即使回调已到期、运行页的点击尚未派发，明确取消仍清任务 / pending，旧回调与旧事件不能复活。关闭只处理到期任务；重来使用原时长。服务不访问设置 / FlashDB，启动为 IDLE，重启不恢复任务。
- **主机证据**：最终 CTest **22/22**，0 失败，27.60 秒。`product_countdown` 直接编译实际服务，验证 getter 不驱动到期、一次交付、锁 / callback 分界、两种截止取消竞争、亚秒暂停、重来、最大 86399 秒的 288 段等待与 RT tick 回绕；`product_countdown_ui` 将 RT callback 与 LVGL handler 分开，实际停止全部 LVGL timer 后仍收到到期，验证勿扰、30 秒保留 / KEY1、原长睡时刻、有效设置确认与失效取消、顶层 draft 确认、秒表继续 / 退出请求、期间通知缓存及原时长重来。其余回归同时通过。
- **实际渲染**：三份 390×450 LVGL 合成帧分别在息屏 / 勿扰、设置确认和编辑草稿期间展示同一官方到期页面，未露出下层确认或缺字。PPM 在 `artifacts/simulator-build/renders/countdown_product_*.ppm`，拼图在 `artifacts/countdown/product-reminders.png`；使用未校时快照，时钟 `--:--` 是有效占位。
- **固件与记录**：忽略目录的 `build-isolated.ps1` 复制当前 Build 记录 / 验证流程，只把环境激活接到已有主目录 SDK，随后恢复本工作树 ProjectRoot；两目标仍由官方离线 check / export 与 `scons --board=sf32lb52-lchspi-ulp -j4` 构建，未重装依赖。最终 Product `artifacts/product/20261010-185503-451/result.json` 和 UI Demo `artifacts/ui_demo/20261010-185556-201/result.json` 退出 0、产物验证通过；主 BIN 分别 7637244 / 7439512 B，330 / 315 个工程源与全部记录产物重哈希一致。最终配置为 1000 Hz、软 timer、Deep 与 LPTIM1，map 含服务 / 宿主；工具版本、三镜像地址 / 哈希及 map / 配置哈希见精简证据。保留 SDK 原有告警，不给硬件运行结论。
- **文档检查**：本轮 32 份文本严格 UTF-8、8 份 Markdown 的 180 个相对链接 / 锚点和 diff 空白检查通过。22/22 全量回归后，只新增 Product PM 的 COUNTDOWN / KEY / PHONE 合并邮箱断言，受影响 `product_pm` 1/1、0.07 秒通过；固件源码未再变化。
- **失败记录**：最初新测试在尚亮屏时停 LVGL，KEY1 按普通导航返回，修正为先自然息屏；随后两处测试绕过既有导航 / 卡片入口，修正为经菜单打开设置、滑到组件页后长按。源码独立审查另发现截止交界取消被拒绝，已修正并补回归；没有把测试前提错误当作设备故障。

固件编译与镜像重哈希结果在精简证据及本轮 `result.json` 单列；没有串口、烧录、设备输入或电流测量。独立 callback 与主机恢复通过不证明板上自主唤醒、精度、并发压力、真实触摸释放或功耗。第三轮同版 USB 验收仍独立；C2 / 通知联合验收、完整裁剪、公共确认、多提醒冲突与 RTC / 断电恢复不随本轮启动。
