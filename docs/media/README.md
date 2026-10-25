# 首页素材说明

本目录保存 README 直接展示的精选素材。2026-10-25首版使用用户当日提供的5张原型照片及已有7张WristFlow主机 LVGL／UI Demo 渲染截图，未引入第三方项目照片、Redmi参考图或个人通知。PNG副本逐字节复制；JPEG发布副本仅移除APP1／APP13／注释元数据，原始编码图像扫描字节保持一致，没有裁切、缩放、合成或修饰画面。原分辨率JPEG原件保留在本机`artifacts/showcase-20261025/photo-originals/`，不依赖临时剪贴板路径接续。

| 文件 | 内容与类型 | 所属记录 |
|---|---|---|
| [watchface-device.jpg](watchface-device.jpg) | 表盘实拍，保留USB／原型接线和`~100%`估算显示 | [用户照片记录](../REPOSITORY-SHOWCASE.md#photos-20261025) |
| [control-center-device.jpg](control-center-device.jpg) | 控制中心实拍，图标存在不表示所有快捷功能已实现 | 同上 |
| [activity-device.jpg](activity-device.jpg) | 活动页面实拍，步数0、其他未提供项显示未知 | 同上 |
| [countdown-device.jpg](countdown-device.jpg) | 倒计时运行实拍，00:02:55为画面状态 | 同上 |
| [aod-device.jpg](aod-device.jpg) | AOD实拍，USB在接；不据此推算电流和续航 | 同上 |
| [control-center.png](control-center.png) | 控制中心三状态主机渲染，示例时间／未知电量；找手机和排水图标不代表已实现 | [2026-10-22控制中心记录](../evidence/2026-10-22/control-center-hardware.json)中的render_local |
| [activity.png](activity.png) | 活力总览，UI Demo演示数据；不是真实健康指标 | [2026-10-19活力页面](../ACTIVITY.md) |
| [steps.png](steps.png) | 步数与小时图表，UI Demo演示数据；截图不是设备计步测量 | [活力数据边界](../ACTIVITY.md#数据边界) |
| [countdown-picker.png](countdown-picker.png) | 倒计时预设，主机LVGL渲染 | [倒计时专题](../COUNTDOWN.md)及早期六状态快照 |
| [countdown-running.png](countdown-running.png) | 倒计时运行，主机LVGL渲染，示例时间 | [倒计时专题](../COUNTDOWN.md)及早期六状态快照 |
| [brightness.png](brightness.png) | 2026-10-23亮度页面主机渲染，自动模式演示状态；截图本身不证明光感效果 | [亮度软件记录](../evidence/2026-10-23/brightness-page.json) |
| [compass.png](compass.png) | 2026-10-24指南针主机渲染，305°为演示输入，保留未校准提示 | [指南针软件记录](../evidence/2026-10-24/compass-live-data.json) |

[manifest.json](manifest.json)记录文件大小、像素尺寸、当前及原始SHA-256、原本机路径、素材类别和相关证据。原始路径仅作本机追溯，不是GitHub可用链接。既有截图未逐图保存捕获源码提交；新照片拍摄时也未独立读取板上身份，因此capture_source_commit明确为null。相关软件／硬件记录只作语境，不能当作每张图的精确BIN身份。首版明确区别实拍与主机渲染，不把照片收集当作新的功能验收。12个发布素材共23,948,151B，JPEG保留原3072×4080分辨率；首版以原画面可追溯为先，未生成缩略图或重新有损编码。

这些界面使用项目内字体和图片，原有来源与许可见[字体说明](../../ui/xml/fonts/README.md)及[图片说明](../../ui/xml/images/README.md)。本轮未对照片／截图或自有代码新增再授权声明。用户已明确视频后置，未来单独规划完整功能演示；参考[拍摄计划](../REPOSITORY-SHOWCASE.md#capture-plan)，不作为首版图片交付的阻断。
