# 设置固件上板记录（2026-09-26）

板型 `sf32lb52-lchspi-ulp`；当前端口 `USB-SERIAL CH340 (COM5)`，设备 ID `USB\VID_1A86&PID_7523\6&A8355E6&3&1`。使用固定 SDK v2.5.1（`421126d9`）及 sftool 0.2.5。两次均仅向 `0x12010000` 写 bootloader、`0x12020000` 写 main、`0x12000000` 写 ftab，命令带 `write_flash --verify`，退出码 0；没有整片擦除或写设置区。主机串口 1000000 baud、8N1、无流控。原始烧录、启动和交互输出保存在当前工作树的忽略目录 `artifacts/hardware/product-settings-20260926/`；其中 `initial-*.log` 属首版，`fix-*.log` 属修复版。

在 `apps/product/project/build_sf32lb52-lchspi-ulp_hcpu/` 中执行的两次命令相同，仅镜像内容不同：

```text
D:/MY_Desk/project/wristflow/.tools/sifli/tools/sftool/0.2.5/sftool.exe -p COM5 -c SF32LB52 -m nor -b 500000 --connect-attempts 3 --after soft_reset write_flash --verify bootloader/bootloader.bin@0x12010000 main.bin@0x12020000 ftab/ftab.bin@0x12000000
```

| 镜像 | 首版 SHA-256 | 修复版 SHA-256 | 字节数（修复版） |
|---|---|---|---:|
| bootloader.bin | `e59d70db44367d524fbb46a8a014ce0092462e86959d101a8e2e00ad27bb5e21` | 同左 | 58496 |
| main.bin | `8bb1b12416c4f74a995a8db38d136a6b3939be6d1621807fb5dcc0445441417e` | `5b9b00eea16dd20fb47600b564bee633042fa10382e426aa63a7c586ae9f2d11` | 3388992 |
| ftab.bin | `e03576c0408d46d9a84a03e69f7b866b0505c64f8bc7d362aad267083645afe1` | `4f1e1b856ac9054c102d20f29a4f0975a902081ed9fb30f0124d67c4cc4e9731` | 11288 |

首版烧录前 260 个工程源和 22 个产物逐项匹配构建 JSON，SDK 与两个子模块均匹配锁文件。启动时 FlashDB 恢复旧设置（亮度 25、diffusion），CO5300/FT6146 打开。用户确认设置十项目录可滚动、从表盘管理选中后直接回表盘、5 秒为约 4 秒正常亮度加 1 秒变暗，变暗首次触摸和 KEY1 只提亮。交互 UART 记录表盘与亮度设置成功保存、变暗/关屏以及两次 LCD/触摸重新上电。用户随后确认持续亮屏状态在设置和控制中心一致，但等待约 5 分钟后黑屏，USB 保持连接且 KEY1 无法唤醒，断电重连后恢复。该故障未被采到发生前的连续串口日志；重新打开串口会触发设备复位，复位后的正常启动不能证明故障原因。

同轮发现 Product 应用列表左边缘右滑不能回表盘。源码原因是导航层将所有 launcher 手势排除；改为仅演示版蜂窝 launcher 排除，产品列表/网格接受左缘右滑。针对两种布局补真实指针回归，12/12 主机测试通过。黑屏路径另改为 KEY1 按下即触发唤醒、吞掉该次随后产生的点击；原版仅等待 `BUTTON_CLICKED`，长按不会产生该事件。此改动不能单独证明上述短按失效的根因。

修复版直接用已安装的官方 SDK 环境执行 `scons --board=sf32lb52-lchspi-ulp -j6`，退出码 0。隔离工作树内运行公共 `Build.ps1` 曾因没有本地 `.tools/sifli/sifli-sdk-env.json` 在进入 SCons 前退出 1；随后加载原项目锁定环境完成构建。修复版三镜像已再烧录并校验；受控 RTS 复位后的启动日志显示构建标识 `2a0bd8b9`、设置恢复、显示/触摸初始化和按 5 秒配置关屏。用户随后确认修复版应用列表左缘右滑回表盘、黑屏后 KEY1 可唤醒，持续亮屏 5 分钟测试通过。未分别核对列表/网格两种布局，也没有故障前后连续串口记录；旧版无法唤醒的具体根因仍不能确定。手电筒退出、编辑草稿及秒表长睡恢复尚待验证。PM/BLE 仍关闭；黑屏不代表低功耗通过。单击亮屏继续隐藏，需低功耗触摸中断证据后再开放。
