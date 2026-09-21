# WristFlow

以低功耗和交互体验为核心的智能手表原型。目标为 SF32LB52-DevKit-Nano-R16N16 与 390×450 AMOLED（CO5300 / FT6146）。用户于 2026-09-21 确认已下单，尚未到货；实际 SKU、附件和版本仍待核对。

## 新窗口入口

1. 阅读 [当前状态与下一步](docs/STATUS.md)。
2. 阅读 [共识及本轮变更](docs/BASELINE.md)，需要细节时读取 `docs/reference/` 三份原始快照。
3. 按 [构建说明](docs/BUILD.md) 复现，按 [到货验收](docs/ACCEPTANCE.md) 收集真机证据。

日常使用 VS Code 可直接运行生成任务，见 [VS Code 操作说明](docs/VSCODE.md)。

项目托管于 [私有 GitHub 仓库](https://github.com/e-n-ye/wristflow)，分支、PR 和云端构建流程见 [贡献与开发流程](CONTRIBUTING.md)。

等待到货期间的独立应用为 `apps/bringup/`，显示、触摸与按键休眠实验见 [上板基础固件](docs/BRINGUP.md)。

SDK 固定在 v2.5.1；先完成官方示例构建，再扩展应用。编译成功不代表烧录、显示、触摸、BLE 或低功耗实测通过。
