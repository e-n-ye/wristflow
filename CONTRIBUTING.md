# WristFlow 工作流

仓库：https://github.com/e-n-ye/wristflow ，私有。首次基线为 `27ee4d1`。

## 一次变更

1. 从最新 `main` 建立 `codex/<主题>` 小分支，每个 PR 只处理一个实验或修复。
2. 在 VS Code 修改项目文件，运行对应的 `WristFlow: Build ...` 任务；修改构建入口、SDK 锁或公共配置时验证 Hello、BLE、Bringup 三项。
3. 检查差异，提交明确范围的文件并推送分支。不要提交 `.tools/`、日常 `artifacts/`、构建目录或凭据。
4. 创建面向 `main` 的 PR，按模板记录源码检查、编译证据、硬件验证及剩余风险。
5. 等待最新提交的 `Firmware / Build baselines` 成功，检查差异后使用 Rebase and merge。失败先修复，不把本地通过替代云端结果。
6. 合并后同步 `main`，下一项实验另开分支。不直接向 `main` 推送日常修改，不强推。

首次基线初始化是上述分支流程的唯一初始例外。远端强制保护是否可用，以仓库设置和账号套餐实际结果为准；文档约定不等于已启用服务端保护。

2026-09-21 实际请求 main 分支保护返回 HTTP 403：私有仓库需要 GitHub Pro 或改为公开。当前保持私有，不升级套餐；PR 和成功检查是执行约定，尚无服务端强制保护。

## VS Code 日常入口

左下角分支名称可创建或切换分支；源代码管理视图查看差异、暂存、提交和推送。构建仍使用现有任务，`Ctrl+Shift+B` 仍为 Hello。PR 和 Actions 可在仓库网页查看，无需新增 VS Code 扩展。

新电脑使用 `git clone --recurse-submodules https://github.com/e-n-ye/wristflow.git`，安装 Git、PowerShell 7、uv 后执行 `scripts/Install-Sdk.ps1`。本机已有环境不重装。VS Code 当前任务的 PowerShell、Git、uv 路径是本机配置，迁移时按 `docs/VSCODE.md` 调整。

## 最小云端检查

`.github/workflows/build.yml` 在 PR、main 推送和手动触发时使用 Windows Server 2022 runner，递归取得固定 SDK，安装 uv 0.11.21，调用官方 SDK 安装包装器，再顺序编译三个基线。

新 runner 首次安装需要网络；日常构建沿用已安装 Python 的离线检查与环境导出。暂不缓存整个 SDK 环境，避免依赖安装路径的状态文件跨机器复用。单次作业限时 45 分钟，证据 artifact 保留 14 天，失败也尝试上传安装和构建记录。固件本体不自动发布；记录包含固件哈希、配置及版本。

这只验证安装与编译链接，不执行烧录，也不证明显示、BLE、休眠或续航。私有仓库 Actions 可能消耗账号额度；本轮不启用付费或修改账单。

不搬迁旧 F411 容器、CMake、签名发布和模拟器。后续只有出现明确需要时再增加检查。
