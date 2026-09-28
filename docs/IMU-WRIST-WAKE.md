# IMU 与翻腕亮屏

2026-09-28，范围经 Q1–Q9 确认。当前为 C1 诊断增量，C2 翻腕显示行为须在实板路径通过后实施。分支 `codex/imu-wrist-wake` 从 `main 5c0b82c` 建立，工作树为 `C:/Users/13984/.codex/worktrees/imu-wrist-wake/wristflow`。

## 前置核对

主线源码与 PR #28 通过的完整云端运行 [36323967702](https://github.com/e-n-ye/wristflow/actions/runs/36323967702) 文件树一致：15/15 主机测试、5/5 固件构建。PR #29 仅补充该版本的 Product 复编译与 COM5 写入/verify 记录，没有固件源码改动，仍开放；见 [复验证据](https://github.com/e-n-ye/wristflow/pull/29)。QQ/微信通知曾短暂收不到后恢复，根因未知，不记作已修复。未发现已有证据确认、会阻断本增量的源码缺陷。

既有长测、五次重连联合矩阵、电流和续航仍后置；历史分项硬件结果不覆盖本增量。当前主目录的旧 bringup 分支及未提交改动保留。

## 已确认的行为与验收

- 两步交付：C1 先验证 ID、地址、轴方向、采样、供电状态和中断/休眠恢复；通过后 C2 接翻腕亮屏。没有可用中断唤醒路径时保留诊断、暂缓亮屏，不用息屏持续轮询替代。
- 本轮手持演示，尚未固定佩戴。动作：屏幕朝上静止 1 秒，熄屏后绕长边翻转约 90° 朝向自己，再保持 0.5 秒。轴的符号和映射由实测决定，不猜最终左右手适配。
- C2 开放现有“设置→显示→抬腕亮屏”，默认开启并保存，与勿扰独立；不新增定时开启或灵敏度档位。
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

硬件：本轮尚未烧录、复位或操作串口。下一有界实验为核对 COM5/USB 供电与板上附件，写入经校验的 C1 三镜像，先读取 ID 和三个静止朝向；再在亮屏和息屏状态各做限时 tilt 中断实验，记录 PM 计数及 KEY1/BLE 恢复。通过后再实施 C2，不能用编译代替此条件。

## C1 实板步骤与通过条件

1. 确认同一黄山派仍仅 USB 供电、无新增接线，再枚举 CH340 端口；使用精简证据中的三镜像和 SDK `sftool write_flash --verify` 单独下载，不整片擦除。烧录包含复位，先获本次现场条件确认。
2. 启动后执行 `wf_imu probe`，只允许恰好一个地址匹配且 `prepare=ok`。另一地址无响应带来的 `io_errors` 增加需与后续传输错误区分；ID=0x6A 本身不能唯一证明器件型号。
3. 分别让屏幕朝上、长边竖起朝自己、短边竖起，各执行 `wf_imu sample 20`。保留 raw XYZ／mg、姿态与方向描述，确认静止重力轴和符号；采样结束必须 `stop=ok`，且没有 `wake disable FAILED`。失败先停在诊断层，不猜轴映射。
4. 亮屏时启用既有持续亮屏设置，执行 `wf_imu arm 30` 并按固定动作转动，记录 PA31、`src & 0x20`、irq/tilt 增量；命令只记录，不要求屏幕变化。随后关闭持续亮屏，重新 `arm 60`，静置等屏幕关闭后再动作，记录 PM `deep_elapsed`／wake、寄存器保持、触发和限时退出。若串口输入会干扰观察，预先布置命令后只收日志。
5. 执行 `wf_imu status` 确认 MD1／CTRL10／XL／G 为零，KEY1 能恢复 UI，既有 BLE 测试通知仍可到达。只有读数/方向、中断及息屏恢复均有证据后进入 C2；GPIO 高低和软件 PM 计数不替代电压、电流或硬件驻留测量。
