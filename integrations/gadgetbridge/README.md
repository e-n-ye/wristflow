# Gadgetbridge 有界测试扩展

活动任务与下一步只见 [STATUS](../../docs/STATUS.md#current)。这里维护基于官方源码的补丁、锁及构建方法，不保存上游完整源码或二进制。补丁新增源码按 **AGPL-3.0-or-later** 提供；原文件版权保留，完整对应源码为锁定的[上游仓库](https://codeberg.org/Freeyourgadget/Gadgetbridge)加本补丁，遵循其[许可证](https://codeberg.org/Freeyourgadget/Gadgetbridge/src/tag/0.94.0/LICENSE)。这是未提交上游的本地测试扩展，使用了 AI 辅助。

## 契约

只有 `mainlineWristflow` 构建且设备真实名称为 `Bangle.js WristFlow` 时附加顶层 `wf` 对象，普通构建默认关闭。包名独立，应用名“WristFlow 测试版”，Pebble provider authority 也独立，保留原版应用与数据；调试签名仅供测试。既有 `notify.id`、`notify-.id`、操作句柄、通知内容和天气 v1/v2 字段均保留，v2 的 `d` 字节不改变。不增加网络权限、不改全局 NotificationSpec ID。

| 报文 | `wf` 字段与含义 |
|---|---|
| notify | `v:1`；`notification_key` 为安装内随机盐 HMAC-SHA256 的前 128 位、小写 32 位十六进制；`package` 为来源包名（有且 UTF-8 ≤255 B）；`when_present` 表示原时间为正，存在时附 `when_ms`，单位 Unix 毫秒 |
| weather v1/v2 | `v:1`；`aqi_present` 表示 `airQuality.aqi >= 0`，存在时附原整数 `aqi`；`aqi_scale:"gadgetbridge-native"` 表示原来源指数未经换算 |

匿名 key 是 **Android 通知生命周期**，不是已证明的 QQ 联系人或消息身份；清除／重建后的关联未证实。盐私有保存，应用数据清空或重装会改变标识；保存失败／损坏／加密失败时省略通知扩展，保留原通知发送。保存失败的进程内偏好对象不再产生身份，防止使用未持久化的内存盐。原 key 空或超过 2048 UTF-8 B 时省略扩展。

`when_ms` 是 Android 通知时间，不保证等于 QQ 发送时间；没有伪造缺失时间。AQI 存在不代表新鲜、准确或已归一化为中国／美国 AQI。上游模型说明单污染物指数预期 Plume，部分来源使用 WHO 回退；整体 `aqi` 通常为各项最大值，因此本扩展不宣称统一指数标准、不填假值、不按 500 截断。缺失时没有 `aqi` 数值，真实 0 保留。

新增 `WF_META` 日志只包含条目 ID、匿名 token、通知时间、来源包名及 AQI，不记录原 key、盐、联系人或正文。原 Gadgetbridge UART 日志仍可能含原通知正文；采集只在内存提取 `WF_META` 与最终 TX 的类型／ID／版本／`wf` 白名单，不提交整份手机日志。本手机补丁只扩展发送字段，列表归组、详情历史、精确撤回及 AQI 页面显示不在补丁内实现。后续固件的 AQI 消费与既有页面绑定见[固件闭环](../../docs/WEATHER-UI.md#weather-aqi-display)；通知 `wf` 标识／时间的有界保存、旧手机兼容和两人对照见[固件数据闭环](../../docs/NOTIFICATION-UI.md#notification-metadata-storage)，后续固件列表归组与详情历史的实施及验收见[通知专题](../../docs/NOTIFICATION-UI.md#notification-groups)。

本测试包同版构建、安装共存、真实同一通知生命周期 A/B 与原 ID 删除、天气 v1 AQI 126 的证据见 [精简记录](../../docs/evidence/2026-10-13/notification-metadata.json)。真实 TX 日志先于 GATT 入队，手表收到／清除是用户反馈，不等于新字段存储或显示。普通上游变体、实际进程重启、跨人／清除重建、天气 v2 扩展发送、最大帧与设备队列压力未实测。固件的 8192 B 行缓冲仍是既有约束；附加元数据会增加帧长，接近上限的报文可能丢弃，不保证任意通知兼容。

正文消费边界沿用锁定上游：监听器在 `notification_prefer_long_text=true` 时优先非空 `EXTRA_BIG_TEXT`，否则使用 `EXTRA_TEXT`；BangleJS 发送路径将正文裁到 400 个 UTF-16 code units。2026-10-14 Q8-D 只读实测偏好为 true，QQ 通知正文仅 99 字且没有更长字段，手表照片末尾与该来源一致。本补丁只扩展元数据，无法恢复来源没有提供的聊天文本；精简字段与本地整组删除前后对照见[归组证据](../../docs/evidence/2026-10-13/notification-groups.json)。本次未修改手机包或设置。

## 取得与构建

已有独立 Gradle 缓存时传 `-GradleCache <缓存绝对路径>` 复用；默认使用 OutputRoot 下的缓存，均不改机器级配置。

版本与包名以 [upstream.lock.json](upstream.lock.json) 为准，SiFli SDK 锁不变。本目录属性固定 LF，保留补丁空上下文行必需的空格；补丁内容另由锁哈希及干净 index 应用检查验证。使用短路径的独立上游 checkout，保留现有源码、缓存和失败下载；Windows 本 checkout 启用 `core.longpaths`，不改全局 Git 配置。

```powershell
git clone -c core.longpaths=true --depth 1 --branch 0.94.0 https://codeberg.org/Freeyourgadget/Gadgetbridge.git <短路径>
git -C <短路径> config core.longpaths true
git -C <短路径> apply --check <本仓库绝对路径>/integrations/gadgetbridge/wristflow-metadata.patch
git -C <短路径> apply <本仓库绝对路径>/integrations/gadgetbridge/wristflow-metadata.patch
```

先核对 HEAD 与锁；不把补丁应用到用户现有改动上。需要时用官方 SDK 管理器安装锁指定的准确平台包与 Build Tools（平台包名带 `.0`），按官方流程接受 SDK 许可。使用 JDK 21、SDK 平台与官方 Gradle Wrapper；不降级或更换上游依赖。已打补丁的源码可执行：

```powershell
pwsh -NoProfile -File <本仓库绝对路径>/integrations/gadgetbridge/Build-TestApk.ps1 `
  -SourceRoot <上游绝对路径> -JavaHome <JDK绝对路径> `
  -AndroidSdk <SDK绝对路径> -OutputRoot <忽略的产物绝对路径>
```

脚本先验证上游 HEAD、补丁哈希、五份目标文件的 UTF-8/LF 内容哈希及无非忽略的范围外 Git 改动；`-CheckOnly` 只验证，不构建。此检查不覆盖所有忽略配置或缓存，需使用独立、已知的构建环境。实际 Gradle 命令运行本扩展的 Robolectric 用例及 `assembleMainlineWristflow`，成功后复制 APK 并记哈希；日志／失败结果留输出目录。脚本不打开串口、不安装或烧录设备，不设置机器级环境。已有 `local.properties` 必须指向同一 SDK。APK 安装、蓝牙／通知访问授权、天气来源投递和真机验收是另外的操作；不能把构建成功当成手机或手表通过。

Breezy Weather v6.2.2 的[设置](https://github.com/breezy-weather/breezy-weather/blob/v6.2.2/app/src/main/kotlin/org/breezyweather/ui/settings/preference/composables/MultiListPreference.kt#L352)按广播 action 动态发现接收 App，非包名白名单；测试包保留该接收器。安装后重新进入数据共享选择页，选择完整测试包名，触发一次真实天气共享；实际发现、缓存和投递需要手机验证。
