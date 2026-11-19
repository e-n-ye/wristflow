# 06 健康确认与普通重启的主机回放

基线为 `2b2f35c7`。使用[05 真机回滚读回](ota-05-dma-finished.json)的两份完整 8 KiB 启动记录，以及[已验证签名的 06 发布包](../2026-11-18/ota-product-health.json)。本次只编译主机测试，实表仍运行原 A。

| 输入 | SHA-256 |
| --- | --- |
| 第 3 代 TRIED B 原始单元 | `db6f4534952f61e7b6e8fc48668ffab4692e0243cf83d8e015f74b6abca7420a` |
| 第 4 代 STABLE A 原始单元 | `a9608a443f23be490dd9e27c55754fe3cc6abb31e80972689302542f7a8abd50` |
| 手机工具包中的 `06-WristFlow-2026111802.bin` | `8a9306b8b2d627c4a198c05102488b8e818543c3c1b12081b743c689019f5ee3` |

本机原始文件在主工作树的 `artifacts/`。已核对手机 ZIP 中的 06 与原打包文件逐字节相同，正文长 3,033,572 字节，摘要与包头一致。测试第三个输入是这个已核实包的 `wire[58:98]`，依次包含小端正文长度、发布号及 32 字节正文摘要。

```bash
py -3.12 -B - <<'PY'
from hashlib import sha256
from pathlib import Path
from zipfile import ZipFile
with ZipFile("artifacts/WristFlow-Android-OTA-20261118.zip") as kit:
    wire = kit.read("06-WristFlow-2026111802.bin")
assert sha256(wire).hexdigest() == "8a9306b8b2d627c4a198c05102488b8e818543c3c1b12081b743c689019f5ee3"
out = Path("artifacts/ota-06-health-replay-001")
out.mkdir(exist_ok=True)
(out / "identity.bin").write_bytes(wire[58:98])
PY
cmake --build artifacts/host-ui-build --target ota_journal
ctest --test-dir artifacts/host-ui-build -R '^ota_journal$' --output-on-failure
media=artifacts/ota-05-dma-finished-inspect-device-003/media
artifacts/host-ui-build/ota_journal.exe --replay-health "$media/key-00da0000.bin" "$media/key-00da2000.bin" artifacts/ota-06-health-replay-001/identity.bin
```

两个测试均退出 0。生产日志模型从第 4 代稳定 A 登记第 5 代 READY B，在另一单元写入第 6 代 TRIED B 后选 B。测试提供与该记录匹配的健康字 `0xa28965cf`，得到第 7 代 STABLE B。模拟普通重启时仅保留两个 NOR 单元，重新加载日志，RTC 输入为零，仍选 B，擦除和编程次数都为零。健康字由主机注入；新版 Product 能否在实表上满足屏幕、电压、记录与服务条件并提交健康字，留待现场观察。

现场沿用封存的整片备份，以 05 回滚后的修正 boot、B 内的 05 和第 3、4 代启动记录作为测前状态。按片内偏移，06 在 B 擦写的正文范围为 `[0x420000,0x705000)`，签名尾页为 `[0x81f000,0x820000)`，两份启动记录为 `[0xda0000,0xda4000)`。其中额外一页 `[0x704000,0x705000)` 超出 05 的读回范围，在已有整片备份中的 SHA-256 为 `a3774230afbc30797b638fc239a073eb65228b04702f731194cfabd8b8191ab2`。进入接收器前需读回该页并与备份核对。

有线恢复复用已有的分块写入和读回流程。05 工具的 `ROLLBACK_BODY_END` 和 `RESTORED_SHA` 固定于较短的旧正文与 05 安装前的整片，06 的操作入口须绑定上述测前字节和实际范围。真机通过需要同一轮的 B 试启动、健康交接、稳定 B 启动日志，以及启动记录独立读回和随后的普通启动再次进入 B。

## 06 有线关键区工具

原始备份、05 真机读回与 06 签名包在电脑上生成两份参考模型。模型中的用户分区来自历史备份，不代表当前设置和运行记录，工具也不恢复这些分区。

| 操作 | 模型 SHA-256 | 读回条件 |
| --- | --- | --- |
| 首次进入，或断连后再次进入 | `116219ecabdcf9c5ce01043e821ad3885d86e6251a199e5a166f1ede23bc95ed` | 当前为第 4 代稳定 A，B 保留 05。全部关键区吻合且操作者就绪后才请求接收器。 |
| 独立检查成功升级后的普通重启 | `3e24c4b965433b6225b001c3a47744e833594c7663ba25cf7a9efa097f18ea47` | 第 7 代稳定 B、第 6 代已试 B、签名尾页和 B 正文页吻合。普通重启要求 OTA 备份字为零。 |
| 有线恢复 | 同首次进入 | 读回保护区；按顺序恢复第 4 代稳定 A、第 3 代已试 B、05 正文相关页及签名尾页，再独立核回。与目标相同的块跳过写入。 |

三种操作均先读 59 块共 3,551,232 字节，含新增的 4 KiB 页。恢复写入许可只涵盖上述两个启动记录和两段 B 区域。恢复的分块发送、独立读回、取消及串口关闭沿用原流程。

主工作树内的原生构建为 `artifacts/ota-06-media-build-002/result.json`，可执行文件 SHA-256 为 `fa64f81e735c8610123f30a17da4664d46371c48aafac8a406b6d5794ad02537`。锁定工具链通过 11 个 Rust 单元测试，release 构建成功。模拟结果在 `artifacts/ota-06-media-check-002/`。首次进入、断连后使用相同关键区再次进入、成功后的独立检查各通过 59 块比对，零写入。额外页末字节改变、06 已安装后再次尝试从 05 进入、仍在 A 却要求确认 06 三种输入均拒绝。模拟注入的是预期媒体状态，手机实际断连和 B 启动仍需采集证据。

```bash
py -3.12 -B scripts/ota_health_media.py --prepare artifacts/ota-06-models-001
WRISTFLOW_FIRST_INSTALL_BUILD=artifacts/ota-06-media-build-002/result.json py -3.12 -B -m unittest discover -s tests -p test_ota_first_install.py -k test_health -v
```
