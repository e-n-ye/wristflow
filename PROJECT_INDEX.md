# 按任务读取

先完整理解 [AGENTS](AGENTS.md) 及适用规则，再选本次需要的一行；索引只指路，不保存进度。接续时核对工作树、HEAD 和已有改动，不遍历全部链接。

| 当前需要 | 读取位置 |
|---|---|
| 接续、找活动任务或下一步 | [STATUS 当前区](docs/STATUS.md#current)，历史区按需 |
| 核对目标、范围、预算和既有决定 | [BASELINE](docs/BASELINE.md) 对应主题；长期方向见 [架构 §1–2](docs/ARCHITECTURE.md#1-目的输入与适用范围) |
| 安排阶段、判断依赖和停止条件 | [架构任务卡 §4](docs/ARCHITECTURE-REVIEW.md#4-后续任务卡与顺序)；同版矩阵见 [Product 验收](docs/PRODUCT-ACCEPTANCE.md)，活动项回 STATUS |
| 找工程、配置、手写与生成边界 | [工程地图](docs/ENGINEERING-MAP.md)；术语按需查 [CONTEXT](CONTEXT.md) |
| 环境与 SCons 固件构建 | [BUILD 当前机器入口](docs/BUILD.md#当前机器直接使用)、[SDK 锁](sdk.lock.json)；已有环境不重装 |
| 主机测试与模拟器 | [SIMULATOR](docs/SIMULATOR.md)、[测试工程](tests/CMakeLists.txt) |
| UI / XML / 字体资源 | [UI-DEMO](docs/UI-DEMO.md)、[UI 交互契约](docs/UI-INTERACTION.md)；先由地图确认源与输出，再定位对应专题 |
| BLE、PM 或产品集成 | [产品入口](docs/PRODUCT-RUNTIME.md)、[BLE](docs/BLE-FIRST-LINK.md)、[PM](docs/PM-EVENT-WAKE.md)，仅选相关域 |
| 架构、裁剪或风险核查 | [ARCHITECTURE](docs/ARCHITECTURE.md) 相关契约与 [审查债务 §3](docs/ARCHITECTURE-REVIEW.md#3-债务清单全部未关闭)，先核对证据提交 |
| 下载、真机、功耗 | [HARDWARE](docs/HARDWARE.md) → [FLASHING](docs/FLASHING.md) / [Product 验收](docs/PRODUCT-ACCEPTANCE.md)；步骤不构成授权 |
| 追溯 N0 审计与旧返工 | [首轮验收](docs/handoffs/N0-CODEX-ACCEPTANCE.md)、[旧任务契约](docs/tasks/N0-VERSION-AUDIT.md)；固定历史 SHA 不替代当前核对 |
| 决定记录写到哪里、整理文档 | [职责与维护](docs/DOCUMENTATION-WORKFLOW.md#responsibilities) |
| 分支、审阅、PR 与合并 | [CONTRIBUTING](CONTRIBUTING.md)；纯文档只做文档验证，CI 沿用手动基线 |
| 了解 flow 试点版本与取舍 | [接入记录](docs/FLOW-PILOT.md)，无需到 flow 通读资料 |

工具入口（含 [VS Code](docs/VSCODE.md)）遵循同一 AGENTS / 索引 / STATUS，不维护另一份任务状态。资料不足、冲突或依赖核对才扩大阅读。
