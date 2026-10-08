# WristFlow

以低功耗和交互体验为核心的智能手表原型。当前使用立创黄山派 SF32LB52-ULP 与 390×450 AMOLED（CO5300 / FT6146），已完成首轮显示/触摸实验；尚未接电池，以 USB 供电推进产品框架。实物准确版本和剩余物料见 [黄山派选型复评](docs/HARDWARE.md)。

## 新窗口入口

先读 [项目规则](AGENTS.md)，再由 [任务索引](PROJECT_INDEX.md) 定位所需章节。[STATUS 当前区](docs/STATUS.md#current) 是活动任务与下一步的唯一来源；[已确认目标](docs/BASELINE.md) 和 [工程地图](docs/ENGINEERING-MAP.md) 按任务读取。

操作前通过索引核对相关构建、验证与硬件条件；文档中的命令不构成执行授权。[flow v0.1 接入记录](docs/FLOW-PILOT.md) 说明采用版本与适配取舍。

日常使用 VS Code 可直接运行生成任务，见 [VS Code 操作说明](docs/VSCODE.md)。

项目托管于 [GitHub 仓库](https://github.com/e-n-ye/wristflow)，分支、PR 和云端构建流程见 [贡献与开发流程](CONTRIBUTING.md)。

`apps/product/` 是共享 UI 的日常产品入口，当前接入 RTC 和设置保存，见 [产品固件框架](docs/PRODUCT-RUNTIME.md)。`apps/ui_demo/` 保留固定数据回归，`apps/bringup/` 用于 [硬件诊断](docs/BRINGUP.md)。

SDK 与子模块以 [sdk.lock.json](sdk.lock.json) 为准，固件沿用官方 SCons。编译成功不代表烧录、显示、触摸、BLE 或低功耗实测通过。
