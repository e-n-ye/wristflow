# 工作流首轮验证

日期：2026-09-21。仓库：https://github.com/e-n-ye/wristflow 。首次基线 `27ee4d1`，流程 PR：https://github.com/e-n-ye/wristflow/pull/1 。

## 实际完成

- 本地通过现有 `scripts/Build.ps1 -Example <hello|ble|bringup> -Jobs 4` 复编译三个目标，退出码均为 0，三个 BIN 的 SHA-256 均保持原基线不变。
- 首次云端运行 https://github.com/e-n-ye/wristflow/actions/runs/35616117658 成功，对应 PR 头提交 `0b1ee8166cf539fff6026f9c54581eb8f673d6f9`。全新 Windows 2022 runner 递归 checkout、安装官方环境、顺序构建三项并上传证据，全部成功。
- 云端 SDK 和两个子模块与 `sdk.lock.json` 一致，SDK 工作树干净；三份生成 `.config` 与 `docs/evidence/2026-09-21/` 原始配置按文本行比较无差异。
- 云端编译器 Arm GNU 14.2.Rel1 / GCC 14.2.1，SCons 4.10.1，uv 0.11.21，Python 3.13.14。本机 Python 为 3.13.15；官方约束仍为 3.13 系列，未改锁或重装本机环境。
- main 保护 API 回读确认：必须 PR、要求 `Build baselines` 成功且分支与 main 同步、管理员受约束、禁止强推和删除。仓库只允许 Rebase and merge。

## 云端产物

| 目标 | main.bin 字节 | SHA-256 |
|---|---:|---|
| Hello | 296736 | `20f804bff490c7a44f446953a6a6f35e95b77b3bdb86a85bd3897fbe8f14db07` |
| BLE | 486424 | `9e9c7d09b27269b1f5ce6f6f1751b1543a3350306045dc3c3329a406d652d4e7` |
| Bringup | 609976 | `e090194fd15cb931789ba7830b5c68ca7eba721c8d7ff26c34cd6955b8839220` |

机器可读结果已归档为 `docs/evidence/2026-09-21/ci-*-first-build.json`。完整下载证据和日志保留在本机 `artifacts/ci/35616117658-retry/`；GitHub artifact 为 `build-evidence-35616117658-1`，保留 14 天。第一次下载命令因 20s 超时被终止，已有文件保留，改用后台任务下载到独立目录成功。

云端 BIN 大小/哈希与本机不同，原因未单独定位；路径和安装环境不同，当前只证明可重建成功，不承诺跨机器逐字节一致。配置相同也不等于运行行为通过。

## 阻断及处理

1. GitHub CLI 不在 PATH，下载官方 gh 2.101.0 到被忽略的 `.tools/`，校验官方 SHA-256 后解压；第一次校验文件下载 TLS 失败，重试成功。
2. CLI 未登录，由用户完成设备授权。第一次工作流分支推送因缺少 `workflow` scope 被拒绝；用户完成补充授权后推送成功。未记录 token 或密码。
3. 初始私有仓库分支保护返回 HTTP 403，用户明确选择公开后切换可见性并启用保护。没有购买服务或修改账单。
4. 初次 `git diff --cached --check` 指出历史原始构建日志的表格尾空格；保留原始日志，仅对其他文件检查并通过，没有重写历史证据。

## 边界与下一项实验

本轮没有修改固件源码、SDK、离线环境激活机制或默认 VS Code 构建任务；没有烧录或硬件测量。源码核验、实际编译成功与硬件验证分别记录。

下一项仍为独立 UI + BLE + PM 最小实验：先核查 BLE 初始化、连接事件及双核低功耗约束，再决定如何替换无 BLE 的 `HAL_LPAON_Sleep()` 假设。现有三个基线保持独立；Gadgetbridge 真通知等硬件到货，累计实验上限仍为 8h。工作流已具备可核验闭环，不继续扩展签名、模拟器或旧 F411 CI。
