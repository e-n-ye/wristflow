# 按任务读取

先完整理解 [AGENTS](AGENTS.md) 及适用规则，再选本次需要的一行；索引只指路，不保存进度。接续时核对工作树、HEAD 和已有改动，不遍历全部链接。

| 当前需要 | 读取位置 |
|---|---|
| 接续、找活动任务或下一步 | [STATUS 当前区](docs/STATUS.md#current)，历史区按需 |
| 核对目标、范围、预算和既有决定 | [BASELINE](docs/BASELINE.md) 对应主题；长期方向见 [架构 §1–2](docs/ARCHITECTURE.md#1-目的输入与适用范围) |
| 安排阶段、判断依赖和停止条件 | [架构任务卡 §4](docs/ARCHITECTURE-REVIEW.md#4-后续任务卡与顺序)；同版矩阵见 [Product 验收](docs/PRODUCT-ACCEPTANCE.md)，活动项回 STATUS |
| 10月25日用户优化观察、后续顺序及健康硬件前置 | [本轮静态契约](docs/ARCHITECTURE-REVIEW.md#followup-20261025) → [健康前置](docs/HARDWARE-CAPABILITIES.md#health-prereqs-20261025)；只记录与依赖，实施授权和当前选择回STATUS |
| 找工程、配置、手写与生成边界 | [工程地图](docs/ENGINEERING-MAP.md)；术语按需查 [CONTEXT](CONTEXT.md) |
| 选择硬件内置能力、驱动或自研算法 | [硬件能力选择与验证](docs/HARDWARE-CAPABILITIES.md)；型号／接线回 HARDWARE，活动项回 STATUS |
| 板载光感／地磁／音频及TF、KEY2、RGB的开发顺序与采购准备 | [板载资源规划](docs/BOARD-RESOURCES.md)；逐器件硬件跑通→页面→功能，实际条件见 HARDWARE，活动项仍回 STATUS |
| LTR-303双通道识别、限时读数、停止恢复 | [LTR专题](docs/LTR303.md)；原值／lux分开，复用传感器worker／I2C3，设备条件回 HARDWARE，活动项回 STATUS |
| MMC5603NJ身份、原生自检、限时XYZ与停止基线 | [地磁专题](docs/MMC5603.md)；原始counts／航向分开，只写控制不冒充配置读回，复用唯一worker／I2C3，活动项回STATUS |
| 环境与 SCons 固件构建 | [BUILD 当前机器入口](docs/BUILD.md#当前机器直接使用)、[SDK 锁](sdk.lock.json)；已有环境不重装 |
| 主机测试与模拟器 | [SIMULATOR](docs/SIMULATOR.md)、[测试工程](tests/CMakeLists.txt) |
| 复杂人机协作验收、多步动作／等待／采集／清理，以及电量回放与记录 | [统一协作测试流程](docs/HARDWARE-TEST-WORKFLOW.md#shared-operator-workflow)；优先评估复用协作页／阶段日志并增加专用适配，先模拟验交互；实机核HARDWARE与授权，活动项回STATUS |
| 电脑端实验脚本复用、前后快照／启动检查、简单状态查询／被动采集、模拟／历史UART回放及接续进度 | [本地工具选择与职责](docs/HARDWARE-TEST-WORKFLOW.md#local-experiment-tools) → [工程地图](docs/ENGINEERING-MAP.md)；先选已有入口／参数，协议与清理差异才新增适配；历史artifacts只作证据／快照，不作新工具代码依赖 |
| 实验前核现有构建／传输记录、显式证据哈希或改动文档 | [公共检查与重验条件](docs/HARDWARE-TEST-WORKFLOW.md#公共检查与重验条件)；lab_check的build／files／docs，先看--help；只读本地输入，设备连续性仍由适配与现场确认 |
| UI / XML / 字体资源 | [UI-DEMO](docs/UI-DEMO.md)、[UI 交互契约](docs/UI-INTERACTION.md)；先由地图确认源与输出，再定位对应专题，如 [控制中心／共用选时](docs/CONTROL-CENTER.md)、[充电画面](docs/CHARGING-UI.md)、[呼吸训练](docs/BREATHING.md)、[闹钟](docs/ALARM.md)、[倒计时](docs/COUNTDOWN.md)、[2048](docs/GAME-2048.md)、[活力指标](docs/ACTIVITY.md)、[指南针](docs/COMPASS.md) |
| BLE、PM 或产品集成 | [产品入口](docs/PRODUCT-RUNTIME.md)、[BLE](docs/BLE-FIRST-LINK.md)、[PM](docs/PM-EVENT-WAKE.md)、[AOD面板与显示](docs/AOD.md)，仅选相关域；倒计时后台边界见 [COUNTDOWN 第二轮](docs/COUNTDOWN.md#round2) |
| 架构、裁剪或风险核查 | [ARCHITECTURE](docs/ARCHITECTURE.md) 相关契约与 [审查债务 §3](docs/ARCHITECTURE-REVIEW.md#3-债务清单全部未关闭)，先核对证据提交 |
| 硬件准备、焊接、电池、功耗与试戴 | [HARDWARE](docs/HARDWARE.md) → [电池与电机接入准备](docs/ACCEPTANCE.md#battery-motor-preparation)；[测流方法](docs/ACCEPTANCE.md#first-power-session)／[完整清单](docs/ACCEPTANCE.md#hardware-sequence)仅存契约，活动顺序回 STATUS |
| 电机控制的软件边界与诊断 | [手动限时电机](docs/MOTOR-PULSE.md)；真实引脚／驱动与负载条件见HARDWARE，当前活动项回STATUS |
| 电量算法、电池页、长续航及充电进度环 | [电压估算契约](docs/BATTERY-ESTIMATE.md)、[电池主页面](docs/BATTERY-PAGE.md)、[长续航](docs/ENDURANCE.md)及[充电画面](docs/CHARGING-UI.md)；软件／同版设备验证与容量校准分别判断，活动项回STATUS |
| 听懂当前系统与低功耗做到了哪里 | [当前系统／PM 说明](docs/PM-EVENT-WAKE.md#current-power-system)；区分当前源码、分版本 USB 证据与尚未测得的电流／续航 |
| 下载与 Product 真机验收 | [HARDWARE](docs/HARDWARE.md) → [FLASHING](docs/FLASHING.md) / [Product 验收](docs/PRODUCT-ACCEPTANCE.md)；核对实际版本与供电 |
| 追溯 N0 审计与旧返工 | [首轮验收](docs/handoffs/N0-CODEX-ACCEPTANCE.md)、[旧任务契约](docs/tasks/N0-VERSION-AUDIT.md)；固定历史 SHA 不替代当前核对 |
| 决定记录写到哪里、整理文档 | [职责与维护](docs/DOCUMENTATION-WORKFLOW.md#responsibilities) |
| 分支、审阅、PR 与合并 | [CONTRIBUTING](CONTRIBUTING.md)；纯文档只做文档验证，CI 沿用手动基线 |
| GitHub成果展示、参考仓库与参与开发入口 | [展示调研与素材契约](docs/REPOSITORY-SHOWCASE.md)；阶段依赖见架构任务卡，活动项仍回STATUS |
| 了解 flow 试点版本与取舍 | [接入记录](docs/FLOW-PILOT.md)，无需到 flow 通读资料 |

工具入口（含 [VS Code](docs/VSCODE.md)）遵循同一 AGENTS / 索引 / STATUS，不维护另一份任务状态。资料不足、冲突或依赖核对才扩大阅读。
