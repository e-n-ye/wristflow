# 2026-10-08 工作区收拢记录

本页记录本轮成果保全和恢复方法；唯一活动项/下一步见 [STATUS 当前区](../STATUS.md#current)，已确认完成线与验证取舍见 [BASELINE](../BASELINE.md#2026-10-08-清理完成线与恢复开发)。本轮为一个文档与工作区闭环，不包括天气/C2 修复、固件构建或设备实验。

## 版本与未集成功能

- 文档起点 `origin/main=da3e417d5f56ecab39a55263bddb2d61cbe9c826`，#32–#35 均已合入。主目录原为 `codex/weather-sync@f68d64f170fea22eaf9561dc844731aa4f0fa9ed`，本地 main 落后十五个提交，已快进到起点后建立 `codex/workspace-recovery`。本轮 PR 及最终 main 身份由交接/GitHub 回读记录，不为回填合并再开文档 PR。
- 天气保留 `codex/weather-sync@f68d64f`，相对主线四个功能提交；未开 PR、未集成。D1/D2 已有静态风险，D3/D8 仍须核查。原视觉/构建/注入证据不证明真实手机来源。
- C2 [#30](https://github.com/e-n-ye/wristflow/pull/30) 保持 OPEN / DRAFT，head `823e656e4bd36ff0e84360ff085d524ad641917f`，与主线冲突；独立工作树保留。原手持门槛、响应偏慢和灵敏度反馈不等于组合回归或佩戴/功耗通过。
- 旧表盘、阶段规划、BLE、PM、通知及产品设置分支的有效内容已经由后续集成吸收。未再次整支合并旧文档，避免恢复过时的阶段/能力描述；分支和恢复引用均保留。

## 本机保全定位

以下为本机证据位置，忽略产物不会随 clone 自动取得；不是新增公共配置。

| 保存对象 | 本机位置/身份 |
|---|---|
| 主目录原三项本地改动 | `artifacts/workspace-recovery/20261008/primary-local/` 的原字节副本及 `primary-local.json` |
| 主目录 Git 恢复快照 | `refs/wristflow-archive/20261008/primary-local` 指向 stash `cfccacb12ca53c9a059e6d995112c3ba74daaec9`；原 stash 亦保留 |
| 归档工作树全目录 | `C:/Users/13984/.codex/worktree-archives/wristflow-20261008/<名称>/wristflow` |
| 每个工作树原 Git 管理数据 | `D:/MY_Desk/project/wristflow/artifacts/workspace-recovery/20261008/git-admin/<名称>`，包括原 index、HEAD、日志及存在的 submodule 元数据 |
| 归档清单和修改文件哈希 | 同一 `artifacts/workspace-recovery/20261008/<名称>.json`；记录原路径、归档路径、HEAD、分支、状态和保护引用 |
| 提交对象长期保护 | `refs/wristflow-archive/20261008/<名称>`；保护历史 HEAD，未提交内容仍以归档原文件和原 index 恢复 |

主目录三项为 `.vscode/settings.json`、Gemini `docs/evidence/n0-version-audit.json` 与 `docs/handoffs/N0-VERSION-AUDIT-RESULT.md`。逐文件 SHA-256 与副本一致后执行限定路径 `git stash push -u`；未把本机 CMake 路径或未通过报告推入主线，旧报告保持 FIX。

逐个归档十二个名称：architecture-consensus、ble-first-link、documentation-sync、flow-pilot、next-stage-plan、product-acceptance、product-runtime、product-settings、product-ui-feedback、ui-shell、watchface-design、weather-ui。保留全部目录内容，包括忽略的 artifacts、构建输出、不完整 SDK 下载和 junction；主项目修改/未跟踪文件在移动前后按哈希核对。SDK 内部内容未重新审计。

操作先保存 HEAD 引用与原 Git 管理数据，再对明确路径用 PowerShell Move-Item 移动目录；存在的 Git module 元数据另行保全，确认原路径已不存在后仅移除该路径的 Git 工作树登记。没有批量删除、清理下载、删除分支或修改 SDK。天气目录首轮哈希路径查询因 Git 扫描子模块停滞而中止；未移动文件，随后改用 `--ignore-submodules=all` 完成。主项目状态核对与 SDK 内部核查分开。

最终注册工作树为主目录与 `imu-wrist-wake` 两个。本轮文档合并后主目录切回并快进最新 main；未集成的天气提交、C2 和旧实验仍可恢复，不把工作树减少视为功能全部完成。

## 本轮文档与保全验证

本机 Git 为 `2.42.0.windows.1`，PowerShell 为 `7.6.5`。在主目录执行：

```powershell
& 'C:/Users/13984/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe' -NoProfile -File artifacts/workspace-recovery/20261008/Validate-Recovery.ps1
```

退出 0：本轮 7 份中文 Markdown 严格 UTF-8、130 个本地链接/锚点、7 项历史文件哈希、12 个归档的修改文件哈希及保护引用、主目录 3 项原文件副本与恢复引用均通过；注册工作树为 2，`git diff --check` 通过。脚本和输出 `validation.json` 留在上述本机证据目录；不是公共构建入口。检查没有执行固件构建或硬件操作，也没有逐字节审计归档中全部忽略产物或 SDK 内容。

## 历史验收救回与恢复边界

Product 验收工作树原未提交记录已保留原件，其有效摘要进入 [PRODUCT-ACCEPTANCE](../PRODUCT-ACCEPTANCE.md#2026-10-04-联合验收尝试历史记录恢复) 和 [精简证据](../evidence/2026-10-04/product-joint-attempt.json)。现存三镜像、build/flash JSON 和启动日志重新核对哈希；J1 用户十轮反馈、J2 简要确认、J3 未完成及 J4 未通过分别记录。未重新运行构建、烧录、重连或功耗实验。

要追溯日志，按归档清单把旧工作树路径替换为 archive_path 后追加原相对路径。要恢复完整旧工作树，先读对应 JSON，确认原路径与原 Git 管理路径均空闲，再将归档目录和保留的管理数据恢复到原明确路径，并执行 `git worktree repair <原路径>`；恢复前不得覆盖现有目录或索引。主目录三项可从字节副本单独恢复；需 Git 补丁时用受保护 stash 先检查 diff，在合适分支按所需路径恢复，不在 main 盲目 apply 整个 stash。

本轮 PR 仅包含经更正的历史证据、当前状态和用户已确认的顺序调整。旧硬件事实只支持原版本；今天设备身份保持 unknown_not_observed。天气/C2 的修复、测试/编译、USB 验收和各自实际合并仍需后续闭环完成。
