# Flash核算与官方EZIP图片试点

本页保存方法、版本证据和试点边界；活动项及唯一下一步见[STATUS](STATUS.md#current)。本轮从干净主目录`25521af`接续用户选定的容量闭环，不改SDK锁、分区或指南针产品流程。

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

通知字库`notification_22_data.o`为3,832,051B，其中位图3,319,993B；13张小时图含未知图，每张像素192,888B、含描述符192,916B。字库范围不在本轮修改，不能把图片压缩率套到字体或整机。

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

源码检查、官方工具离线往返、固件编译／字节绑定、实机显示分别判断。当前没有本轮设备操作；未验证EPIC真实显示、三图切换、睡醒恢复、帧时间、运行内存峰值、功耗或续航。已有SDK配置不等于上述设备项目通过。设备窗口需另行确认实际版本、连接和供电后进行，不能沿用指南针的旧窗口。
