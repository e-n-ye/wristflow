# Flash核算与官方EZIP图片试点

本页保存方法、版本证据和试点边界；活动项及唯一下一步见[STATUS](STATUS.md#current)。三图试点从`25521af`开始，后续6500字裁剪从其交付`d6af724`接续；两轮均不改SDK锁、分区或指南针产品流程。

## 2026-10-28 三图与字库上板显示验收

本轮由27日晚延续，用户确认同板屏／原电池＋电脑USB／接线固定且人在旁，允许烧录和必要校时。显式`Build.ps1 -Example product -ResourceProbe`生成诊断版`16a6c8cd…`；普通构建关闭该开关，ELF不含资源探针或`wf_resource`命令。`product_resource_probe`拥有FinSH邮箱、缓存、资格和清理，`resource_probe`仅在UI线程挂载官方视图；main保留五处接入。每视图60秒、KEY1／高优先级事件／资格丢失退出，AOD诊断互斥，不改RTC或通知历史。

复用Console／Journal与RecordedSession增加`resource_acceptance.py`，默认模拟，设备模式绑定构建／下载／完整BIN／源码ZIP／CH340端点。六步浏览器模拟完成；相关C主机4/4、适配6/6覆盖资源指针、字体继承、期限／释放、丢ACK、取消、异会话拒停和关闭失败报告。准备时修复旧草稿图片二级指针、错误LVGL参数及未接入问题；初版把实现放入main，经用户要求移到独立模块。编译／工具失败原记录保留。

诊断版三镜像write＋verify成功，593源码／29产物逐哈希核验并ZIP保全。设备四窗人工确认1点／7点／未知图完整，以及通知常用汉字、英文数字、标点、三个BMP外二级字与范围外占位；分别在14.1／9.3／6.6／20.2秒内按归属停止，最终active0／pending0、无恢复需求、端口／租约关闭。原UART有一条touch buffer overwrite警告，无assert或冷启动标记；不把流程passed扩成全机无故障。真实60秒到期未触发，完整6500字未逐字上板。

普通Product恢复、最终末态及全部指纹见[精简证据](evidence/2026-10-28/resource-display-hardware.json)。本轮不测帧时间、CPU、运行峰值、功耗或续航；余下10张小时图约1.84MiB仅作为后续候选，透明通道大图另核官方支持，不在本轮扩压。

## 2026-10-27 常用通知字库裁剪

按[用户决定](BASELINE.md#common-font-20261027)，保留《通用规范汉字表》一、二级6500汉字，加英文、数字和常用标点／符号；实际源TTF和官方生成C均为7174码点（6500＋674）。固定Unicode17.0.0 Unihan的kTGH／2013序号1–6500，提交清单／许可／导入哈希；日常生成离线使用。锁定源字体支持全部6500字，含U+20CD0／U+25ED7／U+2C27C三个BMP外二级字，XML导出范围相应扩至0x2ffff。范围与再生成方法见[字体说明](../ui/xml/fonts/README.md)。

这轮是覆盖裁剪，22px／2bpp不变，官方生成命令仍带`--no-compress`、bitmap_format0；Product字体解压开关继续关闭，没有新增RLE解码步骤。范围外字形使用既有占位，正文内容不改。EZIP／EPIC图片硬解也有时间、带宽与能耗代价；未上板不能据硬解配置宣称零开销或帧率不变。

| 相对三图试点Product | 裁剪前B | 裁剪后B |
|---|---:|---:|
| 通知字体链接资源 | 3,832,051 | 914,067 |
| 全部UI字库 | 4,233,292 | 1,315,308 |
| Product BIN | 7,788,120 | 4,870,136 |
| 8MiB应用区余量 | 600,488 | 3,518,472 |

Product净减2,917,984B（约2.78MiB），余约3.36MiB；SDK静态RAM仍253,980B。图片资源2,510,236B、其他代码／数据／对齐1,044,592B均与三图版相同。UI Demo BIN4,570,344B，余3,818,264B。以上由实际ELF LMA／BIN及map核算，不使用SDK重复汇总。

官方Editor2.0.1／预览LVGL9.4.0完成导出与预览编译，通知详情可见。清单3项及相关Debug6/6通过，C运行期逐字验证6500字且检查3个补充平面字、plain格式和明确范围外占位；TTF与C覆盖经独立只读核验均为7174。Product`20261027-204749-852`、UI Demo`20261027-204946-730`编译与字节绑定另见[本轮精简证据](evidence/2026-10-27/common-notification-font.json)。

实际390×450主机快照已检查普通通知与缺字显示。20份前后快照中4份相同，其余每帧至多470像素有差异，不能宣称逐像素一致。样例“明天下午一起去图书馆，记得带上笔记本。”的18个唯一字符中，起／记／笔／句号的官方导出bitmap与box高／偏移有微差；advance均不变，行高31／基线7不变，另14字完全一致。源TTF这些字的轮廓与水平度量相同；重新栅格化差异的内部原因未定，不归因测试时序，不声称所有帧差均已由这4字解释。截图与负结果保留，未测本板帧率、负载、功耗或实际显示。

以下保留三图先行试点的账单与方法，字库广覆盖数值属于该历史对照版本。

## 核算口径

`scripts/flash_audit.py`先复用`lab_check.check_build`核对当前源码、SDK及产物字节，再读取已验证的GNU map的`ROM`区域与ELF的`LOAD`物理地址（LMA）。应用占用是最高Flash加载末端减分区起点，并要求它等于`main.bin`长度。复制到SRAM的初始数据仍占Flash；零初始化RAM不加到Flash。

只对已链接、地址不重叠的UI字体／图片输入节归属计数；对齐、SDK字体、代码和其他数据进入余额。丢弃节不计入。GNU合并字符串的输入节可能重叠，SDK汇总也存在重复计数，不能将这些总数直接当成Flash容量。

| 基线Product `d0a461cb…` | 字节 |
|---|---:|
| 应用区容量 | 8,388,608 |
| 实际加载跨度／BIN | 8,355,968 |
| 剩余 | 32,640 |
| 已链接UI字库 | 4,233,292 |
| 已链接UI图片 | 3,078,092 |
| 其余代码／数据／对齐 | 1,044,584 |

当时通知字库`notification_22_data.o`为3,832,051B，其中位图3,319,993B；13张小时图含未知图，每张像素192,888B、含描述符192,916B。三图先行轮未改字库，不能把图片压缩率套到字体或整机。

复核基线使用原始`closure-product-20261027-145839-266.zip`证明Git换行差异，589源码／29产物通过；不是忽略内容差异。原始报告位于主目录忽略的`artifacts/flash-ezip-pilot-20261027/`，精简结果在本轮证据中。

## 官方路线与选择

锁定SDK的`example/multimedia/lvgl/ezip_v9/README.md`列出黄山派，`tools/png2ezip/ezip.exe`支持LVGL9；当前Product已有`LV_USE_EZIP`、EPIC和EZIP decoder。采用官方工具和`Env.ImgResource`，不替换片上解码器，不修改厂商树。

HAL文档`docs/source/zh_CN/hal/ezip.md`明确EZIP直接绘制支持混叠／缩放、不支持旋转。本轮只选`hour_1`、`hour_7`、`hour_unknown`：表盘只换源，无旋转／缩放；旋转指南针指针保留原资源。`hour_10`缩略图有stretch，但不在这三图试点内。

`ui/xml/SConscript`仅在设备构建替换三图。官方XML、PNG和导出C保留，编辑器及主机模拟器继续使用官方原始输出。SCons以导出C及`scripts/ezip_rgb565.py`为依赖，读取受限的packed RGB565像素并生成构建目录下的临时RGB PNG，再由官方EZIP工具生成C；不解析XML或生成布局，不手改生成C。转换参数通过SCons Value依赖显式纳入增量重建。

这样处理是因为本轮实测直接转换原PNG与原LVGL导出存在RGB565量化边界差异。临时PNG展开既有RGB565通道后，SDK压缩／解压往返与既有像素逐项一致。辅助脚本拒绝非RGB565、非紧密stride、尺寸或载荷不符，不将其推广到alpha资源。`-section rodata`使输出被原链接脚本的`.rodata*`接收，不建立孤立ROM3节或挪用其他分区。

## 离线试点与复现

官方工具本次返回版本`2.6.4_20260414_dyn_app_support_separate_dir`。输入路径须使用Windows反斜杠；本机工具在斜杠路径写入失败时仍可退出0，且成功转换有时汇总`convert_num 0`。探针同时检查逐文件成功标志、输出文件、V9头／C载荷一致及解码像素，不只看退出码或计数。

```bash
python -m unittest discover -s tests -p test_flash_audit.py -v
uv run --with pillow==11.3.0 --no-project python scripts/ezip_image_probe.py --output artifacts/flash-ezip-<new-run>
pwsh -NoProfile -File scripts/Build.ps1 -Example product -Jobs 4
pwsh -NoProfile -File scripts/Build.ps1 -Example ui_demo -Jobs 4
```

探针输出目录必须新建，保留失败和旧证据。Pillow只用于主机往返检查，不加入锁定SDK环境；设备构建的预处理仅用Python标准库。账单命令为`python scripts/flash_audit.py --result <实际result.json> --readelf <已安装arm-none-eabi-readelf> --output <新账单.json>`；旧源码确为Git换行时才提供原始`--source-archive`。

| 图片 | 原像素B | EZIP载荷B | 逐像素往返 |
|---|---:|---:|---|
| hour_1 | 192,888 | 3,735 | 一致 |
| hour_7 | 192,888 | 4,431 | 一致 |
| hour_unknown | 192,888 | 2,642 | 一致 |
| 合计 | 578,664 | 10,808 | 289,332像素一致 |

载荷减少567,856B；整机BIN实测净减567,848B，差8B来自链接布局／对齐余额变化。主机4项检查覆盖LMA与运行地址区分、丢弃节／合并字符串排除、重叠资源拒绝和预处理格式／长度守卫。

## 双目标编译结果

Product `20261027-195252-025`／BIN `e6b85838…`为7,788,120B，应用区余600,488B（约586.4KiB）；UI字库4,233,292B不变、图片2,510,236B、其余代码／数据／对齐1,044,592B。SDK静态RAM253,980B与基线相同，运行峰值未测。三份真实构建的压缩载荷与离线往返输入逐字节相同；旧三图raw对象未重复链接。

UI Demo `20261027-195340-669`／BIN `865c52bb…`为7,488,328B，应用区余900,280B。两目标构建退出0，589／526源码及29／22产物严格字节复核，完整ZIP另含预处理及核验脚本。首轮Product `20261027-194955-729`因辅助脚本的variant相对路径失败，改成源目录绝对File依赖后通过；失败日志保留。原FinSH类型转换、链接RWX／syscall／ftab入口提示保留，未发现新增图片编译警告。

源码审查和参数依赖修复、工具版本／哈希、完整命令与本机归档指纹见[精简证据](evidence/2026-10-27/flash-ezip-pilot.json)。未改公共构建入口或SDK／工具链，不启动云端全量基线；无XML内容变化，不重复GUI导出。

## 验证边界

源码检查、官方工具离线往返、固件编译／字节绑定、实机显示分别判断。27日软件轮没有设备操作；28日样本显示与切图结果见上方新记录。帧时间、运行内存峰值、功耗或续航仍未测，SDK配置及主观显示不能替代测量。
