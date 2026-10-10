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

| 层 | 入口 / 所有权 | 本轮边界 |
|---|---|---|
| 纯 C 模型 | [core/countdown.h](../core/countdown.h)、[实现](../core/countdown.c) | IDLE / RUNNING / PAUSED / EXPIRED，保存原时长、剩余量、单调时钟锚点和回调代次。到期转移与待交付标记分开；取消 / 暂停 / 继续拒绝旧代次回调，无 LVGL / RTOS / RTC 依赖 |
| XML 视觉 | [screen_countdown.xml](../ui/xml/screens/screen_countdown.xml)、[预设组件](../ui/xml/components/countdown_preset.xml)、[沙漏组件](../ui/xml/components/countdown_indicator.xml) | 布局、样式、控件名称与样例值；生成 C / 清单 / 字体由官方 Pro 导出，业务不写入生成物 |
| 演示适配 | [countdown_view.c](../ui/runtime/countdown_view.c) | 持有模型并具名绑定生成页；离页 detach 后销毁页面，模型与演示 timer 留在 adapter，回跳重建。250ms LVGL timer 只在运行或到期待交付时活跃；这不是 Product 息屏调度方案 |
| 入口与生命周期 | [app_registry.c](../ui/runtime/app_registry.c)、[apps.c](../ui/runtime/apps.c)、[ui_shell.c](../ui/runtime/ui_shell.c) | 新 descriptor 为 `demo_only`；演示菜单可见，Product 菜单、shell open 和页面工厂均阻断，不创建任务 / timer / 沙漏。surface 追加在旧值之后，编辑器类别比较补上界 |

单调时钟单位为毫秒，32 位无符号相减覆盖一次回绕；活动任务必须在一个完整时钟周期内被观察。第一轮验证暂停的亚秒精度、最大时长和一次回绕，不给硬件时间误差承诺。Product 后续轮须选择可跨息屏运行的权威时钟与独立调度源，保留相同领域模型；不得把 LVGL timer 接入后当作系统后台闭环。

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
