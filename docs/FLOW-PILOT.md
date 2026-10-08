# flow v0.1 首个已有项目试点

接入日期：2026-10-08（Asia/Shanghai）。对象：WristFlow 文档与导航。活动项和推荐试跑只由 [STATUS 当前区](STATUS.md#current) 维护；本页是版本、取舍与验收记录，不是第二份状态页。

## 采用版本与来源

采用个人嵌入式工作流 **v0.1 / 2026-10-08 文档快照**。源目录为用户指定的相邻 `flow` 文档集，检查时不是 Git 仓库，没有可记录的 commit / tag；用本次实际读取文件的 SHA-256 固定版本，不虚构版本提交。来源路径以下均相对 flow 根目录；它们是外部出处，不是本项目运行所需的链接。

| 采用来源 | SHA-256 |
|---|---|
| `PROJECT_INDEX.md` | `d806c63ade9534421f6687520a2eb2e28e2e402d0679fb77205fa6f5ab15acc4` |
| `WORKFLOW.md` | `6f1178417b54c3c81174b773c4a61c190075fe5f66809b76453e1c3f8f4eeed8` |
| `docs/DOCUMENTATION.md` | `14acd07a8703c3110f9c4c50268a0ca0d2f5f12259e9790c28e6c72a2d5a3ede` |
| `docs/adapters/SIFLI-SCONS.md` | `b386eb71d08b9270def897de4f92730d3c7ea01d1006a0a1bbb274b03413dbc3` |
| `prompts/INITIALIZE.md` | `cda346ca4141d23eef39b42eae258c8ae291b82df3a500c3f5e3a600a0d5c964` |

读取从 PROJECT_INDEX 路由到初始化、文档职责与维护、SiFli 适配和通用阶段 / 验证 / 接续规则；为确认版本另读 flow 的 `docs/context/NOW.md`。没有通读 STM32 适配、教程 PDF 或所有提示词，也没有修改 flow。

## 适配取舍

- 沿用目标、预算 / 日历、架构共识和 N0–N5 依赖、SDK 锁、官方 SCons、LVGL 官方导出与手动 CI 策略；不重访已确认需求，不更换构建链。
- 文档职责映射落在 [DOCUMENTATION-WORKFLOW](DOCUMENTATION-WORKFLOW.md#responsibilities)。已有 BASELINE、STATUS、架构 / 风险、操作手册和证据目录足以承担对应职责；只补 PROJECT_INDEX、工程所有权地图及本试点记录，不建立平行 docs/context 全套文件。
- AGENTS 改为完整规则 + 按任务定位章节，不再要求每次全文读取 STATUS / BASELINE / BUILD。当前任务集中到 STATUS 顶部稳定锚点，旧摘要明确标 history；计划保存阶段契约，旧任务书保存历史授权边界。
- README / VS Code 指向同一规则与状态；当前没有需要接入的 CLAUDE.md / GEMINI.md，不创建空壳入口。未来工具确需入口时只链接共同规范。
- 用户本轮明确授权按项目分支、审阅、PR 与合并流程交付，覆盖初始化模板默认的禁止 commit / push / PR 条款；仅文档范围仍有效。旧 Gemini 任务书的 Git 禁令属于其历史执行包，不覆盖本次接入。
- 用户明确因返工过多暂时放弃 Gemini 协作；其原提示词改为历史，保留未通过结果与独立复核，不把停用协作解释成 N0 已通过。项目旧文档中的默认 Sol/low 改为引用适用用户规则，避免与现行模型路由冲突。
- SiFli 适配示例包含天气分支才有的 `scripts/Flash.ps1`；主线没有该文件，差异在 [工程地图](ENGINEERING-MAP.md) 明确，未为套模板移植脚本。隔离工作树的依赖可用性也不由文档检查证明。

## 现场与版本核对

推进上下文取自用户指定的 Codex 对话 `01a115ff-d5f8-7ba0-9ed3-8cdc0e25dac2` 的最后两个已结束任务，仅作为来源线索；仓库记录可独立接续，不要求其他工具访问聊天。

| 对象 | 2026-10-08 实际核对结果 |
|---|---|
| 原工作树 | `codex/weather-sync@f68d64f170fea22eaf9561dc844731aa4f0fa9ed`，未切分支 / stash / reset |
| fetch 后主线 / 本次分支起点 | `origin/main@8be5821423df42c2c03feccbe3380f3402b5ea22` |
| 本次隔离分支 | `codex/flow-pilot`；独立 managed worktree，未复制原工作树未提交文件 |
| C2 | [PR #30](https://github.com/e-n-ye/wristflow/pull/30)：OPEN、DRAFT，`codex/imu-wrist-wake@823e656e4bd36ff0e84360ff085d524ad641917f` |
| 冻结实现 / 天气审查对象 | `5c0b82c02cf8f80168b076b0f76e2514a1867042` / `0606f3b23bdaccae14f547f5fa55d997d3a50b21` |
| SDK 身份来源 | `sdk.lock.json` 与主线 gitlink 均为 `421126d9f476ed8e2a6f0b0ca28a9f241c182e65`；本轮未初始化或更新 SDK / 子模块 |

原工作树已有改动三项保持原样，起始 SHA-256 用于收尾对照：

| 文件 | Git 状态 | SHA-256 |
|---|---|---|
| `.vscode/settings.json` | 已修改 | `c6660f3747fafa617e2b41e808c268f70cdbbb2108ccf7059ea97f13e5cef03d` |
| `docs/evidence/n0-version-audit.json` | 未跟踪 | `ba5133f291d4f933d0f1ad0e798bf364bea6d421734983d183d9e00c84569aa3` |
| `docs/handoffs/N0-VERSION-AUDIT-RESULT.md` | 未跟踪 | `e2736c4ff3139f0b1c867321aa2f0dfe1e87a2b570205609a68aed8489552835` |

因此新 clone 没有这两份原交付；可先读已提交的 [N0 验收](handoffs/N0-CODEX-ACCEPTANCE.md) 与 [复核证据](evidence/n0-codex-acceptance.json)，需要原件时从本机原天气工作树按上述路径取得并校验。缺失不能伪装为已交付或重建完成。

在原仓库 / 隔离仓库执行的只读命令包括：`git status --short --branch`、`git branch --show-current`、`git log -5 --oneline`、`git worktree list --porcelain`、`git remote -v`、`git fetch origin`、`gh pr view 30 --json number,state,isDraft,headRefName,headRefOid,baseRefName,url`，以及 `Get-FileHash -Algorithm SHA256`。Git 为 `2.42.0.windows.1`；gh 复用原项目已有工具，无安装。

版本差异核对命令（从隔离工作树根目录运行，均退出 0）：

```text
git diff --name-status 5c0b82c02cf8f80168b076b0f76e2514a1867042 origin/main
git diff --name-status 0606f3b23bdaccae14f547f5fa55d997d3a50b21 codex/weather-sync
git diff --name-status origin/main codex/weather-sync -- scripts
git ls-tree HEAD vendor/SiFli-SDK
```

前两项分别只列文档 / PR 模板和文档；第三项证明天气新增 Flash / Generate-Weather-Sun 脚本并修改资源生成脚本。没有把这次路径比较扩写成源码清单、镜像或设备复验；旧 N0 比较仍绑定旧固定 SHA。

## 文档验收记录

主代理结构检查：从隔离工作树根目录以 Python 3.14.3 的 `python -c` 执行本轮文件清单校验，严格 UTF-8 解码、替换字符 / 空字符、代码块闭合、行尾空白、相对文件链接和标题 / 显式锚点检查。15 份 Markdown、216 个本地链接 / 锚点通过，索引为 2,500 字节，退出 0；外部网站连通性不包含在链接检查内。`git diff --check` 退出 0；路径白名单确认只有这 15 份 Markdown，无业务代码、SDK、锁、构建 / CI 配置或生成输出差异。原工作树 HEAD、分支及三项已有文件哈希在中断恢复后再次核对不变。

独立只读接续走查由未参与编写的 `flow_handoff_walkthrough` 子代理完成，主代理按其出处抽查并作最终文档验收；子代理没有写入、Git 操作、构建、测试或设备行为。请求档位为 `gpt-6-luna / low`，运行工具未提供额外的有效模型标识，故不由自报推断实际模型。前轮尝试曾遇服务并发限制及 Luna / Sol 的明确不可用错误，没有取得验收证据；用户确认服务恢复后，本轮重新派发才取得以下结果。

| 只读场景 | 实际读取路线 | 验收结果 |
|---|---|---|
| 新接手者找到目标、活动项与首项建议 | AGENTS → PROJECT_INDEX → STATUS current → N0 验收 / 旧任务契约；目标按路由补读 BASELINE 的固定边界、日期预算、功耗比较和 USB 阶段，以及 ARCHITECTURE §1 | 能找到已确认目标与成功标准，不重做访谈；区分本轮文档接入、N0 历史 FIX、待后续授权的建议及主线 / 天气 / C2 / 未知设备身份。未执行旧 Gemini 返工 |
| 假设后续获准改现有 UI XML 页面 | AGENTS → PROJECT_INDEX → ENGINEERING-MAP 的 XML 与操作表 → UI-DEMO 的官方操作段 / UI-INTERACTION 相关契约 → DOCUMENTATION-WORKFLOW 职责与完成条件 | 能找到设计源、官方输出、手写适配与 SCons 清单，识别实际工作树 / 环境前置、预览导出 / 受影响目标验证及行为、方法、证据、STATUS 的各自更新位置；未把天气 Flash 脚本当主线入口 |

走查无需通读所有资料；未遍历 SDK、全部历史或旧原始日志，不声称这些材料已经重新验收。目标路由可通过既有标题定位，没有为锚点外观再拆一套目标文档。主代理最终检查时另修正工程地图中构建目录的回退描述：仅当 `_hcpu` 目录不存在时才改查无后缀目录，与 Build 的条件一致。

最终 PR 合并回读留在 PR / 交接，不再为合并编号新开文档 PR。

本轮明确未执行：业务代码修改、固件 / 主机构建、CTest、XML 导出、设备查询 / 串口 / 烧录、功耗测量、技能安装或定时任务。纯文档检查不证明 N0 修订通过、固件可构建、硬件或用户体验通过。

剩余已知缺口是历史 N0 FIX、原始本机材料的可携带性、分支能力差异与未知设备身份；不借此次接入关闭这些风险。下一项有界试跑及停止条件只见 STATUS 当前区。
