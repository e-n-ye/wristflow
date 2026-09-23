# WristFlow 工作流

仓库：https://github.com/e-n-ye/wristflow ，公开。首次基线为 `27ee4d1`。

## 一次变更

1. 从最新 `main` 建立 `codex/<主题>` 小分支，每个 PR 只处理一个实验或修复。
2. 在 VS Code 修改项目文件，运行对应的 `WristFlow: Build ...` 任务；修改构建入口、SDK 锁或公共配置时验证 Hello、BLE、Bringup 三项。
3. 检查差异，提交明确范围的文件并推送分支。不要提交 `.tools/`、日常 `artifacts/`、构建目录或凭据。
4. 创建面向 `main` 的 PR，按模板记录源码检查、编译证据、硬件验证及剩余风险。
5. 等待最新提交的 `Firmware / Build baselines` 成功，检查差异后使用 Rebase and merge。失败先修复，不把本地通过替代云端结果。
6. 合并后同步 `main`，下一项实验另开分支。不直接向 `main` 推送日常修改，不强推。

首次基线初始化是上述分支流程的唯一初始例外。main 已启用服务端保护：必须通过 PR，要求最新基线上的 `Build baselines` 成功；管理员也受约束，禁止强推和删除。个人项目不要求额外审批人数，但合并前仍须检查差异。

2026-09-21 最初私有仓库分支保护请求返回 HTTP 403 套餐限制；用户随后明确选择公开，已切换并成功启用保护，未升级套餐。

## VS Code 日常入口

左下角分支名称可创建或切换分支；源代码管理视图查看差异、暂存、提交和推送。构建仍使用现有任务，`Ctrl+Shift+B` 仍为 Hello。PR 和 Actions 可在仓库网页查看，无需新增 VS Code 扩展。

新电脑使用 `git clone --recurse-submodules https://github.com/e-n-ye/wristflow.git`，安装 Git、PowerShell 7、uv 后执行 `scripts/Install-Sdk.ps1`。本机已有环境不重装。VS Code 当前任务的 PowerShell、Git、uv 路径是本机配置，迁移时按 `docs/VSCODE.md` 调整。

## 本机 GitHub CLI

2026-09-23 核验：Bash 的 `gh` 不在 PATH，但项目内 `.tools/github-cli-2.101.0/bin/gh.exe` 可用，`--version` 返回 2.101.0，`auth status` 确认认证有效。此前任务已使用该路径，后续却因 `gh: command not found` 转到浏览器；命令不在 PATH 不等于未安装。

从仓库根目录执行以下 Bash 命令，优先复用项目工具，再检查 PATH；两处均无可用工具才评估安装。变量仅作用于当前 shell，新 shell 需重新解析，不改全局 PATH。

```bash
gh_bin='./.tools/github-cli-2.101.0/bin/gh.exe'
if [ ! -x "$gh_bin" ]; then gh_bin="$(command -v gh)"; fi
if [ -z "$gh_bin" ]; then
    printf '%s\n' 'GitHub CLI not found in the recorded location or PATH' >&2
    exit 1
fi
"$gh_bin" --version
"$gh_bin" auth status
```

认证失败应处理登录，不重复安装。PR 使用 `"$gh_bin" pr create --base main --head <branch> --title '<title>' --body-file <UTF-8文件>`；正文通过文件传递，避免多层 shell 转义。创建后使用 `pr view <编号> --json url,headRefOid,statusCheckRollup` 和 `pr checks <编号> --required` 查询，确认检查对应最新提交且必需构建成功，再按上述流程 Rebase 合入。不要输出或保存认证令牌。

本节变更只做文档检查和 CLI 只读核验，不新增本机固件编译或硬件证据。下一次 PR 沿用解析后的路径，验证创建及最新提交检查查询；`.tools/` 二进制不提交，其他机器不假设拥有此版本目录。

## 最小云端检查

`.github/workflows/build.yml` 在 PR、main 推送和手动触发时使用 Windows Server 2022 runner，递归取得固定 SDK，安装 uv 0.11.21，调用官方 SDK 安装包装器，再顺序编译三个基线。

新 runner 首次安装需要网络；日常构建沿用已安装 Python 的离线检查与环境导出。暂不缓存整个 SDK 环境，避免依赖安装路径的状态文件跨机器复用。单次作业限时 45 分钟，证据 artifact 保留 14 天，失败也尝试上传安装和构建记录。固件本体不自动发布；记录包含固件哈希、配置及版本。

这只验证安装与编译链接，不执行烧录，也不证明显示、BLE、休眠或续航。本轮使用 GitHub 托管标准 runner，不启用付费或修改账单。

不搬迁旧 F411 容器、CMake、签名发布和模拟器。后续只有出现明确需要时再增加检查。

首次本地与云端验证、具体产物、已处理失败和下一项实验见 [工作流证据](docs/WORKFLOW-EVIDENCE.md)。
