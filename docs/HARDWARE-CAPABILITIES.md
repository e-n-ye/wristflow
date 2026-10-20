# 硬件能力选择与验证

依据[2026-10-19用户决定](BASELINE.md#hardware-first-20261019)。本页保存选择门槛、版本化核查与实验契约；活动项及唯一下一步只在[STATUS](STATUS.md#current)，所有设备操作仍须核对[HARDWARE](HARDWARE.md)和本轮授权。

## 必须经过的选择门槛

1. **先列能力**：核对实际器件、板级连接、数据手册／应用笔记和锁定SDK。区分芯片内置检测、SDK软件封装及产品策略；名称相近或WHO_AM_I相同不足以证明不同型号的扩展寄存器完全兼容。
2. **先证明配置正确**：核对总线、寄存器页、启用顺序、采样参数、轴向、输出／中断及停止路径；检查返回值和必要的写后读回。官方或SDK存在API，不等于该API实际正确写入。封装缺陷应先在项目适配边界修正并验证，不改动锁定厂商源码。
3. **先测内置效果**：以实际设备记录配置、源寄存器／计数、事件与时间，按同一安装方向、动作／干扰样本和供电条件比较效果。先约定可接受标准，短样本确认链路后再扩大样本；主机模拟寄存器、编译或肉眼看到一次亮屏均不能代替完整效果比较。
4. **有证据才替代**：仅在正确配置和合理参数试验后，设备对比仍表明该内置能力不足，才提出自研替代算法；记录不足、配置／版本、样本、指标和软件替代范围。尚未接入、驱动缺陷、赶进度或没有设备测试，不得记成“硬件效果不行”。硬件明确没有目标能力时，记录器件资料依据，不把不存在的能力列为已失败。
5. **保留职责边界**：保存、跨天归档、导航、提醒仲裁及用户开关等产品策略可以用软件实现；不能因此重写已有硬件检测。算法选择、配置与数据处理分别说明，必要的换算／滤波也不自动等于替代硬件算法。

硬件相关PR须提供器件能力、选择结果和相应证据；自研替代额外提供上述比较。未取到设备结果时写“待验证”，不能宣布片上算法有效或无效。

## 2026-10-19 有界核查

接入前核查基线为主线`ba6a93c`、C2草稿`9815dca`和`sdk.lock.json`固定SDK`421126d9f476ed8e2a6f0b0ca28a9f241c182e65`。下表只记录该基线的源码／资料检查，不是全仓硬件能力审计；随后实现的计步诊断及其软件验证单列于后文，仍没有设备效果结果。

| 对象 | 已查到的能力／实现 | 本轮结论 |
|---|---|---|
| LSM6DS3TR-C计步 | 芯片内置step detector／step counter；ST AN5130说明加速度计ODR须不低于26Hz | 尚未在当前Product接入或验证；优先验证硬件计步，不先写步数识别算法 |
| LSM6DS3TR-C腕部检测 | 芯片内置absolute wrist tilt；普通tilt是另一能力 | C2草稿用了普通tilt作候选，再做软件姿态／保持判定；未找到先行比较absolute wrist tilt不足的证据，不能据旧草稿跳过内置能力 |
| LTR-303／MMC5603NJ | 官方板载环境光／地磁器件 | 优先利用现有器件；本轮没有审查全部检测能力、驱动或效果，不预判自动亮度／指南针已可用 |
| 电量 | 当前读取ADC，使用SDK参考曲线的软件估算；未使用SDK软件计算器 | 本范围没有证明板上存在被绕过的硬件SOC计量；SDK软件计算器不等于片上库仑计，不把自实现估算直接定性为重复硬件 |
| 充电 | Product配置AW32001寄存器并读取芯片状态，由芯片执行充电调节 | 未见软件替代其硬件调节的证据；会话、停止与显示是产品控制层，实际电气结果另验 |
| RTC／触摸 | 当前使用RTC设备接口；触摸首次唤醒／输入消费属于产品策略 | 本范围未发现已证实的硬件算法替代问题，也未完成触控芯片全部手势能力核查 |

板载资源来源：[立创官方硬件资料](https://wiki.lckfb.com/zh-hans/hspi-sf32lb52/hardware/board.html)。LSM6DS3TR-C能力及配置来源：[ST产品资料](https://www.st.com/en/mems-and-sensors/lsm6ds3tr-c.html)与[ST AN5130](https://www.st.com/content/ccc/resource/technical/document/application_note/group0/90/41/09/82/24/d9/4f/29/DM00472670/files/DM00472670.pdf/jcr%3Acontent/translations/en.DM00472670.pdf)。

### IMU接入必须处理的已知风险

- C2草稿`apps/product/src/imu_device.c:56–65`写`CTRL10=0x0c`启用普通tilt；`product_imu.c:316–380`使用1.5秒扫描／500ms软件保持判定，不能称为absolute wrist tilt验证。草稿只保留为板级适配及历史证据来源，不整支合入。
- 锁定SDK的`customer/peripherals/sensor/LSM6DSL/libraries/lsm6dsl_reg.c:5290–5305`中，`lsm6dsl_tilt_src_set()`调用read而非write，不能据函数名认为腕轴mask已配置；`:5190–5203`等路径仅成功时回user bank，事务失败可能遗留bank B。需针对实际LSM6DS3TR-C核对并在项目边界处理，不能用这类驱动缺陷判定硬件识别效果差。
- ST专用驱动也须逐项核验：2026-10-19核对的[官方提交`6735cd1`](https://github.com/STMicroelectronics/lsm6ds3tr-c-pid/blob/6735cd1f6ea9208084f2e7dc69605250bc532ac6/lsm6ds3tr-c_reg.c#L6683)的`lsm6ds3tr_c_tilt_src_set()`仍调用read。该提交是本次外部核查锚点，没有下载进厂商树或替换SDK锁。
- 草稿`WF_EVENT_IMU_WAKE=16`与当前Product的倒计时事件16冲突；旧v5设置与当前v7也不同。当前暗屏唤醒已协调提醒／充电／通知，不能复制旧main、PM头文件或设置编码来覆盖现有行为。

## IMU第一轮硬件验证入口契约

第一轮以单项、有明确期限的原生能力诊断为目标，先准备计步实验，再独立准备absolute wrist tilt；不是一次实现所有板载传感器。计步配置按AN5130 §6.1、腕部检测配置按§6.4核对并分别实现于后文；不能直接套用旧C2。

- 启动默认无采样；显式probe后才接受单项实验。单一拥有者管理I2C、寄存器bank与实验期限，先记录型号／地址／配置读回；共享传感器电源不得用于粗暴复位。
- 开始前约定有限时长和样本；到期／stop／故障都走关闭、源清理和读回。关闭失败保留故障及资源责任，不误报停止成功或立即接受下一实验。
- 计步记录硬件累计计数和实际步数参照，分开判断增量、计数复位、误计与漏计；腕部实验记录源寄存器／事件、安装轴向和动作／干扰反馈。硬件普通tilt与absolute wrist tilt结果分别标注。
- 初轮允许在限定实验窗口读取硬件结果以隔离算法验证；不默认常驻、不接UI、不继承旧软件pose。GPIO／AON息屏唤醒、全天累计／跨天／持久化及效果对比各自按后续有界闭环验证。
- 配置和失败清理可先主机验证并编译；这些结果不能抵扣板上效果。设备操作前重新确认板屏、供电、电机断线情况和当轮镜像／实验范围；本契约不自动授权烧录、动作采集或充放电。

通过内置能力实验后，再据实接入Product服务、PM、用户开关和已有组件；没有硬件效果不足的比较记录，不启动自研替代算法。

<a id="imu-steps-diagnostic"></a>
## 原生计步诊断（2026-10-19软件候选）

`apps/product/src/imu_pedometer.c/h`只配置／复位／读取芯片16位step counter，没有软件步数识别或累计算法。`product_imu_diag.c/h`拥有单一事件worker、I2C3及实验期间的IDLE申请；main只创建休眠worker，启动不访问传感器。当前Product没有其他IMU拥有者，后续接入环境光／地磁时须统一共享总线所有权，不能并行改mux／配置。

板级适配参考C2的PA39 SDA／PA40 SCL、PA30共享传感器电源、100kHz／50ms总线参数及10ms准备等待；不接INT1／AON。显式probe只接受0x6a／0x6b中唯一WHO_AM_I=0x6a的设备；该ID结合官方板载BOM确定预期型号，不能仅凭ID识别任意替换芯片。PA30只使能、不在退出时断电，以免关闭LTR／MMC的共享电源；这不构成功耗优化结论。

| 配置／读回 | 本轮选择及依据 |
|---|---|
| CTRL1_XL 0x10 | 0x20：26Hz、±2g，满足AN5130计步最低ODR |
| CTRL3_C 0x12 | 0x44：BDU／地址自增；BDU不能作为step counter原子读的依据 |
| bank A 0x0f／0x14 | 0x10／0x6e：AN5130参考阈值及去抖；bank选择0x80，退出始终尝试恢复user bank |
| CTRL10_C 0x19 | 0x14启用FUNC_EN／PEDO_EN，0x16→0x14明确置位再清除非自清零PEDO_RST_STEP |
| STEP_COUNTER 0x4b／0x4c | H-L-H最多3次，跨字节变化重读；只防撕裂，不识别动作；每次开始确认计数归零 |
| 停止／路由 | INT1_CTRL／INT2_CTRL／MD1_CFG／MD2_CFG均0；停止写CTRL10／XL／GYRO为0并读回，确认user bank后逐项尝试清理 |

以下是诊断接口说明，不构成设备操作授权：

```text
wf_imu probe             # 显式准备总线并识别设备，不启动计步
wf_imu steps 30          # 1..120秒，必须先probe成功；每次复位原生计数器
wf_imu status            # 只打印缓存，不做I2C；运行时约每秒刷新缓存
wf_imu stop              # 优先取消待办／活动实验；故障时显式重试清理
```

实验期限从首个加速度ODR写入前开始，准备阶段不计入；配置步骤间检查实时期限／取消，到期或故障即清理。不能中断已经开始的I2C事务，实际停止延迟尚未测量，不能称为精确物理断流保证。重复启动不续时；非活动时worker无限等待，无后台采样。I/O失败、读回不符或计数不一致均保留FAULT；即使随后清理成功也不掩盖失败。停止未确认时保留IDLE及责任，显式stop确认后才能恢复；不能用电源轨复位掩盖故障。

状态值phase为0未识别、1就绪、2运行、3到期完成、4故障；end为0未结束、1用户停止、2到期、3I/O／验证失败。`counter_valid`和`stop_verified`分别表达计数及停止有效性，旧数值不可脱离有效位使用。默认去抖6意味着通常需连续第7步后才开始报告，孤立几步不增不能直接判为故障；本轮未调参，也没有全天／跨天／持久化、UI或抬腕亮屏。

该软件候选的主机验证为Debug完整49/49、Release相关2/2；同时测试纯寄存器层与真实worker C文件（RT接口替身），覆盖bank恢复、计数跨字节、期限回绕、取消、读写及读回失败、FAULT清理和PM责任。审查发现的准备期取消、过期启用与失败原因被取消覆盖问题均已修正并有回归。固件构建、源码／产物标识和剩余边界见[精简证据](evidence/2026-10-19/imu-native-pedometer.json)。软件准备阶段未操作设备；随后首次上板见下节，不宣称内置算法已适合腕戴场景。

设备实验前须重新确认当前供电和精确候选，先记录静止基线、约定连续实际步数及非步行手部干扰样本，再对照原生计数／配置／时长。效果标准及扩大样本在该实验前对齐；未经实测，不替换成软件阈值算法，也不将本候选直接接成日常步数。

## 首次上板发现的I2C3配置遗漏（2026-10-19）

用户授权上板并确认仅USB／无电池／松线固定。原候选下载和启动成功，但probe返回`command=1 accepted=0`且没有地址日志；实际`.config`明确`BSP_USING_I2C3`未启用。板默认只开I2C1／2／4，`rt_i2c_bus_device_find("i2c3")`因此无设备，尚未访问芯片，不能把该失败归因于原生计步算法。首个会话已正常stop／allow／关闭。

修复在Product `proj.conf`显式启用I2C3；诊断加入缺配置时的编译阻断、总线未注册及配置失败的独立日志。保持官方传感器例程PA39／PA40适配、100kHz／50参数和原生计步配置不变；不启用SDK传感器轮询服务或DMA。主机新增两种准备失败的无采样／资源归还回归及缺I2C3编译拒绝检查；Debug49/49、Release相关2/2及预期编译拒绝通过。Product`20261019-170220-315`退出0、433源码／29产物重哈希一致，BIN`cb5e73c8…`三镜像下载verify通过；没有UI或SDK修改。

<a id="imu-first-device-20261019"></a>
## 首轮设备观察与串口恢复（2026-10-19）

修正版仅0x6a地址返回WHO_AM_I=0x6a，0x6b不匹配；15秒用户确认静置的计数为0，phase=DONE／end=DEADLINE、计数有效、停止读回确认，I/O错误和不一致读均0。这验证当前板的有界接入及一次静置样本，不等于完整计步精度通过。

用户改用原电池自然走动，首组回接USB时，助手虽然没有显式切换RTS，却取得SFBL／完整冷启动、RTC回2000年及未识别快照，实验缓存丢失，该组无效。此前两次BAT＋USB串口无回执也保留为失败；没有回执的hold／stop／allow不能记成设备已执行。无电池低VBAT拒充曾与正常UART／计步并存，不能直接把该提示判为失联原因。

用户提供的[官方说明](https://wiki.sifli.com/self-check/SF32LB52x-HW-Self-Check.html)明确列出黄山派串口RTS引起开口复位／关机；[pySerial说明](https://pyserial.readthedocs.io/en/latest/pyserial_api.html#serial.Serial.open)也说明打开时可能出现驱动瞬态。用户在COM5高级属性启用“禁用Modem流控”，只读核对`DisableModemOutHandShake=1`。随后稳定供电开口和一次电池→USB回读均无启动标识，RTC／实验started_ms／缓存连续；该对照支持此项缓解有效，不等于示波器证实所有重启原因。操作方法见[FLASHING](FLASHING.md)。

第二组120秒原生实验从0开始，拔USB后用原电池行走，再回接读取RUNNING；最终到期计数70，计数／停止有效，I/O错误及不一致读均0。用户随后澄清“30”按左右脚一组计一次，约为60个单脚步，又说明出门／返回并非严格计数，完整窗口还含拿取和拔插操作。**该组只作探索参考，不能把70对30、或70对精确60的算术差值写成误差率／精度判定。**一般单脚一次落地记1步，左右各一次记2步；后续先统一口径和完整窗口参照，再固定安装方向、分离操作干扰及试验原生参数。没有本轮效果不足的合格比较，不启动自研替代。

18:46:06助手正常关闭，最终计数70／停止读回确认、PM allow=1／off=1／own_idle=0／sdk_idle=1；剩余全局IDLE责任本轮未定位，不宣称物理深睡或低功耗。末次用户确认供电为原电池＋USB，电机松线固定；没有手动充电／电机命令，已存在的自动充电状态不作为新充电验收。完整命令、候选、失败、原始日志哈希及用户口径更正见[设备精简证据](evidence/2026-10-19/imu-native-pedometer-hardware.json)；活动项和唯一下一步只回[STATUS](STATUS.md#current)。

## 运行期今日步数接入（2026-10-19软件轮）

在上述原生链路及活力页验证之后，用户要求计步接口并确认先做运行期今日总量，重启保存后续；[契约](ACTIVITY.md#native-daily-steps)。仍采用同一LSM6DS3TR-C内置step counter及原阈值／去抖，没有软件识别或硬件不足判定。寄存器层新增显式continuous入口；正常poll只检查配置／读计数，不重复begin。既有worker统一拥有日常／诊断两种互斥模式，main改为启动日常生产者；旧“默认不采样／不接UI”的描述只适用于前轮诊断版本。

低功耗路径只完成源码依据核对：锁定SDK`drv_i2c.c`每笔传输申请／释放IDLE，event timeout注册thread timer，PM按下一timer设tickless唤醒并补偿tick；Product普通DEEP的PowerUpCustom直接返回，基础板PowerDownCustom为空，没有源码证据该路径主动关闭PA30。服务每次读取前恢复PA39／40 mux并配置I2C3，完整序列短持IDLE，正常等待释放，停止未确认保留FAULT责任；亮屏1秒／息屏60秒是新读取负载，不等于已测低功耗。

本轮没有接IMU中断或新的AON事件。只读核查的官方V1.2 EPRO显示INT1接PA31、INT2 NC；精确器件驱动含step／overflow及INT2转INT1能力，PA31具备52X AON源码条件，但实装引脚、清源和睡醒未验，不以此冒充中断接入证据。详细出处见[软件证据](evidence/2026-10-19/native-daily-steps.json)。原生计数跨DEEP保持、I2C恢复、精度与新增耗电仍须本版有界上板验证；不回填成算法效果已经通过。

随后同版Product`4cd7c09b…`三镜像verify／启动／RTC通过，日常phase2／valid1／两次total0／错误0。关闭串口后，用户报告15秒静置0→0，两组各人工50单脚步为0→57、息屏后57→113；证明链路有增量，不是原生算法不足的对比证据。额外动作、完整窗口控制及佩戴适配尚缺，保留现有硬件算法／参数；没有软件替代、小时桶或中断接入。物理DEEP、60秒定时、精度及耗电仍未证明，[硬件证据](evidence/2026-10-19/native-daily-steps-hardware.json)保留边界。

后续[检查点软件轮](ACTIVITY.md#steps-persistence)只为同一原生计数增加部分今日总量的Flash恢复，不持久化原始计数或推断重启间隙步数；原阈值／去抖不变。新Flash负载及worker栈预算单列，没有新增硬件算法验证、精度或功耗结论。

再后续同版`e7cca572…`有[自动40检查点／普通reboot及RTC未知后校时恢复](evidence/2026-10-19/steps-persistence-hardware.json)的设备证据；重启后原生0与恢复日总量40独立，未将旧原始计数接回。该证据不改变原生算法能力判断，也不证明物理掉电连续性、精度、DEEP或新增Flash负载的功耗。

2026-10-20[小时分布软件轮](ACTIVITY.md#steps-hourly)仅为同一原生计数增加时间归属和v2检查点，不重写动作识别、不改阈值／去抖、采样节奏或SDK锁。跨整点不猜柱，旧总量保留而小时未知；新增结构与Flash载荷单列在[软件证据](evidence/2026-10-20/hourly-steps.json)。后续精确`2a1a39aa…`有[设备迁移／小时保存证据](evidence/2026-10-20/hourly-steps-hardware.json)：保留旧56且不造旧柱，新27进18时桶，总量83与完整桶／mask普通重启恢复，原生计数从0重建。该结果支持产品时间归属／保存链路，不改变原生算法效果判断，也不证明精度、物理DEEP或功耗。

<a id="native-wrist-diagnostic"></a>
## 原生absolute wrist tilt限时诊断（2026-10-20软件轮）

从`main@c7dd3c5`实现LSM6DS3TR-C内置AWT，无软件姿态／动作识别。`imu_wrist_tilt.c/h`拥有腕部寄存器配置、读回、源快照及期限；仍由`product_imu_diag.c`的唯一IMU worker访问I2C3，与日常计步共存，不新增线程、IRQ或AON。本节覆盖上文旧单项诊断“启动默认无采样”的适用范围：Product日常计步已默认运行，腕检测仍默认关闭，仅接受显式限时命令。

按[AN5130 Rev1 §6.1、§6.4](https://www.st.com/resource/en/application_note/dm00472670-lsm6ds3trc-alwayson-3d-accelerometer-and-3d-gyroscope-stmicroelectronics.pdf)和[精确器件数据手册Rev3](https://www.st.com/resource/en/datasheet/lsm6ds3tr-c.pdf)核对：只借用已确认的26Hz／±2g原生计步配置；`CTRL10=04h→等50ms→00h`后写bank B的LAT／THS／Mask，每次写后读回并恢复user bank，再开启共享`FUNC_EN+PEDO_EN+WRIST_TILT_EN=94h`。不置`PEDO_RST_STEP`、不改ODR或切共享电源。官方说明停用计步不会清零已有计数；配置期间采集暂停、重启识别连续性与漏计量仍须设备观察，不能由mock保数推定。

腕阈值固定`20h`（500mg／30°），默认延迟15×40ms=600ms，可选1–255；每次只选一个半轴，六种命令轴映射至Mask的bit7–2，显式覆盖数据手册与应用笔记默认mask描述差异。bank B值在加速度计退出Power-Down时会复默认，故每次会话重配。锁定SDK的mask setter读代写缺陷未改厂商树，由项目边界直接写入并读回。

设置`TAP_CFG.LIR`及`DRDY_PULSE_CFG.INT2_WRIST_TILT`，即使INT2在官方板图为NC也保留真实中断路由：AN5130明确不路由时源位仅维持1/26秒，单设LIR不能锁存。worker限时窗口请求每100ms读取，先读`WRIST_TILT_IA(55h)`半轴再读`FUNC_SRC2(54h)`清锁存。两读非原子；`observations`是见到锁存源的次数，不是完整手势数或动作时刻。诊断全程持IDLE，尚无MCU唤醒或亮屏策略。

```text
wf_wrist start <1..120秒> <xpos|xneg|ypos|yneg|zpos|zneg> [1..255延迟单位]
wf_wrist status
wf_wrist stop
```

命令只入队／读缓存，不直接I2C。已有日常计步必须处于RUNNING；重复启动、未清理FAULT和取消期间拒绝启动。到期／stop／配置失败恢复`CTRL10=14h`保留计步、清源、恢复原PULSE／TAP。bank恢复未确认时不写user地址；未确认清理则保留FAULT／IDLE并使共享计步无效，显式stop清理后才允许retry重建计步。日常stop先清腕、再关闭共享设备。已保存日／小时检查点不被腕路径清零。

主机验证覆盖非零计数与小时保留、latched源读取、缓存status无I/O、重复命令、截止／tick回绕／准备期取消、bank B写失败及恢复失败、运行中丢配置、日常stop和两种腕准备总线失败后的责任保留／显式恢复。Debug完整53/53、Release相关5/5通过；源码审查、编译标识及证据见[本轮记录](evidence/2026-10-20/native-wrist-tilt.json)。本轮未打开串口、下载或采集动作；轴向／配置实机读回、片上检测效果、实际计步保持／漏计、清源／停止、PA31 IRQ／AON、亮屏开关及功耗均未验，不据此判定内置算法不足。

### 同BIN首轮设备观察（2026-10-20，部分通过）

随后按新授权下载精确`83c9bb50…`，恢复107／小时51，再自然短走形成native22／总129／小时73并自动保存。一次30秒xneg／600ms／30°会话`accepted1`表示配置序列及写后读回已完成，后续运行快照通过共享配置守卫；没有逐项原始寄存器打印。静止及用户四方向动作窗口均`FUNC_SRC2=84h／axes00／observations0`。UART捕获30023ms到期、DONE／stop_verified1；末端串口拒绝访问，用户听到设备重连声，原助手失败保留。独立cleanup会话无复位／补时，再读到同一DONE与native22／总129／完整小时／代次12保留；幂等stop与allow1回执后串口释放，用户确认屏幕／触摸／KEY1正常。[设备证据](evidence/2026-10-20/native-wrist-hardware.json)仅支持配置／到期清理／非零计数保留，不支持正向AWT效果或活动中取消。

**首轮未解观测异常**：精确数据手册Table179的bit7及bit2为0，而运行快照source84h将两位读为1；首轮bit0仍0，不能当腕事件或公开定义的NACK。每次poll已检查bank0／XL／CTRL10／LIR／路由，尚无确定源码原因。WHO6a也被LSM6DSL等共用，单改器件命名不能解释异常。首轮停止时SOURCE读到ignored，缓存source不会更新，因此当时停用后84h仅历史值。首轮缺少原始XYZ及三阶段新读寄存器的限制由下节软件及补测覆盖；不能据该次零事件判原生能力不足。

<a id="wrist-register-snapshot"></a>
### 三阶段寄存器与XYZ观测（2026-10-20软件增量）

本节覆盖上一版“没有新读寄存器／XYZ”的软件限制，不改变首轮设备结果。精确数据手册§2.2还明确：**只启用一个半轴时，55h轴指示不适用**，因此旧单半轴实验的axes00不能单独判异常；真正待查的是未见源bit0及source84h固定零位。已有文档／相关证据和限定六张20260924硬件照片没有可读IMU丝印，精确器件身份仍不能仅据板级清单或WHO6a确定。

复用原`wf_wrist start`会话，启动前取`before`，每轮运行取最新`running`，停用事务读回确认后取`after`，均由原worker独占。每帧先确认BANK读成功且严格为0，才依次读WHO／XL／CTRL3／CTRL10／PULSE／TAP／STATUS、28h起六字节XYZ、AXES55h／SOURCE54h；不临时切bank取证。SOURCE读取仍会清锁存，before也有此副作用；running只读一次SOURCE并据其bit0累计，after在原停止清源及路由恢复后再次新读，不覆盖运行事件缓存。重复stop不重取after；未执行或未成功停止时after可能尚未采集，不能把零字段当测得0。

`wf_wrist status`保留原三行摘要，再输出每阶段`imu-wrist-frame/regs/source/xyz`四行；只复制同一同步缓存、无I2C。`attempted`区分尚未采集，`begin/end`为帧读取起止单调毫秒；`valid`仅代表对应传输成功，不代表配置相符或数据新鲜。失效字段即使显示0也不可采用：

| valid位 | 1／2／4／8／10h／20h／40h | 80h／100h／200h／400h | 全部完成 |
|---|---|---|---|
| 对应字段 | BANK／WHO／XL／CTRL3／CTRL10／PULSE／TAP | STATUS／六字节RAW／AXES／SOURCE | 7ffh |

XYZ输出保留六个原字节及有符号counts，仅当RAW／XL／CTRL3有效且XL20h、CTRL3=44h（BDU1／IF_INC1／BLE0）时`xyz_valid=1`；±2g参考换算为counts×0.061mg，不把counts打印成mg。`ready`仅在STATUS读取成功且XLDA1时置1；XLDA0是没有新数据，非I/O错误，仍可保留旧读数但不证明新鲜。BDU只保证每轴低／高字节配对，不保证XYZ同时，XYZ与源对也非原子，帧区间必须一起解释。

新增帧每次最多11笔读取，原100ms仍是等待请求而非精确采样周期，取消／截止在整帧后再检查，响应包括剩余有界事务；到期／取消帧不会新增observations。每笔沿用50ms总线超时，无后台额外采样／线程。启动／运行帧失败或格式配置不符终止腕会话并走清理；已确认停用后after遥测失败只保留有效位缺失及错误计数，不把已知关闭的设备重新标成活动，停止结果与遥测完整性分别判断。

完整Debug53/53、保留断言Release相关5/5及Product编译／源码产物核对通过；覆盖负值／大小端、三阶段新读与运行缓存保留、部分RAW失败、非0 bank不访问别名、错误CTRL3、after-only失败、采集中途到期／取消、worker共存及status零I/O。[软件证据](evidence/2026-10-20/wrist-register-snapshot.json)保存精确候选、命令与限制。本轮未开串口／下载，尚未取得本版新帧或XYZ实机读数，source84h根因仍未知；不接IRQ／AON／亮屏，也没有硬件算法不足结论。

### 同BIN三阶段与原生事件补测（2026-10-20）

用户随后明确授权下载，精确Product`df9c37d6…`首次write+verify通过，启动恢复129／小时和73，UI／触摸／KEY1正常；无新固件源码或重编。所有有效新帧均bank0／WHO6a／XL20h／CTRL3=44h，原字节与有符号XYZ解码相符；三阶段valid7ffh、CTRL10=14h／94h／14h及PULSE／TAP=00／01／00支持启用与停用配置。before和after新读SOURCE仍84h，故基值异常不限于旧运行缓存。

四方向xneg窗口的末端负X样本不足以证明持续阈值；后续xpos稳定姿态窗口实测“上边缘抬起”为负X，6帧约−1g、端点跨度10675ms，确认映射而非判正X效果。最后改与姿态匹配的60秒xneg，UART捕获`SOURCE85h／AXES40h／observations1`（sample2067196），随后SOURCE回84h且累计1保持；事件前X=−8852、后续多帧接近−1g。该结果支持本次内置AWT来源bit0触发并清除；单半轴AXES不适用，40h只保留为原始观测。事件行无同次完整XYZ，离散样本不严格证明连续姿态，也不证明恰好600ms响应或手势识别率。

60022ms到期、显式stop、停用读回与allow1通过，20:54:38释放COM5；129／native0／小时73／保存代次13不变，用户最终确认屏幕／触摸／KEY1正常。[设备证据](evidence/2026-10-20/wrist-register-snapshot-hardware.json)保留解析错误、静止／错轴窗口及触发／串口等待失败，不把失败窗口算作有效动作测试。source84h保留位与精确器件身份仍未知，原生能力不足未证；保持内置AWT路线，先核对实际中断输出及板上连线再确定亮屏集成。当前无IRQ／AON／亮屏开关、误触／漏检、佩戴或功耗结论。

中断路线仅有资料候选：精确数据手册的`INT2_WRIST_TILT`为AWT专用INT2路由，而`CTRL4_C.INT2_on_INT1`说明可将全部中断OR至INT1；普通`MD1_CFG.INT1_TILT`不等同AWT。既有[板图核查](evidence/2026-10-19/native-daily-steps.json)为INT1→PA31、INT2 NC；本节设备补测所用`df9c37d6…`代码只设AWT→INT2／LIR，未开启该总路由或MCU IRQ。故不能由INT2 NC直接推断必须飞线，也不能声称PA31已收到AWT；路由读回、实装电平与读源释放须先有界验证。

<a id="wrist-int1-route"></a>
### INT1总路由与PA31电平诊断（2026-10-20软件轮）

接续`583c719`，仍采用芯片内置AWT；上节一次来源触发是选择依据，不是物理中断通路证据。精确数据手册§9.16／Table58–59定义`CTRL4_C(13h).INT2_on_INT1`为bit5，将全部中断OR至INT1；§2.2说明读`FUNC_SRC2`可清AWT锁存。新增显式命令`wf_wrist route <1..120秒> <xpos|xneg|ypos|yneg|zpos|zneg> [1..255延迟单位]`，沿用默认600ms／30°和同worker／IDLE期限；原`start`不配置PA31，也不改CTRL4。

route只在收到命令后确认GPIO设备存在，设置PA31为GPIO普通无上下拉输入，随后在原AWT配置中保存CTRL4并写后读回`原值|20h`。不驱动引脚、不注册MCU IRQ或AON、不发KEY／亮屏事件，停止后PA31保持被动输入，不留采样任务。锁定板级代码的触摸IRQ为PA41，按键PA34／43，VBUS为PA44；当前Product和该板基类未发现PA31其他使用，但实装身份／接线仍须设备核验。

每帧新增CTRL4；route另读取INT1_CTRL／INT2_CTRL／MD1_CFG／MD2_CFG，只有四者全零才启用或继续，以排除已知可配置的其他中断源。其他路由异常时结束本次借用，恢复自身CTRL4／PULSE／TAP及计步使能，不声称修复外来配置。route在AXES读取后、清SOURCE前后分别取PA31原始0／1，`pin_valid` bit0／1分别表示两次调用返回有效电平，`pre_ms/post_ms`为各调用前的单调毫秒；不加稳定等待，不把紧邻的post直接称为释放时延。CTRL3仍要求44h（资料定义高有效／推挽），主机模型不能证明实际电气极性。

status继续只读缓存，每阶段附加`imu-wrist-pin`；有效位增加CTRL4=800h，普通帧全传输成功为fffh，route四路由位依次1000h／2000h／4000h／8000h，全传输成功为ffffh。PA31有效位独立于寄存器传输位。新增`event`阶段保留最后一次通过格式／配置／期限检查的SOURCE bit0帧，后续无事件帧和停止不覆盖它；`attempted=0`表示未曾留存，累计次数仍不是手势数。before／after读源也有清锁存副作用，过期／取消帧不进入event。

到期、取消、日常stop及故障均读回恢复借用的CTRL4；恢复失败保留FAULT／IDLE与计步无效，显式stop成功才释放，既有bank恢复规则不变。after遥测失败不会重新承担已确认停止的硬件责任。普通帧最多12笔I2C读取，route最多16笔及两次GPIO读，各I2C沿用50ms超时，取消响应包含剩余整帧；100ms为等待请求。没有新增线程／timer、持久配置或生成UI修改。

软件轮只做源码／主机／编译，精确结果见[软件证据](evidence/2026-10-20/wrist-int1-route.json)；当时未下载，板上仍为`df9c37d6…`。随后新授权的同BIN设备结果见下节，主机模型不能代替该设备证据。

### 同BIN INT1路由设备观察（2026-10-20）

精确`17f1b775…`三镜像verify及启动恢复129／小时73通过。匹配xneg的上边缘动作取得完整event：SOURCE85h／validffff、CTRL4=20、其他四路由00、XYZ`−15834,−116,−4083`，PA31读源前1／后0（302658／302659ms）；随后源bit0清除且event保留。支持本次醒态内置AWT到PA31的总路由与读源前后电平变化，不推断稳定释放时延或ISR／AON行为。

该动作窗约21.5秒因UART一行RAW只含四字节而被助手中止，显式stop恢复CTRL4／PULSE／TAP与计步使能、allow关闭；原始失败不删除，不算60秒到期。修正助手按route标记限定同一报告，避免尾随后台摘要与旧帧拼接，并对不完整报告有界重读，未补字节。随后只补静置60秒，60022ms到期与三阶段ffff／CTRL4=00,20,00／CTRL10=14,94,14／PA31=0,0通过，观察0；保留一次SOURCE80h的bit0为0读数，84h／80h保留位及器件身份仍未知。

前后129／native0／小时73／代次15不变，stop／allow1／IDLE0及22:34:14关闭COM5，用户最终确认UI／触摸／KEY1及129正常。[设备证据](evidence/2026-10-20/wrist-int1-route-hardware.json)区分正向／取消窗、静置到期窗及坏报告；没有非零native保持、完整动作识别率、精确600ms响应、IRQ／AON、常驻开关／专用亮屏、物理DEEP或功耗结论。
