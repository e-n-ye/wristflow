# 2026-10-23 工作目录迁回与旧工作树清理

用户指出当前没有并行开发，并要求清理已交付的工作树、以后不要在C盘创建工作树。后续顺序开发优先使用干净主目录中的主题分支；确需隔离时只在已核对的D盘路径建立，规则已写入项目AGENTS，不修改全局Codex配置。

## 迁移与保全

- 主目录`D:/MY_Desk/project/wristflow`原为干净`main@de99c5a`。旧树`C:/Users/13984/.codex/worktrees/aod-stack-regression/wristflow`也是`de99c5a`，仅6个本轮LTR文件未提交；未新建工作树。
- 在旧树分离HEAD后，主目录检出同一`codex/ltr303-diagnostic`分支；6个LTR文件原样复制，逐文件SHA-256一致。原文件、迁移清单和旧树全部内容同时归档，未stash、丢弃或重置用户工作。
- 完整旧树5,946个文件／1,421,136,404B已保全到主目录忽略路径`artifacts/worktree-archives/20261023/aod-stack-regression-complete.zip`，ZIP为374,253,484B，SHA-256 `7f4084e739a31af8e3fb6b56ec148cf84c20422cb0fc0b17d6a2cfb262dd1aaf`。ZIP内成员为`wristflow/<旧树相对路径>`；原始日志／JSON中的C盘绝对路径保留，不改写历史哈希。
- `aod-stack-regression-manifest.json`记录全部文件哈希，`ltr303-transfer.json`记录当前改动的迁入，`cleanup-result.json`记录清理及剩余工作树。首次校验最终列表用了Windows Path排序与字符串排序两种顺序，断言失败；ZIP已完整写入且之前逐文件哈希通过，后续独立验证使用相同字符串顺序并重新核全部ZIP及源文件哈希，确认完整。原失败脚本／日志保留。
- 确认归档、未提交改动迁入及HEAD归属后，仅对上述精确旧树路径执行`git worktree remove --force <旧树路径>`。读回只剩D盘主目录，旧目录已不存在；保留已有主题分支与提交，不展开无关分支清理。

## 历史证据的新查找方法

当旧记录引用`C:/Users/13984/.codex/worktrees/aod-stack-regression/wristflow/<相对路径>`时，从完整ZIP中读取`wristflow/<相对路径>`，按manifest或原证据SHA-256核对。包括AOD官方预览／真实LVGL截图、失败日志、主机结果、各Product／Demo结果和当时尚存的固件、LTR首版构建及测试，不丢弃构建目录中的证据图片。

原始设备记录仍在D盘主目录`artifacts/aod-a1-hardware-20261023/`，迁移不覆盖它们。选择修正版过去归档失败、22B不完整ZIP的历史限制仍成立；完整搬迁只保全当时剩余文件，不宣称补齐了已被后续构建覆盖的历史BIN。

LTR接下来使用D盘主目录常规`scripts/Build.ps1`。旧CMake缓存包含旧绝对路径，作为历史留在ZIP，不搬来直接运行。LTR软件与设备验证结果回[专题](../LTR303.md)和[STATUS当前区](../STATUS.md#current)，本页不维护活动队列。
