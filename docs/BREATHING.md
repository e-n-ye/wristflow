# 呼吸训练

## 已确认契约（2026-10-11，Q1–Q12）

用户参考照片决定视觉，目标 390×450。菜单“呼吸训练”，主页“呼吸”。时长无限滚轮选择 1–5 分钟，首次 1 分钟，勾确认、返回放弃；记住确认值，跨重启保存在第二轮接入。

每分钟 7 个完整吸呼循环，吸呼等长，无屏息，开始直接吸气。圆环吸气放大、呼气缩小。动画与“结束／重新开始”两页左右切换，切页继续计时。“结束”回主页，不产生结果；“重新开始”立即清零，沿用时长，从吸气开始并回动画页。仅自然完成进入结果，训练前／后心率均为 `--`，显示训练时长；“完成”回主页。没有训练历史或模拟 PPG 值。

训练期间按用户亮度常亮，其他页面普通息屏。主动离开先确认，确认期间暂停；取消从吸气继续，确认停止并去目标。离页／重启不恢复运行中的训练。普通消息只入库、不弹提示；闹钟／倒计时覆盖期间暂停，全部提醒关闭后从吸气继续，暂停时间不计入训练。

## 三轮边界

1. 官方 XML／独立模型、UI Demo／主机交互动画、官方预览导出、Product 与 UI Demo 编译、审阅合入。Product 训练入口保持 gated。时长只在同一 Demo 会话内记忆；模型支持暂停／恢复，电源、Flash、通知／提醒联动未接线。
2. Product 持久保存与前台生命周期：有拥有者和期限的亮屏申请、退出确认、普通消息抑制、提醒暂停与恢复。不能覆盖用户全局常亮；不把前台呼吸引导作为后台倒计时服务。
3. 同版 USB 验收。没有本轮设备操作；编译和主机像素不能证明设备触摸、亮屏、PPG、功耗或续航。

## 第一轮记录

基线 `f250eb9ed3dd070371eb867a4b4fcf25b5a8b1c8`（闹钟 PR #46 已合入）。隔离工作树 `breathing-ui/wristflow`，分支 `codex/breathing-ui`；主目录 Pro 改动保留。SDK／子模块与工具锁不变。唯一活动下一步见 [STATUS](STATUS.md#current)。

### 实现与所有权

- `core/breathing.c/.h` 使用调用者提供的 uint32 单调 tick，支持回绕、活动时长截止、暂停和从吸气恢复。节拍使用有理数位置 `rhythm_ms * 7 % 60000`，避免每半周期取整造成累计漂移；没有 RTC、持久存储或 LVGL 依赖。
- `ui/runtime/breathing_view.c/.h` 持有五个页面状态、时长草稿和 33ms 刷新 timer；圆环以 smoothstep 在 140–220px 之间缩放，中心来自官方生成对象的几何。`apps` 管实例与屏幕生命周期，`ui_shell` 管导航和退出目的地；重复请求保留首个目的地，已自然到期但 timer 尚未采样时，本次退出请求先呈现结果。
- XML 有六个组件和一个 screen，使用官方内嵌字体。菜单枚举追加在既有应用之后，保留旧整数身份。Demo 创建实例，Product 菜单隐藏且 API 拒绝打开；此隔离不表示生成资源已从 Product 二进制裁剪。
- 时长确认值仅随同一 Shell 实例记忆；手动结束、重启、离页销毁均按契约处理。实际保存、训练亮屏、消息抑制及提醒暂停接线留第二轮。

### 验证方法

主机配置 `cmake -S tests -B artifacts/breathing/host -G Ninja -DCMAKE_BUILD_TYPE=Debug`，使用已有 UCRT64 GCC／Ninja，并用 `-DWRISTFLOW_SDK_ROOT=<已锁定 SDK checkout>` 复用依赖。构建 `cmake --build artifacts/breathing/host -j 6`，执行 `ctest --test-dir artifacts/breathing/host --output-on-failure`；UCRT64 工具目录置于当前进程 PATH，使用同一 CMake 安装的 CTest。

独立模型另以 `gcc -std=c11 -Wall -Wextra -Werror -I core core/breathing.c tests/breathing.c` 编译运行。UI 用例使用 LVGL 指针输入和实际绘制，覆盖无限滚轮、草稿取消、1–5 分钟、横划、边缘返回、重新开始、自然截止、退出确认暂停、目的地保留、销毁回收和 Product gate；真实像素以 `lv_snapshot_take` 保存。

固定 SDK 由官方环境管理器离线检查与激活，再在对应 `apps/<target>/project` 执行 `scons --board=sf32lb52-lchspi-ulp -j6`。本机忽略目录的隔离包装器只调整项目路径，复用既有锁定 SDK；不修改共享环境配置或烧录。Product 的 Base64 路径改从官方 `SIFLI_SDK` 根解析，避免隔离工作树空子模块路径。

### 最终结果与证据边界

Pro **2.0.1**、预览与固定 SDK **LVGL 9.4.0**，官方 `Ctrl+B` 导出并编译，回读 `Project compiled successfully`。主页、时长、动画、操作及结果五页均在 100% 核对并保存窗口截图；临时单页预览已恢复为默认主页，最终生成 C 的其余四个 panel 为 hidden。只把换行和末尾空行等价的无关生成噪声逐路径恢复，没有手改生成 C。

最终 CMake **3.31.4**／UCRT64 GCC **15.2.0**／Ninja Debug 主机 **31/31** 通过（15.48 秒），其中呼吸 UI 为 2.45 秒；严格 core 编译运行通过。九张真实 LVGL 快照覆盖不同状态，最大圆环与引导文字至少留 18px，尺寸奇偶变化的中心偏差不超过半像素。

![第一轮真实 LVGL 页面与退出确认](evidence/2026-10-11/breathing-round1.png)

上图来自最终主机绘制，时间为样例 `13:40`，心率无输入显示 `--`。截图不是设备实拍。完整精简哈希与检查结果见[第一轮证据](evidence/2026-10-11/breathing-round1.json)。原始截图、日志和结果文件位于本机 `breathing-ui/wristflow` 的忽略目录，不是仓库内可下载附件。

| 目标 | 本机记录 | 主 BIN 字节 | SHA-256 |
|---|---|---:|---|
| Product | `artifacts/product/20261011-222319-329/result.json` | 7,710,436 | `25691e73e585c382a3378f56fb86bf781e7d57e9270493ad7857afc67cc376ca` |
| UI Demo | `artifacts/ui_demo/20261011-222515-154/result.json` | 7,507,392 | `f75e9935231929b8a438f3b2a41e53825133ec27a7c147e8731db88a0af05bd6` |

两目标使用 SDK Python 3.13.15、SCons 4.10.1、Arm GCC 14.2.1，均退出 0、镜像清单与产物校验通过。Product 383 份源码、UI Demo 366 份源码及各自产物逐项重哈希匹配；官方清单 106 份 C 全在两端实际编译数据库中，旧 `hour_seven_data.c` 未编译，新增 `guide_40_data` 与使用的字体在链接地址中确认。固定 SDK／子模块保持锁定且干净；没有手动云端基线或设备操作。

已处理失败保留：首次 Product 编译因空工作树 vendor 路径找不到 Base64 头文件；一次主机构建与生成文件恢复重叠而短暂缺头文件，恢复后重新构建通过；Pro 曾停在 `Generating code` 并刷新黑屏，重启既有编辑器后恢复，根因未定。操作页的“束”字导出清单漏项、最大圆环与引导文字间距也已通过官方预览／主机绘制发现并修正。失败日志与旧成功构建保留，以上最终记录覆盖布局修订前的验证版本。

源码独立审查未发现新的计时／导航／生命周期缺陷；PR 最新 head 的审阅、Rebase 合入及 fetch 核对由主代理收尾，回读记录放在 PR Closure 或最终交接。第二轮与第三轮仍未实施，不能由第一轮编译推断 Product 保存、设备常亮、提醒恢复、PPG、功耗或续航通过。

提交检查：57 个改动文件 UTF-8 与 224 个本地链接／锚点通过。默认 staged `git diff --check` 仅报告七份新增官方生成 C 的 EOF 空行；遵守生成物所有权，保留该输出。`git -c core.whitespace=-blank-at-eof diff --cached --check` 通过，其余空白错误仍检查，未手改生成文件消除该格式差异。
