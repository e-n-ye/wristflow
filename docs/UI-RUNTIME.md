# UI 框架的第一步

2026-09-25 后续增量已接入白灰资源、蜂窝菜单、秒表、长按选表盘和亮度等，见 [菜单与应用](UI-APPS.md)。本页保留 2026-09-24 第一阶段的结构与验证记录。

日期：2026-09-24。用户决定先打通整体框架，视觉细节逐步调整；避免表盘、导航和业务数据互相依赖。此增量建立可运行的边界，尚不是完整产品框架。

## 保留的产品决定

- 应用菜单参考官方 Watch 的蜂窝菜单，其余界面参考 Redmi Watch 4。
- 首版打磨一个白灰扩散表盘。小时的两位数字作为整体，两侧毛玻璃不倾斜，外层与内层宽度约 1:2。整套系统不强制黑白。
- 将来长按主页进入表盘选择；换表盘只改变主页。当前扩散表盘没有可配信息，其他表盘以后可以提供配置。
- 动画动作和触发时机尚未决定。Monica 参考的是开机动画过程及最终静止画面，不据此规定每次唤醒播放。

白灰资源仍在独立设计实验中。本次继续使用已导出的旧资源进行结构与交互回归，不把框架截图当作白灰或最新 1:2 效果的验收。

## 当前边界

| 位置 | 负责 | 依赖约束 |
|---|---|---|
| `core/watch_core.*` | 已提交页索引、主页/控制中心状态、时间与电量快照校验 | 纯 C；不含 LVGL、RTOS、硬件和堆分配 |
| `ui/runtime/ui_shell.*` | 页面创建/退出、拖拽吸附、循环镜像、手势转换、切屏动画、表盘实例 | 只通过表盘描述符操作表盘；不知道数字或分钟控件名 |
| `ui/runtime/watchface.h` | 稳定 ID，创建、更新、可见性和销毁回调 | 每个镜像独立实例；数据来自调用方 |
| `ui/runtime/default_watchface.c` | 当前表盘资源及控件适配 | 数字/电量控件依赖留在该适配器和原 presenter 内 |
| `ui/xml/` | 外观、资源、官方生成 C | 本轮未改；不承载业务导航或传感器读取 |
| `apps/ui_demo/src/ui_demo.c` | 选择表盘、卡片和控制中心，提供样例快照 | PC 与板端共用的组装入口 |

卡片工厂数量由组装入口指定，core 不写死五页。当前演示保留四张信息示例页，其中整页组件仅为演示；产品仍为表盘加三张核心卡片。

## 生命周期与数据

所有 shell 调用和回调在 LVGL 线程执行。当前面向单个 390×450 显示器，由 shell 独占其页面切换；未来 RTC、BLE 或传感器线程先传递消息，再由 UI 线程提交快照，不直接操作控件。

1. `wristflow_ui_shell_create` 挂载现有页面并激活主页。旧活动屏幕仍归调用方，演示入口在成功后删除它。
2. `wristflow_ui_shell_update` 验证并复制快照，然后更新全部表盘实例，包括隐藏实例。非法输入不覆盖上一次有效值。当前卡片仍是样例数据，快照暂只绑定表盘。
3. `set_visible` 表示当前是否允许执行表盘动画。离开可见横向区域或进入切屏过渡时停用；回到主页并完成过渡后恢复。隐藏的更新回调只改显示数据，不启动动画。
4. `wristflow_ui_shell_set_watchface` 先创建所有新镜像，再提交替换。任意工厂返回空指针，清理已创建的新实例，保留旧表盘与导航状态。不会重建卡片或控制中心。
5. `wristflow_ui_shell_destroy` 停用表盘、结束待处理切屏、释放表盘外部资源与全部页面；显示器留下一个活动空屏，归调用方或 LVGL 最终反初始化管理。

描述符必须比 shell 活得久；回调不能重入 shell API，也不能保留快照指针。`destroy` 释放计时器、订阅、私有上下文，随后 shell 删除控件树。工厂返回失败前须清理自身未完成工作。LVGL 自身内存分配仍按其断言策略处理，本接口不承诺在整个 LVGL 堆耗尽时恢复。

换表盘会短暂同时持有新旧两套表盘控件，这是失败后保留旧视图的代价；图片/字体仍共享静态资源。峰值内存需上板测量，不能由主机测试推断。

## 本次验证

- 主机编译及 5/5 CTest 通过：`clock_format`、`core_navigation`、`ui_preview`、`ui_navigation`、`ui_shell_lifecycle`。
- 原始指针输入回归覆盖横向跟手、短拖取消、半页提交、动量、双向循环、固定分页点、控制中心反复进出和滑块。
- 结构独立性测试用单个文字控件及私有定时器替换数字图片表盘；覆盖隐藏更新、可见性、镜像一致、第二实例创建失败回滚、控制中心打开时替换、滑块值和卡片对象保留、过渡中销毁、无卡片与连续 12 次重建。
- 实际 LVGL 快照已检查：`artifacts/simulator-build/renders/shell-regression.png`。它验证旧页面外观和导航保留，不验证新设计效果。
- 主机日志：`artifacts/simulator-build/Testing/Temporary/LastTest.log`；本轮主机 GCC 15.2.0、固定 SDK 的 LVGL 9.4.0。
- 固件 UI Demo、Hello、BLE、Bringup 四目标编译链接和产物检查全部通过。Python 3.13.15、SCons 4.10.1、Arm GNU 14.2.Rel1/GCC 14.2.1。SDK 及其锁定子模块工作树保持干净。

| 目标 | 本工作区记录 | main.bin 字节 | SHA-256 |
|---|---|---:|---|
| UI Demo | `artifacts/ui_demo/20260924-203944-956/` | 3027836 | `d41372e2a1f0929f8c4dc70a39f49551f71c9ad1e74103a9869365d317573cc5` |
| Hello | `artifacts/hello/20260924-204220-572/` | 300336 | `5d1e396f2e9cd6edd35f0586d2c30dbd7f3dd4cf0f42a5bcf17c9a976e4173c0` |
| BLE | `artifacts/ble/20260924-204230-879/` | 490064 | `09d17264a1822a45731664091785dea1b0a0324f5be2af46f32c154500d396d2` |
| Bringup | `artifacts/bringup/20260924-204246-695/` | 614176 | `83c35613de81c929074e04415d488917c5a439775bb19ea3bfe008ecee8c3978` |

Hello/BLE 复用原 SDK 目录增量构建；Bringup/UI Demo 在本工作区构建。Bringup 哈希与旧路径记录不同，未承诺跨路径逐字节一致。SDK 的 RWX LOAD segment、`lv_obj_tree.c` 潜在空指针等既有告警保留，未修改 SDK。

## 隔离工作区构建

本轮分支 `codex/ui-shell` 从 `main` 建立在 `C:/Users/13984/.codex/worktrees/ui-shell/wristflow`，保留原工作区未提交的到货/烧录工具修改和独立设计实验。

SDK 及工具版本不变。本机空 SDK 目录移到忽略的 `artifacts/uninitialized-sdk`，原位置使用 Windows junction 复用原工作区固定 SDK。主机执行：

```text
cmake -S tests -B artifacts/simulator-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe -DCMAKE_MAKE_PROGRAM=D:/msys64/ucrt64/bin/ninja.exe -DWRISTFLOW_BUILD_SIMULATOR=ON
cmake --build artifacts/simulator-build --parallel 6
ctest --test-dir artifacts/simulator-build --output-on-failure
```

官方环境状态按绝对 SDK 路径登记。为复用已有工具，本机忽略目录的 `artifacts/build-isolated.ps1` 加载原工作区 `Set-LocalEnvironment.ps1`，执行锁校验与官方离线 check/export，随后在本分支工程目录执行官方 `scons --board=sf32lb52-lchspi-ulp -j6`，检查 ELF/配置/三种镜像并记录源文件哈希。不修改 SDK 或环境注册，不执行下载脚本。标准独立 checkout 与 CI 仍使用 `scripts/Build.ps1`。

首次 Bash 启动固件构建时使用了不存在的 `C:/Program Files/PowerShell/7/pwsh.exe`，退出 127，尚未进入构建；改用 PATH 中的已安装 `pwsh` 继续。记录该失败，避免误判为工具链或源码故障。

## 下一项有界实验

接入一个蜂窝菜单页面和一个最小应用入口，验证“主页 → 菜单 → 应用 → 返回菜单 → 回主页”的完整路径，菜单位置应能恢复。先提取官方菜单的布局/拖动实现，导航命令交给 shell/core；不迁入官方 Watch 的整个应用框架。KEY1 在这一步增加输入适配。

接着实现长按表盘选择与稳定 ID 保存，再接卡片配置和真实数据。通知、计时器、设置按实际页面逐项加入路由。电源策略、通知中断优先级和服务生命周期仍须单独实现与验证。

暂不建立插件装载器、通用事件总线、任意页面栈、万能属性表或磁盘配置模式。已有两个实现确实出现重复时，再抽取公共部分。`mount_screen` 当前只迁移生成页面的子树与已有根背景/文字样式；未来根布局或根事件改变时须复核该适配，不能假设任意 screen 都能无损重挂。

本轮没有烧录或新增硬件验证。用户此前确认的黄山派基础显示、触摸、按键及官方蜂窝菜单体验，不等于本分支固件已真机通过，也不证明 BLE、休眠电流或续航。
