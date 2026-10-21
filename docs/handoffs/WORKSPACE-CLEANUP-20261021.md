# 2026-10-21 工作区收拢与恢复记录

用户在原生翻腕基础验收后要求收拢所有分支、工作树、PR和无用存储。起点为干净`main@264ef9fa1f105787e22640f8a9c8d44dd8a6adf4`，本轮不改固件，不构建、烧录或打开串口；当前任务和下一步只见[STATUS](../STATUS.md#current)。[精简证据](../evidence/2026-10-21/workspace-cleanup.json)记录路径、SHA、字节数、命令与失败。

## 处置与边界

- 旧软件姿态PR [#30](https://github.com/e-n-ye/wristflow/pull/30)／[#39](https://github.com/e-n-ye/wristflow/pull/39)分别回读CLOSED，原head为`823e656…`／`9815dca…`；未合并。主线已采用原生AWT，旧独有诊断和硬件记录保存在Git bundle与工作树ZIP，不恢复旧算法。关闭不把旧体验或组合回归记为通过。
- 50个旧本地主题分支逐个保存到`refs/wristflow-archive/20261021/<去掉codex前缀的名称>`后删除；全部SHA与预先验证的Git bundle一致。远端实际只删除两个旧腕分支，其余36条origin/codex为陈旧远端追踪引用，经fetch --prune清理。清理记录PR创建前，远端仅main、开放PR为0；记录PR完成后再回到同一完成线。
- 7个旧工作树逐个压缩／回读SHA-256、复核源文件后移除：alarm-ui、breathing-product、charger-compare、countdown-product、imu-wrist-wake，以及D盘hardware-meter-plan和snapshot-hardware。独立复核169个未提交文件全部在ZIP内（3／6／1／159），七份Git管理目录也单独备份。通知两独有提交与main的后续提交经range-diff等价；charger_compare.c仍按历史诊断保留，不移入生产源码。
- 10月8日旧存档12组目录共31,814个普通文件，全部压缩／逐文件SHA回读后，按一个明确根路径移除。8个junction只记录目标，不展开、不触碰目标；7工作树同样不跟随共享SDK／.tools链接。另逐个清除26个仅含0字节`.codex-worktree-name`的空名称目录。
- 旧Git保护引用与两个stash保留；主目录464份源码、29份固件产物与原构建逐字节一致，SDK及子模块仍在锁定版本且干净。仅有本轮文档记录待常规PR交付，无新增功能测试或硬件结论。

## 空间与本机保全位置

所有路径是本机保全位置，不随clone取得，不是新的公共环境配置。

| 项目 | 文件字节统计 |
|---|---:|
| C盘5工作树及旧存档移除 | 6,897,222,903 bytes（约6.90GB／6.42GiB） |
| 另移除D盘2个旧工作树 | 1,883,514,688 bytes |
| 8份完整ZIP保留在D盘 | 2,052,068,367 bytes（约2.05GB）；另有清单、Git管理备份及15,477,843 bytes的bundle |
| `.codex`扫描前→后 | 11,068,260,864→4,177,534,094 bytes（10.31→3.89GiB） |
| `.codex/worktrees`和`worktree-archives`扫描后 | 均0文件bytes |

统计不跟随reparse，忽略文件系统分配粒度；运行中的聊天文件会增长，前后差值不是Windows实际可用空间精确值。剩余聊天会话、SQLite数据库、插件、packages、sandbox、cache／临时文件仍有实际用途或所有权未判定，不属于本轮已确认无用工作副本，未删除。没有清理聊天历史、凭据、当前运行环境、现用artifacts或其他项目。

完整保全根为`D:/MY_Desk/wristflow-archives/2026-10-21`：

- 七个工作树各有`<名称>.zip`、`<名称>.json`和`<名称>-git-admin.zip`。JSON包含全部条目SHA、原路径、HEAD、分支、状态、目录与链接目标；ZIP包含所有实际普通文件，保留未提交／忽略内容。
- `legacy-20261008.zip`及同名JSON保留原旧存档的12组相对路径；被10月19日提前保全的构建证据仍在主目录`artifacts/workspace-recovery/20261008`，本轮不删不重复制。旧清理记录和原Git管理数据亦留在该处。
- `wristflow-all-refs-verified.bundle`包含清理前123个引用的完整Git历史，`git bundle verify`退出0；覆盖50个旧分支、旧恢复引用及stash。主仓库亦保留50个本轮保护引用，不依赖reflog存续。
- `metadata/`复制本轮清单、PR状态快照、操作日志和逐目标脚本。主目录`artifacts/workspace-cleanup/20261021`另留原运行记录；不把本机长日志提交到公共仓库。

## 恢复方法与限制

直接解压不会自动得到可用Git工作树：ZIP内`.git`指针指向已移除的管理路径，旧HEAD可能引用已归档的分支名。恢复前读同名JSON，选一个不存在的新目录；读取manifest的`branch`，去掉`codex/`前缀后构成`refs/wristflow-archive/20261021/<原分支后缀>`，核对引用SHA等于manifest的`head`，再建立恢复分支／新工作树（例如alarm-ui对应alarm-usb，breathing-product对应notification-history，hardware-meter-plan对应game-2048-usb，snapshot-hardware对应wrist-int1-route），再按ZIP覆盖需要的普通文件，**不覆盖新工作树的`.git`指针**。本地引用不存在时，可先用bundle检查引用并取回对应旧head。需要原暂存状态时再核对备份Git管理目录的index，不能直接把旧管理目录盲目覆盖现有仓库。

按manifest的`links`重建需要的SDK／工具链接前，先核对目标和SDK锁；ZIP没有复制共享目标。原未初始化的SDK需按锁重新接入。旧存档恢复按`legacy-20261008.json`映射组名和相对路径，主仓库原10月8日保护引用及管理备份仍保留。禁止覆盖现有目录或对main整体apply旧实验；只取本次需要的历史内容。本轮验证了备份完整性与未提交文件，未执行实际恢复演练。

初始bundle前台命令超时后，改为后台创建并验证；单独清除了该次11,111 bytes的未完成bundle锁文件。一次远端分支查询404由陈旧追踪引用解释，随后先fetch --prune确定真实远端清单；未误报为本轮删除了38个远端分支。只读ignored清单曾跨共享工具链接遇到长路径警告，实际保全与移除均用不跟随reparse的文件遍历，不依赖该不完整清单。

PR收尾依[CONTRIBUTING](../../CONTRIBUTING.md)回读合并及同步结果，记在该PR Closure或最终交接；不为回填结果另开PR。当前轮播偶发故障根因、佩戴识别效果、物理DEEP和功耗仍未关闭，不因工作区收拢而改变。
