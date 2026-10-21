# GitHub 成果展示与参与入口

2026-10-21按[本轮方向](BASELINE.md#next-six-20261021)先做参考调研。这里保存参考及交付契约，活动状态只见[STATUS](STATUS.md#current)，阶段依赖见[任务契约](ARCHITECTURE-REVIEW.md#next-six-20261021)。本轮读取公开仓库README及提交标识，没有复制第三方图片／代码，也没有创建展示网站。

## 已核查的参考

通过GitHub CLI的`gh api repos/<owner>/<repo>`、`commits/<default_branch>`及`contents/<README>?ref=<sha>`读取；以下SHA固定本次观察，主页持续变化。参考的是信息组织，不据这些项目的硬件／功能推断WristFlow能力。

| 仓库及固定证据 | 可借鉴之处 | WristFlow的取舍 |
|---|---|---|
| [InfiniTime](https://github.com/InfiniTimeOrg/InfiniTime)；[README@b02e59b](https://github.com/InfiniTimeOrg/InfiniTime/blob/b02e59b3fdca525c49bc1043031d97a5b5f8f98a/README.md#L1-L64) | 首屏表盘拼图；面向新用户的Getting started／应用／表盘，再进入开发文档和伴侣端 | 主要结构参考：先让人看到产品，再给“了解／体验／开发”入口；清楚区分固件、手机伴侣与开发环境 |
| [SmartKnob](https://github.com/scottbez1/smartknob)；[README@4eb9883](https://github.com/scottbez1/smartknob/blob/4eb988399c3fda6ffd3006772856093dfe9adb86/README.md#L1-L104) | 一句话解释项目；功能列表、演示视频、硬件图及组装／使用入口紧邻；明确Current status限制 | 展示交互短动图、实拍与硬件路线；明确原型阶段及未验能力。它的硬件方案与构建工具不迁入本项目 |
| [wasp-os](https://github.com/wasp-os/wasp-os)；[README@9128cdb](https://github.com/wasp-os/wasp-os/blob/9128cdb675e52ee4899fec1dc0858bc01c28180f/README.rst#L151-L173) | 将实机照片与模拟器应用截图分别说明，按应用展示图集；文档与应用开发指南入口明确 | 借鉴图集及素材来源标注。该版README开头正招维护者、说明原作者不再积极维护且旧二进制撤回，因此只作呈现参考，不作为活跃发布流程范本 |
| [Bangle.js Apps](https://github.com/espruino/BangleApps)；[README@5d36f05](https://github.com/espruino/BangleApps/blob/5d36f053cb93208bef5f9215880e92840bdfb3dd/README.md#L24-L107) | 从“第一个应用”示例进入开发；明确截图／动图获取方式、测试及应用元数据贡献路径 | 借鉴最小贡献步骤、截图与测试要求；本项目沿用编译期功能和现有PR流程，不引入它的运行时App Loader／安装器 |

推荐组合：InfiniTime的入口层次＋SmartKnob的演示优先＋wasp-os的有来源图集＋BangleApps的小贡献入口。无需先做独立网站、重搭文档系统或堆满状态徽章。

## README 首屏和内容顺序

1. 名称、一句话定位、实际板屏与原型阶段，配一张本项目真实主图；中文主说明，短英文定位便于外部开发者识别。
2. 4–6张已实现页面缩略图＋一段约10–20秒交互动图／视频链接，优先表盘／组件环、通知、天气、活动和计时；后续控制中心等有可核对版本再补。
3. 功能简表：提供功能与专题／证据入口，并注明源码／主机、目标编译、有界实机与未验证项。日常状态和下一任务仍链接STATUS，不复制活动队列；截图中的演示数据不能证明真实采集。
4. 两条最短路径：“PC查看UI”链接SIMULATOR；“目标板开发”链接BUILD／HARDWARE／FLASHING。明确模拟器、构建与设备操作各自范围，不在首页堆本机路径和长命令。
5. 参与开发：链接CONTRIBUTING、PROJECT_INDEX与ENGINEERING-MAP，列出文档、UI、主机回归、板载器件适配等可贡献方向；从一个小变更到验证／PR的步骤沿用既有流程。具体认领项以后指向维护过的Issue或STATUS，避免无人维护的第二份待办。
6. 简明硬件／SDK依赖、伴侣端入口、许可证和素材来源。仓库自有代码许可证及第三方SDK／字库／图片许可先核对，不能直接仿照参考仓库写GPL或MIT；不暗示付费许可／外部硬件对所有贡献都是必需的。

## 素材与验收

本轮三张用户照片是Redmi Watch 4参考，不能放入WristFlow成果图集。先从已有WristFlow记录定位素材；忽略目录中的图片要确认版本、隐私和展示价值，再将精选副本提交到例如`docs/media/`。不将聊天临时路径或未提交artifacts写作GitHub可访问图片。

每张图／动图记录来源类别（实机／模拟器／UI Demo）、对应源码或BIN、日期及必要的数据说明。正常产品截图优先；UI Demo确有展示价值时明确标注演示数据。裁切保留主要交互内容，避免通知个人内容、电脑账号、设备标识；不伪造功能状态，不用设计稿代替实机验收。

G1完成线：GitHub README直接可见图片、相对链接／素材大小适当、手机窄屏也能看懂功能和入口；读者能找到开发环境、一个最小验证及贡献流程。主代理核对所列功能对应证据，检查许可证事实、UTF-8与链接；纯文档／图片轮不触发固件构建或设备实验。当前README中的“尚未接电池”已过时，本轮仅修正这个事实并增加调研入口，完整图集按G1独立交付。

本轮文档验证：GitHub CLI 2.101.0读取上述四份固定README；Python 3.14.3对本轮7份改动Markdown严格UTF-8解码，检查替换字符及289个本地链接／锚点，通过；`git diff --check`通过。两项只读复核分别检查电量实验因果与阶段／素材／采购边界，无阻断。没有新增固件构建、主机行为测试、串口／烧录或实际采购，不能由文档通过提升硬件结论。下一有界软件闭环B1由STATUS衔接。
