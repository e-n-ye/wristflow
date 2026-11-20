# 稳定 B 的主动取消与接收超时

原黄山派从[第 7 代稳定 B，发布号 2026111802](ota-06-operator.md)进入本轮。前一次主动关闭手机蓝牙已覆盖断连；本次由电脑发送协议取消和定时数据块，使同一条 GATT 连接可以维持到接收超时。操作者确认手表供电和表盘状态，在被动串口监听开始后使用“设置 → 系统操作 → 固件升级”进入接收器。

## 判据与传输边界

电脑只持有分发包 `06-WristFlow-2026111802.bin` 开头不同的 4,096 字节，前缀 SHA-256 为 `d1196dfd05379cfc2b90bba137b555e273c7c657afee9f53bddcfa57a97bfbd2`。请求固定为 START、块 1、协议取消、块 2，再 START、块 1、块 2、块 2。该流程不能选择其他块或发送 END。板载 COM5 只收 UART，没有发送命令、脉冲复位或进入 ROM；本轮未重新读取整片 NOR。

接收器源码使用 60 秒的接收期限、120 秒的空闲返回期限。取消消息没有应用层回执，必须以随后的数据应答判断进度是否清除。两种情况都要求在同一连接上得到状态 4、重传位 0、已收块数 0。超时前另有一次正向检查，以排除从一开始就无法接收第二块。

## 主机验证

锁定 SDK 的 Serial 类别 1 使用分片标志 0/1/2/3，首片含消息长度。主机 `ota_release` 测试程序接收相同 DFU 字节，运行项目验签、CRC 和接收状态机；长静默在主机中模拟为 worker 的清理调用。RT-Thread 实际定时行为由下述真机结果判定。

```bash
cmake --build artifacts/host-ui-build --target ota_release
ctest --test-dir artifacts/host-ui-build -R '^ota_release$' --output-on-failure
py -3.12 -B scripts/ota_interruption_peer.py simulate --authorize simulation --receiver artifacts/host-ui-build/ota_release.exe --out artifacts/ota-07-interrupt-peer-sim-001
WRISTFLOW_OTA_RELEASE_TEST=artifacts/host-ui-build/ota_release.exe py -3.12 -B -m unittest discover -s tests -p test_ota_interruption_peer.py -v
```

原生 CTest 和 3 项专用 Python 用例通过。最小 MTU 的模拟链路重新组包后送入已编译的接收器。提前探测、意外断连和额外的数据帧均不能通过原始证据检查。OTA Python 回归共 229 项，58 项因需要其他外部材料跳过。未重新构建或部署手表固件。

## 真机证据

Product 在保存代 251 发出升级交接意图，接收器广播独立地址。Windows Bleak 3.0.2 与该地址只建立一次连接，重新枚举服务，使用数据特征 handle 19。48 次特征写入构成上述 8 条固定消息，收到 7 条通知。原始分片逐条重组核对，覆盖消息内容、句柄、应答与发送时刻。

| 阶段 | 观测 |
| --- | --- |
| 取消 | 第一块成功回执至取消后第二块的拒收回执相隔 0.187 秒，远早于接收超时。控制器和板端均记录状态 4、已收 0 块。 |
| 超时前 | 第二轮重新 START 并收到第一块成功回执。静默 45.031 秒开始发送第二块，手表确认累计 2 块。 |
| 超时后 | 从第二块回执静默 65.000 秒，再发相同的块 2，手表返回状态 4、已收 0 块。中途无断连、重新订阅或新的 START。 |

时间取自收到前一通知至发出下一请求首分片的单调时钟。这个窗口给出“45 秒仍接受、65 秒已超时”的真机边界；不能从两个探测点推断精确到毫秒的到期时刻。主工作树原始文件为 `artifacts/ota-interruption-stable-b-device-001/peer/ble.jsonl`，SHA-256 为 `81045471946fe019a81e83d908eda75386938dd903f955be65ff2c98144c048d`。

电脑断开后，手表报告接收器空闲退出、普通启动，并恢复亮度 50、diffusion 表盘、呼吸时长 1 和第 56 代布局的 3 页。启动标记为 `[ota-product] slot=B release=2026111802 trial=0 services=1`。本轮 UART 没有 END、安装或新镜像提交。被动串口原流 `passive/uart.bin` 为 5,662 字节，SHA-256 为 `bdc00f3f9d7d9d3c6268b2d9db9977ef286b5a6acb4abb0d85f3d1d75e137f42`；BLE 会话关闭、串口关闭、worker 退出 0。现场入口为 `scripts/ota_interruption_window.py`，需单独确认测试范围。

## 写入阶段断电的边界

已装接收器在校验完整包后由稳定槽计算 `target = 1 - stable`，先擦目标槽尾页，再写正文和签名，最后才提交启动记录（`apps/ota_loader/src/probe_port.c`）。当前 B 稳定，直接用有效整包进行写入阶段断电可能破坏原 A。现有 06 有线恢复仅允许恢复约定的 B 页和两个启动记录，无法修复已擦除的 A 页。保留原 A 是已确认的约束。

本轮使用 USB 和电池同时供电。用户确认手边只有板载 USB 和电脑，没有可观察电源波形的设备。按用户随后确认的收尾范围，实际擦写固件或提交启动记录期间的供电中断均暂缓，未做真机验证，不计入通过项。设备保留稳定 B 的 06 版本。[本轮交付结论](../../OTA-PREPARATION.md#ota-acceptance-close-20261120)
