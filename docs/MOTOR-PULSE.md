# 手动限时电机控制

<a id="supply-prepare"></a>
## 2026-10-16 LDO3供电前置（当前契约）

本节覆盖下方首轮PA20后端中被扩展的部分，原始版本和硬件记录仍保留。基于`585c988`，后端在显式pulse时临时借用LDO3（设计对应VOUT2），启动初始化只保持PA20 LOW，不主动开启LDO3。当前RGB驱动未编入、板级PA32下拉；开启`RGB_SK6812MINI_HS_ENABLE`会编译报错，须先实现共同供电所有权。没有新增RGB数据、UI或自动提醒入口，也不承诺与任意直接寄存器写者并发共享。

worker取得IDLE并保持PA20 LOW后调用prepare：保存LDO3原EN／PD／SET，先记录归还责任，再按需请求EN1／PD0，核对字段，等待5ms稳定并再次核对。SET不作电压换算或修改。准备完成后重查stop和原接受时间的截止，失败／取消／到期均先OFF，不进入ON；准备耗时计入请求窗口，截止不重置。检查至GPIO ON之间仍有短竞态窗口，返回后继续检查并关闭；20ms请求不保证20ms物理HIGH。

OFF先驱动并核对GPIO LOW，再精确恢复本次改动的EN／PD，保留SET和其他rails。原EN1／PD0只借用、不关闭；原EN1／PD1矛盾状态拒绝准备。自己开启后若LDO3字段冲突、LOW核对失败或归还失败，保留责任、FAULT和IDLE，拒绝新pulse，显式stop可以重试。`off_latch=1`现在表示LOW与后端资源归还检查均成功，`0`也可能已是LOW但归还未确认；仍不证明pad电压、支路断流或停转。新增prepare错误原因值为8，既有编号保留。

PMU字段的短读改写／核对在IRQ临界区内；5ms等待、GPIO、PM调用和打印在其外。此规则替代首轮“所有I/O均在锁外”的表述。寄存器恢复只保护本次字段，不代表已经建立通用多消费者电源管理。测试、编译、下载和实际反馈分别见[本版证据](evidence/2026-10-16/motor-supply.json)；源码通过不代表电气验收。

本版`4df9a03d…`仅USB三镜像verify与启动禁充／默认OFF通过。19:03:31.122只发一次20ms请求，读到LDO3 EN1／PD0／code4，到期reason1／off_latch1并恢复原`PERI_LDO=00080719`；用户确认“感觉到短振并停止，没有异常”。19:03:43.048恢复allow1／own_idle0／sdk_idle0，19:03:44.052正常关闭、exit0。随后用户明确要求更明显的长振，同版重新启动预检后19:06:07.667只发一次100ms，读到相同开启／到期OFF／供电恢复顺序，用户确认“振感明显，停止正常，没有异常”。独立attempt保留20ms原标记，不自动重试／加长；19:06:23.527恢复allow1／own_idle0／sdk_idle0，19:06:24.536正常关闭、exit0。后发stop均晚于到期，未证明活动中取消；启动峰值、温升和电气匹配仍未验收。各次观测与哈希见本版证据。

## 2026-10-16 首轮软件契约（历史，供电扩展以上节为准）

从`origin/main@600e03b`实现Product手动诊断入口；活动项和操作授权只见[STATUS](STATUS.md#current)。本轮不接入通知、闹钟、倒计时、按键触感或振动强度设置，不改XML／生成物、SDK或设置格式。250ms是初轮软件策略，不是测得的电机／驱动电气上限。

### 引脚与所有权

`product_motor_board.c`独占本轮PA20输出，`product_motor.c`独占状态、截止和后台I/O。Product main在消费者启动前构造一次，先请求OFF，再建立事件／worker；构造不支持并发调用。成功构造后的重复调用直接返回，不重置活动脉冲。UI没有新增入口，也不执行GPIO I/O。

官方V1.2 EPRO中，PA20／`PA_20_VIB_PWM`通过R37到Q2栅极，Q2源极接GND、漏极接MT2；MT1经R36接VDD33_VOUT，R38下拉栅极，D16跨接负载。由该拓扑推断HIGH启用、LOW关闭；这是设计依据，不能替代实际PCB装配核验。原始包哈希、成员路径和网表行见[精简证据](evidence/2026-10-16/motor-pulse.json)。已购电机是两线1027 ERM、采购标称DC3–5V；启动／堵转电流未知，实装驱动和3.3V电源余量仍待核对。

固定SDK的`GET_PIN(1,20)`对应PA20；通用motor的44／45默认值不适用于本板，基础板PA44用于VBUS_DET。本后端不调用通用motor／vibrator驱动。初始化依次设输入、预装LOW、选择PA20 GPIO／下拉、启用输出、读取LOW。输入步骤清除旧开漏标志，预装LOW避免启用输出时沿用旧HIGH。SDK在输出模式读DOR锁存器；`off_latched`只表示数字输出契约，不能证明物理pad电压、支路断流或实际停转。启动前的Boot ROM／板级时序、复位及睡眠保持均未作物理测量。

### 请求、停止与故障

- `pulse(1..250ms)`只在已就绪、OFF、无进行中I/O／待处理stop时接受，返回非零请求ID；无效时长、并发脉冲和FAULT均拒绝，不排队或续时。截止从接受请求的单调tick起算，线程延迟会缩短剩余窗口；低于一个tick的请求按一个tick处理。
- worker在取得IDLE后、ON前及ON回调后检查截止／停止。事件等待空闲时为无限等待，活动时只等现有截止，不新增周期采样。ON回调失败也请求OFF，包含已经产生部分副作用的情形。
- `stop()`只提交取消序号并唤醒worker；返回成功表示已提交，不表示电气停止已确认。停止发生在ON I/O期间仍可能短暂输出，返回后再检查并关闭；OFF I/O期间到达的新stop留到下一轮处理，不会被旧关闭吞掉。
- 唤醒提交失败会返回失败。pulse失败不产生ON；stop失败仍保留取消意图，活动截止不延长，但不能承诺立即唤醒。调用者可显式重试stop。
- 正常到期、取消、ON失败均进入OFF回调；确认数字LOW后释放IDLE。运行中OFF失败进入FAULT、保留IDLE、拒绝新pulse，无后台无限重试；显式stop可重试，成功后恢复OFF。
- 初始化／事件／线程创建或启动失败进入UNAVAILABLE并补偿OFF；补偿失败仍持有IDLE。没有可用worker时拒绝pulse／stop，当前产品通过重启重新初始化，不承诺命令恢复。所有I/O与PM调用在短IRQ临界区之外。

独立worker栈2048字节、优先级18，不能从主机测试推断板上栈余量或调度最坏延迟。250ms是软件截止策略，受tick量化、调度和GPIO调用完成时间影响；不构成CPU停滞、复位或硬件故障时的独立断电保护。持有IDLE只约束软件PM选择，不能代替物理驻留／功耗测量。

### 诊断命令契约

以下只描述接口，不是本轮执行授权；实际启振前按[硬件条件](HARDWARE.md#motor-design-20261016)核验。

| 命令 | 行为 |
|---|---|
| `wf_motor status` | 只复制状态，打印原因、请求ID、busy／stop_pending、请求／剩余时间及off_latch；不做I/O |
| `wf_motor pulse <1..250>` | 严格十进制毫秒参数；只提交单次脉冲，返回成功不等于已实际启动 |
| `wf_motor stop` | 提交取消，或重试FAULT的OFF；以随后状态及独立电气观察判断停止 |

源码测试、固件编译和硬件结果分别记录在[本轮证据](evidence/2026-10-16/motor-pulse.json)与[构建摘要](BUILD.md#2026-10-16-手动限时电机软件)。本轮不烧录、不打开串口、不发送pulse；设备保留`310ad9fe…`及既有设置，不新增启停、触摸、睡醒、电流或温升通过结论。

<a id="default-off-hardware"></a>
## 同日后续：默认OFF上板

PR #70合入`c658d55`后，用户继续并确认同款电机／VIB、无万用表。本轮只做默认OFF启动，用户确认断电池／仅USB后下载既有BIN`cd88555b…`，三镜像write／verify退出0；没有源码修改或新构建。两段显式150ms RTS后均取得完整启动、early禁充验证和`off / request=0 / off_latch=1`快照。RTC在两次RTS后均观察为2000年，各自按当前UTC校时并读回；第一段hold下软件亮屏约310ms。第一段关闭后用户回复已息屏，故追加第二段界面观察；用户分别确认显示／无自行振动、触摸滑动／返回正常。最终12:14:50.989回读allow=1，12:14:57.521正常关闭、两个helper退出0，串口已释放。

这是初始化数字OFF、启动与有界UI观察，不是实测启振、断流、复位关断或物理DEEP验收。OFF-only采集器拒绝pulse，后续启振仍缺实物驱动／供电与负载匹配证据。补核设计R36为5.1Ω／62.5mW；用户截图补充电机适用2–5V／额定3–3.7V及电流68–110mA，但未标启动上限，不能由串联电阻或Q2单管额定证明启动余量。对应实物规格或已核验上限的限流验证可补齐，无需先做微安基线。实际供电、完整回执和收尾见[默认OFF证据](evidence/2026-10-16/motor-default-off.json)。

<a id="first-pulse-hardware"></a>
## 同日后续：一次20ms请求，振感未确认

默认OFF记录PR #71合入`b0aaf3d`后，用户知悉启动峰值及过载／压降风险，明确授权仅USB、人在旁的一次20ms功能试探。沿用`cd88555b…`，未改源码／构建／烧录。独立one-shot采集器发送前独占创建持久化attempt标记，只允许此次20ms；完整启动禁充／默认OFF通过后，12:45:10.193发送一次，读到ACTIVE/request1、12:45:10.216到期reason1／OFF，随后status确认OFF。约104ms补stop后reason2／OFF；收尾另发stop只作关闭复查，不增加ON。

用户回复“刚才没有留意，不清楚”，物理振动／停止未确认。日志中的请求20ms和到期时间不能证明物理脉宽，DOR不能证明实际电压／电流。采集只有一个启动banner、未见assert／HardFault或电机故障日志；息屏后充电器raw复查另有5秒I²C超时，保留失败，不扩为全程禁充或电机故障结论。PM许可恢复、正常关闭与exit0均确认，无自动重试／加长。版本、命令、哈希、未验证供电轨及建议诊断见[单次证据](evidence/2026-10-16/motor-first-pulse.json)；本次授权不关闭上文电气未知项。

<a id="power-read-only"></a>
## 同日后续：外围LDO只读诊断

`wf_motor_power`只接受无参数调用，在shell线程单次读取`hwp_pmuc->PERI_LDO`，所有输出解码同一局部raw字；报告LDO2和LDO3各自的`en_req`／`pd_req`／`vout_code`，保留矛盾组合，不换算为测得电压。它不经过电机worker，不调用GPIO、PMU setter、启停或PM申请，不创建线程／timer，不自动绑定UI。无效参数在读取前返回错误；每行保持在SDK 128B console buffer内。此入口仅作瞬时寄存器观察，不能确认电机端供电、实际电压／电流、脉宽或停转。

锁定SDK `drivers/cmsis/sf32lb52x/pmuc.h`定义LDO2 EN/SET/PD为bit8／bits12:9／bit13，LDO3为bit16／bits20:17／bit21。初稿错误标注VOUT2=LDO2，虽编译成功但没有下载；`docs/source/zh_CN/app_development/startup_flow.md:77`明确LDO2对应芯片VOUT1，因此修正为两路并列输出。[DS5203 V0.1](https://downloads.sifli.com/silicon/DS5203-SF32LB52-MOD-1%E6%8A%80%E6%9C%AF%E8%A7%84%E6%A0%BC%E4%B9%A6%20V0p1.pdf)物理第27页U0101-B进一步直接标明LDO3→芯片pin14 VDD33_VOUT2；这是设计映射，不代表实物电压。固件保留`mapping/voltage/current/motion unverified`的保守提示，实物映射／输出仍未测。命令没有任何供电使能行为，不能由新入口存在推断前序20ms实际启振。

最终相关Debug／Release各2/2、Product编译和404／29重哈希通过；用户再确认仅USB后下载`a419023a…`，三镜像verify退出0。启动禁充／默认OFF以及亮屏／息屏hold下两次`PERI_LDO=00080719`通过，LDO3 EN0／PD0／code4，未发pulse／stop或供电写入。PM许可恢复、串口正常关闭；实际供电／启停及当前全量UI／KEY1未验。命令、哈希、校时与收尾见[同版记录](evidence/2026-10-16/motor-power-readonly.json)。
