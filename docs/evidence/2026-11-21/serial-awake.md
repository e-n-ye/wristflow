# 黄山派 Product 2026112002 串口亮屏验收

2026-11-21，原电池与板载 USB 保持连接，主机绑定 COM5、CH340 位置 `1-1.1`。本次按用户授权在手机安装正式 Product。使用主工作目录本机 `artifacts/product/20261120-203458-146/result.json` 的成功构建，核对锁定 SDK、源码与 main 正文。主机另运行 `python -B -m unittest discover -s tests -p test_serial_awake.py -v`，3 项协议测试通过。

## 升级与提示

用 `scripts/ota_release.py --build-result artifacts/product/20261120-203458-146/result.json --release 2026112002 --out-dir artifacts/serial-awake-release-2026112002` 签包，经 USB ADB 复制至安卓 `Download/WristFlow-2026112002.bin`。两端包长 3,035,242 B，SHA-256 为 `3f45837bbd74896f9b91dbfc995afb3defead55860e9011299c2c93c227e5c7b`。正文 3,034,888 B，SHA-256 为 `8acd193e7770ca6b62754121997b9b5b88177cccea44c8c8ebb429957d89233a`。

操作者从表内升级入口进入，用 SIFLI BLE 发送。被动 UART 顺序为 END 接受 1,483 块、开始本地安装、提交新版本试启动、A 槽 `trial=1 services=1`、健康交接、A 槽同版 `trial=0 services=1`。用户点选“看到了「更新成功，版本 2026112002」”。另反馈 USB 持续接入时，复位重启会重弹充电画面，曾因此漏看表盘底部的升级提示。

## 临时常亮

在上述稳定版本运行 `scripts/serial_awake.py` 的 start／stop 实现。两轮前分别捕获 `[product] screen off`，随后均捕获 `wake event=diagnostic`、`screen on`、匹配令牌的 `active`。下表时间为相对各轮 start 回执的主机观测时间，按秒取近似值。

| 请求 | 保持、释放与恢复的观测 |
|---|---|
| 120 秒，令牌 `f52c7929` | 约 20 秒时 `wf_pm status` 报 `off=0`。同一令牌收到 `stopped`，释放约 10 秒后捕获自然息屏。 |
| 24 秒，令牌 `47298525` | 约 12 秒时 `off=0`。约 27.5 秒时同一令牌已为 `inactive`，约 33.6 秒时捕获自然息屏。本次未直接记录到期回调触发的精确时刻。 |

操作者在问题框确认两轮都看到自动亮屏、保持及最后息屏。到期回到原显示策略，显示按原超时变暗和关闭。各串口会话均已关闭。该实验未测物理深睡、电流或电池续航。

原始材料在本机 `artifacts/serial-awake-20261121/`。手机升级的 `phone-001/uart.bin` SHA-256 为 `c44126aa2e578fdbf209d967e34e8af0fa589c68fb0cd8da6f7ac1c0e8589a46`；两轮的 `run-001/steps.json` SHA-256 为 `c0f27872a865935d631ca4de60c03312f8785a36d6365537df6c12eff6a9b77b`。可按每轮目录中的 `awake.json`、`uart.bin` 和 `events.jsonl` 复核命令、令牌、显示状态与串口关闭记录。
