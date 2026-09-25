# WristFlow 工作流

仓库：https://github.com/e-n-ye/wristflow ，公开。首次基线为 `27ee4d1`。

## 一次变更

1. 从最新 `main` 建立 `codex/<主题>` 小分支，每个 PR 只处理一个实验或修复。
2. 在 VS Code 修改项目文件，运行对应的 `WristFlow: Build ...` 任务；修改构建入口、SDK 锁或公共配置时验证 Hello、BLE、Bringup 三项。
3. 按 [文档同步工作流](docs/DOCUMENTATION-WORKFLOW.md) 检查本次行为、决策和验证证据的记录；短小更新由主代理直接完成。审阅代码与文档差异后，提交明确范围的文件并推送分支。不要提交 `.tools/`、日常 `artifacts/`、构建目录或凭据。
4. 创建面向 `main` 的 PR，按模板记录源码检查、编译证据、硬件验证及剩余风险。
5. CI 不随 PR 或 push 自动运行。日常检查最终差异和与当前源码对应的本地编译、主机测试及必要的真机证据，再使用 Rebase and merge。需要干净环境证据时，在 Actions 的 `Firmware` 工作流中选择分支手动运行 `Build baselines`，并把对应提交和结果写入 PR。不要为了等待手动 CI 停止开发。
6. 合并后核对 PR 和最新提交状态，按文档同步工作流保留交接记录，再同步 `main`、另开下一项分支。存在未提交改动时保留原工作树并在隔离工作树继续。不直接向 `main` 推送日常修改，不强推。

首次基线初始化是上述分支流程的唯一初始例外。main 仍要求通过 PR，禁止强推和删除；不再要求自动 CI 状态，因为工作流只响应手动触发。个人项目不要求额外审批人数，但合并前仍须检查差异。

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

认证失败应处理登录，不重复安装。PR 使用 `"$gh_bin" pr create --base main --head <branch> --title '<title>' --body-file <UTF-8文件>`；正文通过文件传递，避免多层 shell 转义。创建后使用 `pr view <编号> --json url,headRefOid,statusCheckRollup` 查询；需要云端证据时手动 dispatch 工作流并确认结果对应最新提交，再按上述流程 Rebase 合入。不要输出或保存认证令牌。

本节变更只做文档检查和 CLI 只读核验，不新增本机固件编译或硬件证据。下一次 PR 沿用解析后的路径，验证创建及最新提交检查查询；`.tools/` 二进制不提交，其他机器不假设拥有此版本目录。

## 最小云端检查

`.github/workflows/build.yml` 只响应 `workflow_dispatch`，在 Windows Server 2022 runner 上递归取得固定 SDK，安装 uv 0.11.21，调用官方 SDK 安装包装器，再编译 Hello、BLE、Bringup、UI Demo、Product 五个目标及执行 PC UI 主机测试。

`Build baselines` 是按需的完整基线，不是 PR 的必需状态检查。它用于 SDK、工具链、公共构建入口变更和阶段验收；日常文档与 UI 迭代不因没有云端运行而阻塞。文档内容是否与实现同步仍需主代理审阅。

新 runner 每次安装需要网络；安装后的构建步骤沿用已安装 Python 的离线检查与环境导出，本机日常构建复用现有环境。暂不缓存整个 SDK 环境，避免依赖安装路径的状态文件跨机器复用。完整基线单次作业限时 45 分钟，证据 artifact 保留 14 天；固件本体不自动发布，记录包含固件哈希、配置及版本。

这只验证安装与编译链接，不执行烧录，也不证明显示、BLE、休眠或续航。本轮使用 GitHub 托管标准 runner，不启用付费或修改账单。

不搬迁旧 F411 容器、CMake、签名发布和模拟器。后续只有出现明确需要时再增加检查。

首次本地与云端验证、具体产物、已处理失败和下一项实验见 [工作流证据](docs/WORKFLOW-EVIDENCE.md)。
