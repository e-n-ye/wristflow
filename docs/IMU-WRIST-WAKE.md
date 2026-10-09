# IMU 与翻腕亮屏

<a id="imu-integration"></a>
## 2026-10-09 天气主线与 C2 同版集成候选

从已交付天气的 `main@64c7e43` 建立 `codex/imu-weather-integration`，在既有 C2 工作树继续；原 `codex/imu-wrist-wake@823e656` 和 PR #30 保留。本候选已推送并建立 [PR #39](https://github.com/e-n-ye/wristflow/pull/39)，因下述硬件失败保持 OPEN/DRAFT。只接入原 C2 的驱动、事件、设置 v5 与测试；固定负 X / 500 ms 行为沿用原版，响应偏慢的旧反馈保留为后续体验实验。下方 2026-09-28～30 的源码、编译及真机结果只证明各自原版。

冲突处置：Product 主循环保留最新天气周期刷新与 PHONE 快照/UI 刷新，加入 IMU arm、息屏恢复和变暗提亮；测试取天气与 IMU 的并集。shell 的天气分页/边缘返回、D3 逐槽目标判空与 C2 唤醒恢复同时保留。旧分支的阶段/采购文本不覆盖当前权威文档；历史构建与串口事实仍保存在本专题及原分支。

设置继续 24 字节：v5 用 byte 16 的 bit 0/1 保存勿扰/抬腕，读取 v4 时抬腕默认开启；旧 v4 固件不能读取 v5。本轮写入前板上为天气 `83a6bd2` 三镜像，现场已确认仍仅 USB、未接电池、屏幕朝上平放且手机正常连接。

候选源码 `a257d67d1ee84d89680306ab27307beeaa967b6c` 已完成源码并集审查，19/19 主机测试与 Product/UI Demo 官方 SCons 构建通过；615 个源/51 个产物重哈希一致，Product 主 BIN 7,627,220 B / `74c83862…`。完整命令/版本/产物见 [构建记录](BUILD.md#imu-integration-build)。COM5 CH340 三镜像 `write_flash --verify` 退出 0，记录 `artifacts/flash/product/20261009-185355-100/result.json`；启动确认 `flat=1`，仍仅 USB。以上尚未形成组合硬件通过结论；只使用本候选的新写入和运行证据，不沿用旧 C2 镜像验收本组合。

首组 3 次固定动作，用户报告 2 次可见亮屏，体感和旧版一样慢。原始日志在本工作树忽略目录 `artifacts/imu-integration/session/`：三个软件触发的 `hold_ms=521/516/523`，trigger 到 `screen on` 的主机接收间隔分别 297/297/312 ms。`screen on` 只证明亮度接口调用后的软件路径，不测量物理发光，不能以三条日志覆盖用户观察。随后请求单次对照，用户实际做了多次（准确次数未知），反馈无法准确感知一秒，要么不亮，要么快放回时才亮；日志有两段 candidate 未走到 trigger，随后一段 `hold_ms=518` / `screen on` 间隔 312 ms，缺少中间采样，不能判定具体失效条件。固定手持门槛本版未通过，不再继续补满干扰等验收组，也不合入。

500 ms 稳定姿态条件及随后显示恢复可能解释部分迟缓，尚无动作开始到可见画面的完整时间。用户认为达到 Redmi Watch 4 水平可能需要神经网络或新方案，此为个人看法，未确立算法选择或参数修改授权。后续诊断不要求用户准确计时，由设备记录短采样与恢复分段时间；本轮未修改算法。

独立恢复项：写入后 `connected=0 / subscribed=0`，用户重新连接 Gadgetbridge；日志确认 generation=1 的连接/订阅、真实天气 24 小时项/7 日项和 KEY1 恢复。用户确认温度/预报/接收时间恢复，返回重入与触摸正常；最后状态 `cached=1 / request=0`、queue dropped/gaps/stale=0。四次 IMU status 的 `configured=1 / io_errors=1` 无增长；这不是整个运行期间零错误的证明。SDK 记录一次 touch buffer overwrite，用户未报告触摸异常，未归因或宣称已修复。采集约 896 秒、退出 0、无待执行计划，串口关闭；临时持续亮屏已由用户关闭。完整哈希及分项边界见[精简证据](evidence/2026-10-09/imu-integration.json)。

当前验证状态及唯一下一步见 [STATUS](STATUS.md#current)。本轮完成条件为相关主机回归、Product/UI Demo 编译及必要 USB 集成恢复验证；完整 N2、佩戴识别率、功耗和灵敏度优化分别保留，不以固定手持样本扩大结论。

## 旧 C1/C2 版本与范围

2026-09-30，范围经 Q1–Q9 确认。C2 手持实板触发验收已完成；变暗恢复的主观灵敏度和速度问题保留为体验优化项。分支 `codex/imu-wrist-wake` 从 `main 5c0b82c` 建立，工作树为 `C:/Users/13984/.codex/worktrees/imu-wrist-wake/wristflow`。

## 前置核对

主线源码与 PR #28 通过的完整云端运行 [36323967702](https://github.com/e-n-ye/wristflow/actions/runs/36323967702) 文件树一致：15/15 主机测试、5/5 固件构建。PR #29 仅补充该版本的 Product 复编译与 COM5 写入/verify 记录，没有固件源码改动，仍开放；见 [复验证据](https://github.com/e-n-ye/wristflow/pull/29)。QQ/微信通知曾短暂收不到后恢复，根因未知，不记作已修复。未发现已有证据确认、会阻断本增量的源码缺陷。

既有长测、五次重连联合矩阵、电流和续航仍后置；历史分项硬件结果不覆盖本增量。当前主目录的旧 bringup 分支及未提交改动保留。

## 已确认的行为与验收

- 两步交付：C1 先验证 ID、地址、轴方向、采样、供电状态和中断/休眠恢复；通过后 C2 接翻腕亮屏。没有可用中断唤醒路径时保留诊断、暂缓亮屏，不用息屏持续轮询替代。
- 本轮手持演示，尚未固定佩戴。动作：屏幕朝上静止 1 秒，熄屏后绕长边翻转约 90° 朝向自己，再保持 0.5 秒。轴的符号和映射由实测决定，不猜最终左右手适配。当前实板照片姿态为屏幕正面朝向观察者、竖直放置、短边在下；结合 C1 三组静止采样，记录为负 X（-X）自然翻腕方向。
- C2 已开放现有“设置→显示→抬腕亮屏”，默认开启并保存，与勿扰独立；不新增定时开启或灵敏度档位。软件判定在 arm 时读取一次屏幕朝上基线，候选竖直姿态需持续至少 500 ms 才触发。
- 息屏时按既有页面/草稿/前台秒表恢复规则亮屏，变暗时提亮并沿用所选熄屏时长；正常亮屏动作不续时，放下手腕不立即关屏。持续亮屏的固定到期规则不变。
- 手持门槛为固定翻腕 10/10 次触发，另做 10 次轻微搬动、敲桌或拿起放下，最多 1 次误触发；记录每次动作，不把这组样本称作佩戴识别率。基础计步另开增量。

## 硬件与寄存器依据

官方 V1.2 [设计包](https://downloads.sifli.com/hardware/files/documentation/ProPrj_%E7%AB%8B%E5%88%9B%C2%B7%E9%BB%84%E5%B1%B1%E6%B4%BESF32LB52%E5%BC%80%E5%8F%91%E6%9D%BFV1.2%28202504250924%29.epro) SHA-256 为 `61065a6b55fedf6b9a67f69013560476748bb5ac7dd4841563924a358f760a96`。本机副本和抽取文件位于本工作树忽略目录 `artifacts/imu-research/`。

- `SHEET/bfbe9dd79a064614aebc68082ceec943/33.esch` 的 U13 为 LSM6DS3TR-C；元件位于 `(550,660)`，符号 `b4e8e8c1825f4b93a58ee7284c75b6bd.esym` 的 INT1/4 脚偏移 `(-55,0)`；原理图 395–397 行从 `(495,660)` 接 `PA_31`。PCB `e4947da8b8a84953a988023242fbea49.epcb:4230/4235` 同时给出 U13、`PAD_NET e21 4 PA_31`。INT2 标为未连接。此为设计证据，销售实板一致性仍待验证。
- PA39/PA40 分别为 SDA/SCL，固定 SDK 示例复用为 I2C3；原理图网络名里的 I2C1 不限制 MCU 的复用选择。V1.2 的 SA0 接地，对应七位地址 0x6A；诊断也只读探测另一地址 0x6B。
- PA30 控制共享 `GS_3V3`，其上游为 VSYS_1；显示关闭保留 PA38 的代码注释明确提及传感器/翻腕。SDK deep 恢复会拉高 PA30/38。GPIO 读回和寄存器保持只能证明软件路径，不能代替实测供电电压、电流和硬件驻留。
- 固定 SDK 的 SF32LB52 HPSYS AON 支持 PA24–PA44，PA31 可查询映射；使用官方 GPIO 和 `pm_enable_pin_wakeup()`，不改 SDK。
- 寄存器依据为 [ST 的 LSM6DS3TR-C 驱动](https://github.com/STMicroelectronics/lsm6ds3tr-c-pid)：WHO_AM_I=0x6A、CTRL1_XL、CTRL2_G、CTRL3_C、CTRL6_C、CTRL10_C、TAP_CFG、MD1_CFG、FUNC_SRC1。0x6A 被部分 LSM6 器件共享，读到它不等于独立证明型号；需结合实板版本与后续行为。

SDK 现有 LSM6DSL 封装初始化不比较 ID 数值，写寄存器路径忽略传输结果，中断模式读取返回 0；本增量使用项目自有的小型寄存器适配，核对每次传输和配置读回。26 Hz、±2 g 加速度，陀螺仪关闭，复位轮询最多 40 ms，I2C 驱动 timeout=50（沿 SDK 单位），100 kHz。倾斜事件只作 C1 中断候选，不等同于已识别用户看表意图。

## C1 诊断入口

启动只建立诊断线程，等待串口命令；不自动开启采样或亮屏。所有寄存器访问由同一线程串行执行，GPIO 回调只计数和投递事件。I2C 操作时持有 IDLE 请求；等待倾斜中断时释放，保持既有 UI/BLE PM gate。

| 命令 | 用途与退出 |
|---|---|
| `wf_imu probe` | 只读检查 0x6A/0x6B，必须恰好一个 ID 匹配才复位、配置和读回；失败不会断言重启产品 |
| `wf_imu sample 20` | 最多 20 个新样本，打印 raw XYZ 和 mg；数量限 1–100，采样窗口有总超时，结束关闭 ODR |
| `wf_imu arm 60` | 倾斜路由 INT1→PA31，最多 60 秒（参数 1–120），仅记录中断源、轴值、GPIO 和 PM 路径；不点亮屏幕；最多处理 32 次 IRQ |
| `wf_imu status` | 配置、错误/中断计数和寄存器读回；不消耗 FUNC_SRC1 中断源 |
| `wf_imu stop` | 中断等待可中止，禁用 GPIO/AON，尝试关闭路由、嵌入功能、加速度和陀螺仪并读回；失败明确记录 |

不关闭共享 GS_3V3，不自动重试总线或电源循环。GPIO/AON 关闭失败会单独报告 `wake disable FAILED` 并清除配置就绪状态，重新 probe 仍关闭失败时拒绝配置；寄存器的 `stop=ok` 仅表示四组寄存器读回成功。退出失败不宣称已释放传感器负载或唤醒资源。原始日志保留本机，提交证据只保留测试消息和必要状态。

## C2 软件实现与主机验证（2026-09-29）

设置记录升级为 v5，沿用 24 字节布局，byte 16 bit 0 为勿扰、bit 1 为抬腕亮屏；v4 解码迁移默认开启。Product 息屏路径在开关开启时 arm PA31/AON，不使用息屏轮询；IRQ 回调只投递线程事件，线程保存屏幕朝上基线，要求先离开该基线，再仅接受短边朝下、屏幕朝向观察者对应的负 X（-X）姿态，保持 500 ms 后发送 `WF_EVENT_IMU_WAKE`，主线程再按既有暗屏恢复流程亮屏。关闭开关时不 arm，勿扰状态不改变抬腕开关。

主机命令 `cmake --build artifacts/host-c2 --parallel 6` 与 `ctest --test-dir artifacts/host-c2 --output-on-failure --timeout 60` 均退出 0，16/16 通过，包含 v5 编解码、设置默认/关闭/重建持久化和 UI 开关状态。`git diff --check` 通过。最终 Product SCons 构建记录为 `artifacts/product/20260929-152725-578/result.json`，退出 0、产物校验通过；主 BIN 7,433,924 B，SHA-256 `6e0bf67e13ab24c0a6d9bd09a384538ca8afc733958901c1ace927a6225a69f6`；bootloader 58,496 B，SHA-256 `e59d70db44367d524fbb46a8a014ce0092462e86959d101a8e2e00ad27bb5e21`；FTAB 11,288 B，SHA-256 `2b660ddaec088e160e4cecd8c27b541992c098bf7a108d8ff0ad16752074c928`。该记录的 `hardware_verified` 仍为 false。以上是源码与主机/构建证据，不是硬件验收。

硬件写入：使用绝对路径 `D:/MY_Desk/project/wristflow/.tools/sifli/tools/sftool/0.2.5/sftool.exe`，在 `apps/product/project` 对 COM5 执行 `write_flash --verify`，地址为 bootloader `0x12010000`、main `0x12020000`、ftab `0x12000000`，未整片擦除、未写 settings，最终退出码 0。第一次记录命令因相对日志目录不存在而未形成有效烧录证据；第二次工具进程报 `NoDevice / 拒绝访问`，结束本次残留的 sftool 进程后重试成功。最终日志为 `artifacts/hardware/imu-c2-20260929-negative-x-flash-final.log`。

启动采集 `artifacts/hardware/imu-c2-20260929/startup-final/uart.log` 使用 1,000,000 8N1、无流控和 150ms RTS 复位，30 秒窗口正常关闭。日志确认 `storage=ready restore=yes`、CO5300/FT6146 初始化、Product 启动及自动息屏；息屏后出现 `[imu-c2] wrist wake armed baseline=-16278,-307,-3013 flat=0 direction=negative-X`。这只证明新版固件启动并进入 arm 路径，不等于动作触发验收。

2026-09-30 已在同一块仅 USB 供电、无电池和新增接线的黄山派上完成上述硬件复测。烧录和采集条件、用户观察与串口原始日志见下节；物料已签收但实物规格和极性尚未核对，仍不接入本轮固件。

## 2026-09-30 C2 实板验收

- **烧录与采集**：COM5 枚举为 CH340。用 sftool 0.2.5 执行三镜像 `write_flash --verify`（bootloader `0x12010000`、main `0x12020000`、ftab `0x12000000`，包含 soft reset，不整片擦除、不写 settings）退出 0。采集脚本以 1,000,000 8N1、无流控、RTS 150 ms 复位、只收日志方式运行；目录为 `artifacts/hardware/imu-c2-20260930/run-20260930-135128-466243/`，14:10:58 用 stop marker 正常结束。原始 `uart.log` SHA-256 为 `1e73611474267668e57b1866c3045a2f019b18463f5b8114a2b63a10c93c9df1`，`serial.bin` 为 `b1169b5a4f548a0366195e5c5c31da18a6273d4b62959f8b357a7ca203c66d98`。
- **固定翻腕门槛**：用户执行屏幕朝上、短边朝下且屏幕朝向自己（负 X）的 10 次动作，主观观察 10/10 亮屏；日志对应 10 次 `wake event=IMU`、`screen_off=10` 和 `screen_on=10`。这满足手持门槛，不称作佩戴识别率。
- **干扰与开关**：4 次轻搬、3 次敲桌、3 次拿起放下共 10 次，用户观察误亮屏 0/10。关闭“设置→显示→抬腕亮屏”后重复负 X 动作没有亮屏，后续日志无新的 `wrist wake armed` 或 `wake event=IMU`；勿扰开关保持原值。关闭开关的后续亮屏来自 KEY1。
- **变暗和正常亮屏边界**：变暗态负 X 动作出现 `brighten event=IMU`，亮度从 9 恢复到 47；用户确认恢复成功但主观感觉比 Redmi Watch 4 慢、灵敏度和“聪明感”较差。正常亮屏动作没有延长亮屏，放下后按原变暗/息屏时序运行，无立即熄屏或其他异常。
- **结论边界**：按本专题的 10/10 与最多 1 次误触发门槛，C2 手持触发功能通过；主观延迟/灵敏度问题作为后续体验优化记录，当前没有新增灵敏度档位或改阈值。该结论不覆盖佩戴识别率、电流、续航、PPG、振动、电池和长时间稳定性。

## 验证记录与下一步

源码：已审阅地址探测、ST 寄存器位、SDK I2C 返回语义、GPIO/AON 与 PM 请求生命周期。传输失败/错误 ID/复位超时/清理/有符号轴解码用例通过。接续审查补齐了 GPIO/AON 关闭返回值报告；本增量不接产品亮屏事件。C1 未改 UI、字体、SDK、公共构建入口或分区。

编译：最终修订版于 2026-09-28 16:17 完成 Product SCons 构建，退出 0、产物校验通过，290 个工程源哈希与当前文件逐项一致。主 BIN 为 7,431,824 B，SHA-256 `b4656248ec75474f6d4af098a85212a075187619e3140dcab02fb0ea1081c0b9`。主机 GCC 15.2.0／Debug／官方 Win32 驱动构建及 16/16 CTest 通过；322 个主机相关源文件的测试前后哈希一致。三镜像、版本、源码快照摘要与命令见 [精简证据](evidence/2026-09-28/imu-c1-local.json)。此前 15:56 的成功构建保留为旧版本证据，不用于本次上板。

本工作树重现命令：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product
cmake -S tests -B artifacts/host-imu-c1 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe -DCMAKE_MAKE_PROGRAM=D:/msys64/ucrt64/bin/ninja.exe -DWRISTFLOW_BUILD_SIMULATOR=ON
cmake --build artifacts/host-imu-c1 --parallel 6
ctest --test-dir artifacts/host-imu-c1 --output-on-failure --timeout 60
```

隔离构建助手仅位于本机忽略目录，复用主目录已注册的 SDK 环境并核对两份锁文件；普通检出使用 `scripts/Build.ps1 -Example product -Jobs 6`。最终 Product 原始记录为 `artifacts/product/20260928-161628-226/result.json`，恢复后的主机日志、源码快照与增量构建日志在 `artifacts/imu-c1-resume-verification/`。保留 SDK `MSH_CMD_EXPORT` 函数指针转换警告（新 `wf_imu` 注册点同样出现），以及既有 RWX、新库 syscall／ftab entry 链接警告；未抑制或修改 SDK。旧对话异常未丢失构建产物，接续后完成哈希核对并验证最终修订版。

硬件：C1 诊断路径和本轮 C2 手持动作已有同一块板、同一 COM5 的实板证据；C2 达到 10/10 触发和 0/10 干扰误亮屏，关闭开关、变暗提亮、正常亮屏时序也有用户观察及日志对照。用户明确反馈触发与变暗恢复相对 Redmi Watch 4 偏慢，保留为体验问题。电流、精确驻留、佩戴识别率、续航和长测仍未覆盖；物料虽已签收，规格/极性核对及接入实验尚未开始。

## C1 实板步骤与通过条件

1. 确认同一黄山派仍仅 USB 供电、无新增接线，再枚举 CH340 端口；使用精简证据中的三镜像和 SDK `sftool write_flash --verify` 单独下载，不整片擦除。烧录包含复位，先获本次现场条件确认。
2. 启动后执行 `wf_imu probe`，只允许恰好一个地址匹配且 `prepare=ok`。另一地址无响应带来的 `io_errors` 增加需与后续传输错误区分；ID=0x6A 本身不能唯一证明器件型号。
3. 分别让屏幕朝上、长边竖起朝自己、短边竖起，各执行 `wf_imu sample 20`。保留 raw XYZ／mg、姿态与方向描述，确认静止重力轴和符号；采样结束必须 `stop=ok`，且没有 `wake disable FAILED`。失败先停在诊断层，不猜轴映射。
4. 亮屏时启用既有持续亮屏设置，执行 `wf_imu arm 30` 并按固定动作转动，记录 PA31、`src & 0x20`、irq/tilt 增量；命令只记录，不要求屏幕变化。随后关闭持续亮屏，重新 `arm 60`，静置等屏幕关闭后再动作，记录 PM `deep_elapsed`／wake、寄存器保持、触发和限时退出。若串口输入会干扰观察，预先布置命令后只收日志。
5. 执行 `wf_imu status` 确认 MD1／CTRL10／XL／G 为零，KEY1 能恢复 UI，既有 BLE 测试通知仍可到达。只有读数/方向、中断及息屏恢复均有证据后进入 C2；GPIO 高低和软件 PM 计数不替代电压、电流或硬件驻留测量。

## 2026-09-28 首轮实板故障记录（未通过）

用户确认仍仅 USB 供电、接线未变，授权烧录及诊断。`ffa2730` 对应上述三镜像经 COM5 `sftool 0.2.5 write_flash --verify` 写入，退出 0。单一串口所有者采集 1,000,000 8N1，启动诊断就绪；0x6A 地址读到 WHO=0x6A，0x6B 无响应，prepare 与配置读回成功，AON 映射为 7。`io_errors=1` 来自该备用地址探测，后续有回报的诊断期间未增加。

三组用户确认姿态均取得 20 个样本并 `stop=ok`：朝上平放重力为负 Z，短边在下为负 X，长边在下为负 Y。用户纠正第二组最初误称长边，记录以纠正后的姿态为准；一次在用户确认前发出的第三组采样弃用，之后重采。长边组含一次轻微动态偏离，不作为标定精度证据。

亮屏 120 秒中断窗口取得 11 次 src=0x20；放下动作也触发，不能直接当作抬腕意图。用户确认持续亮屏。息屏且 `allow=1, own_idle=0` 的 120 秒窗口取得 10 次 tilt 中断，`deep_elapsed` 从 1227 增至 2281（最后一次中断附近），用户确认三次动作时屏幕持续黑屏；到期 `stop=ok`。两轮翻起读数主轴分别为 Y/X，下一阶段需收敛手持动作定义，不能静默假定一致。

**阻断**：息屏窗口之后用户按 KEY1 无法亮屏，串口命令一度没有响应，未取得 BLE/通知恢复证据。因此 C1 仅部分路径有证据，整体未通过，C2 未实施。19:59:23 窗口结束后暂时无设备输出；完整日志后段显示 20:00:40 又执行 hold、20:00:42 响应 IMU/BLE/PM 查询（传感器停用寄存器为零、BLE 未连接），因此撤回“之后完全无设备输出”的早期记录，不能判定为整机死锁。host COMMAND 行仍不表示设备执行。受控 RTS 复位后重新正常启动，设置恢复，暂时 `wf_pm hold`；用户确认 KEY1/触摸恢复，故障原因尚待定位。

所有日志位于本工作树忽略目录 `artifacts/hardware/imu-c1-20260928/`：`flash.log`、`images.sha256`、`serial.bin`（原始本地数据）、`serial-key-safe.log`（续测筛选记录）、`serial-safe.log`（首轮记录）和 `recovery-reset.bin`。续测已完成 GPIO/AON/KEY1 对照；当前保留首轮未复现故障及其原始数据，不将当前计数当作电流或精确硬件驻留证明。

接续源码核对未见 PA31 disable 直接误清 PA34：二者分别在 GPIO 第 0/1 组，AON 关闭也只清 PIN7；KEY1 为 PIN10。日志 `wake=0x2/0x6` 包含 GPIO1/LPTIM1，并非每次 IRQ 的独立溯源，不能据此断言 AON 丢失。当前新增 `wf_key` 只读诊断，记录 PA34、按键四类回调计数、产品 KEY 发送/接收计数、WER/WSR 和 GPIO IER1/ISR1；回调中只计数，日志在主线程或命令线程输出，未替换 SDK 的按键 IRQ。IMU 实验结束也请求一次状态快照。该诊断版另行构建/上板，不用 `ffa2730` 的测试或固件哈希覆盖。

诊断版 Product 于 20:35 构建和 290 个源哈希校验通过，主 BIN 7,432,436 B／SHA-256 `7aeeec580c637eb86c23b4bab114be3e823402857178de9ea53b85e587fae60c`；三镜像再次写入/verify 退出 0。首次编译因 SDK HAL 使用 `GPIO_TypeDef` 的 32 引脚分组、没有 `IER1/ISR1` 成员失败，已按 SDK `GPIO_GetInstance` 的同等分组寻址修正，失败日志保留。详见 [诊断版精简证据](evidence/2026-09-28/imu-c1-key-diagnostic.json)。主机覆盖的核心和测试没有改变，未重复用主机测试冒充按钮/GPIO 验证。

20:46 的未启用 IMU／PM hold 基线：用户确认 KEY1 亮屏正常；日志对应 press=1、sent=1、received=1、随后 screen on。多次按键后 press/release/click=5，sent/received=5；未见单边计数增长。20:52 在屏幕已灭但 PM hold 的状态下 arm 10 秒再自动 stop，没有 IRQ，WER=`0x100400c6`、IER1=`0x1004` 与此前息屏值相同，KEY1 PIN10 与 GPIO PA34 位均仍使能。下一步等待用户按 KEY1 验证该启停后的恢复，然后在传感器关闭时单独 allow PM 对照；不能由寄存器位未变直接认定按键恢复成功。

诊断版日志：`artifacts/hardware/imu-c1-20260928/serial-key-safe.log`，原始数据/命令位于其 `key-diagnostic/` 子目录；下载日志为 `artifacts/hardware/imu-c1-keydiag-20260928/flash.log`。续测时完成了 PM hold、PM allow、IMU arm 自动停止和息屏 arm 对照；息屏窗口收到 22 次 `src=0x20` tilt IRQ（`allow=1, off=1, own_idle=0`），未主动亮屏。arm 期间两次 KEY1 均出现 `sent=received` 并完成 `screen on`，`stop=ok` 后再次 KEY1 仍成功。当前 IMU 已停止；此前 KEY1 异常未复现，根因未知。C1 的 IMU/PM/KEY1 核心路径已验证，BLE/Gadgetbridge 通知恢复尚未补测，C2 未实现。

### 2026-09-28 续测结论

最终组合窗口从 `21:17:46` 的 KEY1 唤醒开始，立即执行 `wf_imu arm 60`。`21:17:56` 熄屏并允许 PM 后，`21:17:58`–`21:18:10` 收到 12 次 `src=0x20`，`21:18:13` KEY1 恢复亮屏；再次进入窗口后又收到 10 次 tilt，`21:18:39` KEY1 恢复亮屏。`21:18:48` 自动停止报告 `stop=ok, irq_delta=22`；`21:19:50` IMU 已停止时再次 KEY1 成功亮屏，计数保持 `sent=received`。该记录证明传感器中断不会主动点亮屏幕，且 KEY1 能在 IMU 活动和自动停止后恢复；它没有解释首轮异常，也没有替代电压、电流或 BLE 长测证据。

用户补充的通知现象：一次消息似乎没有立即在黄山派亮屏或留下记录；打开 Redmi Watch 4 的历史消息后，链路恢复，再次发送同类测试消息时两端同时出现，之后持续正常。本轮复测已成功收到无隐私测试通知。该现象仍记为通知到达/显示时序风险，恢复机制未解释，不能直接归因于 IMU 或 PM。
