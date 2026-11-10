# OTA前置评估与恢复边界

初始研究为2026-11-07，基线`bee670a`；SDK仍锁`v2.5.1 / 421126d9…`。11/08从该远端主线建立`codex/ota-foundation`，开始路线验证与实施。本页保存版本化证据、候选路线和验收契约，唯一活动项及下一步见[STATUS](STATUS.md#current)。

## 阶段判断

**2026-11-08实施更新**：用户在先行规划后明确开始OTA、提交应保存的Git改动，并要求实质取舍先说明和讨论。原规划已提交到本地`codex/wearable-integrated-layout@12bdbb0`；本阶段从主线独立开展OTA，硬件规划暂缓，11/07并行硬件准备建议不执行。路线容量、恢复、认证与数据保全先取得证据；部署另核具体版本与现场。

**后续已选范围**：用户在比较成熟工具链和恢复能力后，明确选择首版以A/B自动回滚为目标，先验证映射与独立loader，再冻结分区。原两条SDK路线保留为能力与缺陷参考；不直接部署其默认错误路径。第一闭环见[启动模型与包核验](#ab-foundation-20261108)。

用户明确需要适合长期佩戴的尺寸、电池布局和维护空间，以支撑尚未接入的长期PPG／血氧检测；当前缺少可用电流测量仪器和正常使用电流数据，认为继续在黄山派增加功能价值有限，提出优先OTA。OTA作为现平台的维护能力闭环，同时保留紧凑结构、健康光机电和测量条件这些硬件缺口；不以完成旧功能清单作为OTA的统一前置。

OTA可减少后续封壳后反复连接下载口的维护负担，也能复用现有BLE和升级后的软件资产；首装引导／loader及失效救援仍需可靠入口。它不能提供电流数据，也不能代替贴肤结构、健康算法或长期佩戴验证。

11/07曾建议并行准备分阶段硬件验证；后续用户决定暂停硬件规划，该建议保留在外壳研究分支。本阶段只推进OTA，OTA完成不自动触发整表投板。

长期血氧目前是用户提出的使用方向，采样频率、连续时段、准确度和验收参考尚未确定。依旧先闭合[健康前置](HARDWARE-CAPABILITIES.md#health-prereqs-20261025)中的准确模块、红光／红外输入、贴合与遮光、离腕／运动质量及算法／标定；长期记录数值不能直接证明长期可靠SpO₂能力。现黄山派可保留作台架和日志对照，最小系统／现屏触验证范围仍有效，尚未决定采购板型或自制PCB。

## 旧规划对账

已回读聊天“梳理项目过去现在未来”（`01a17ca8-a1da-7da2-bfc6-9c04d2838adc`）；下表按当前仓库补正其历史状态，不复活旧顺序或设备授权。

| 旧规划 | 已有结果与剩余边界 |
|---|---|
| Flash／EZIP／字库 | [资源闭环](FLASH-CAPACITY.md#2026-10-29-剩余小时图扩展与压缩备选)已有软件与设备记录；普通Product当时BIN3,019,260B、8MiB应用区余5,369,348B。历史“只余32,640B”不再是最新账单，应用余量也不是OTA下载区容量 |
| 最小长期运行记录 | 已实现并有超过24小时的运行／保存证据；[该次报告](evidence/2026-11-01/runtime-24h.json)仍有未恢复fault及event／sample drops，不能称整机稳定全部通过 |
| 普通亮屏／手电筒／OFF观察 | 已做短窗观察及后续低压／曲线实验；完整正常使用电流、能量和续航仍缺，电压差不能换算真实mAh |
| 指南针 | 已有未校准XY航向与相对响应；[安装零点、校准、倾斜补偿](COMPASS.md)未闭合 |
| 闪屏、活动三项真实算法、声音 | [原契约](ARCHITECTURE-REVIEW.md#followup-20261025)保留；原生步数不等于热量／强度／站立，现手电筒不等于闪屏，喇叭资料不等于声音功能完成 |
| 健康／夜间基础记录 | 硬件与算法前置已有整理，产品PPG采样尚未打通；既有首版真实PPG／记录目标不由本轮取消 |
| 产品受控重启 | SDK普通reboot已有分版本记录；统一保存屏障和服务退出协调仍需实现。OTA只纳入进入升级所必需的部分 |
| 真实关机／再次开机 | 独立待办，仍须核电源／按键拓扑与USB影响；不用关屏、休眠或RTS复位代替，不强行并入首个OTA闭环 |

## 已核到的OTA基础与缺口

| 对象 | 静态事实 | 对方案的影响 |
|---|---|---|
| Product当前接入 | [SConstruct](../apps/product/project/SConstruct#L7)加入bootloader、LCPU和ftab，未加DFU子工程；既有Product构建配置未启BLE serial／DFU／DFU V2，既有下载清单为bootloader／main／ftab | 当前不能通过手机执行Product OTA。SDK里有DFU对象名或分区名不证明产品服务已部署 |
| 当前分区 | [Product ptab](../apps/product/project/sf32lb52-lchspi-ulp_hcpu/ptab.json#L7)为8MiB main、384KiB旧DFU代码、1,179,648B下载区、4MiB FS及KVDB；无`DFU_V2_LOADER`标签 | 不是双完整应用镜像。新增V2标签／loader尾部fwinfo或调整容量均需核对地址、原日志和设置保全 |
| 手机工具 | [旧官方例程](../vendor/SiFli-SDK/example/ble/peripheral_with_ota/README.md)列Android工具与Android／iOS示例；V2文档说明复用BLE协议 | 可优先核官方工具推送本地包，尚未验证当前工具与Product适用性；Gadgetbridge未形成项目OTA闭环，不默认需要先写手机升级功能 |
| 项目保护 | Product现有禁充及LCD／charger共享总线是自有覆盖路径；独立loader另有启动与配置 | Product验收不能自动覆盖loader。必须重审黄山派启动默认充电、LCD／引脚和供电行为，不能只启升级协议 |

### 参考路线一：旧offline整包暂存与安装（11/07核查，未选为首版方案）

官方[peripheral_with_ota](../vendor/SiFli-SDK/example/ble/peripheral_with_ota/README.md)演示主程序接收压缩包到备份区，重启进入DFU解压安装。每约10240B分块压缩属于打包／解码颗粒；BLE传输分包也不能取消完整包暂存要求。[dfu_ctrl.c](../vendor/SiFli-SDK/middleware/dfu/dfu_ctrl.c#L332)按擦除粒度处理整包长度，超出下载区会拒绝。

对约3,019,260B的历史普通main，只有main时，1,179,648B下载区也要求包约为原镜像的39%以内，并继续容纳头部与实际安装内容；若包含额外镜像，预算更紧。官方经验系数0.7不是当前镜像实测比率；已有压缩图像也不能保证进一步压缩收益。**当前未生成包，不能断言能放下，也不能直接以main大于下载区断言OTA不可行。** 本轮限定核查未取得通用应用差分安装闭环，不能用差分作空间假设。

### 参考路线二：DFU V2独立loader直写（11/07核查，默认实现不直接部署）

锁定SDK的[dfu_v2.c](../vendor/SiFli-SDK/middleware/dfu_v2/dfu_v2.c#L474)明确无暂存、无解压／解密步骤，loader可以直接写非自身目标分区；这可绕开旧offline完整包暂存限制，但不等于A/B回滚。官方模板给loader预留2MiB，而当前旧DFU区384KiB；本轮未编译，模板预留不证明实际必需2MiB，也不能保证384KiB够用。

**必须先处理的恢复风险（源码静态可达，尚未设备复现）**：

- [loader超时](../vendor/SiFli-SDK/example/dfu_v2/ble/loader/project/board/main.c#L189)在升级未完成时也重启；初始化检查会[清理遗留更新标志](../vendor/SiFli-SDK/middleware/dfu_v2/dfu_v2.c#L479)。[bootloader路由](../vendor/SiFli-SDK/example/boot_loader/project/butterflmicro/board/main.c#L361)依赖更新标志与loader检查，本轮未找到使所有普通main启动都受“升级完整提交”控制的门槛。因此中断后存在进入半写main的风险，不能把例程重启或FlashDB续传称为产品恢复保证。
- 完整性、发布者认证、恢复和回滚分别验收。旧整包CRC或V2传输校验不能自动等同签名认证；V2的BLE路径、引擎条件编译和默认开发密钥必须作为完整链路核查，不能仅因启用mbedTLS宣称可信升级。
- 续传可以重发剩余内容；旧版回滚需要另有完整旧镜像与启动策略。现布局没有第二个完整main，不能承诺升级失败后立即回到旧版。

官网的[DFU V2文档](https://docs.sifli.com/projects/sdk/v2.5.1/sf32lb52x/middleware/dfu_v2.html)和[BLE示例](https://docs.sifli.com/projects/sdk/v2.5.1/sf32lb52x/example/dfu_v2/ble/app/README.html)只作为能力与流程参考；该例程声明的平台为`sf32lb52-lcd_n16r8_hcpu`，不等于黄山派Product已经适配。实现证据以本地SDK锁为准。

<a id="first-bounded-result"></a>
## 阶段成果与验收边界

进入实施时先取得路线与恢复证据；随后用户已选A/B自动回滚。分闭环交付启动模型与主机包核验→项目loader／boot映射原型→真实持久／签名及传输→同版升级、回滚与代表回归。准确loader尺寸和地址、版本／板型／认证、数据保全、保存退出、供电默认态及有线救援继续是后续闭环的进入条件。每次只闭合当前成果，不将主机模型作为上板放行。

已进入OTA实施，首个功能范围沿用：由用户主动发起，手机推送已知本地固件，经明确的升级环境更新整个main，重启后核版本并运行现有代表回归。首轮不自动加入云端版本服务器、PAN、自写手机App、运行中热换UI资源或远程更新bootloader；这些扩展另有收益和风险。SDK锁保持，具体独立子工程／适配所有权须在方案评审中明确。

进入升级所需的受控退出应等待已接受的设置／记录保存，阻止新的冲突Flash写入，停止或移交当前服务责任，并给出失败反馈；不为OTA先重构所有应用。无PPK2不阻塞这项软件评审，但设备实验仍须有已核供电、loader充电行为和恢复入口，不能仅凭估算百分比设安全准入。

| 后续验收情景 | 必须可说明的结果 |
|---|---|
| 已知正常包 | 新版本可识别、核心功能恢复、配置／记录按既定迁移契约保留 |
| 错板型／越界／损坏／未获准包 | 在对应阶段拒绝，解释哪些区域已写入及可恢复状态；不把传输结束当升级成功 |
| 断连／取消／超时／供电中断 | 保持可进入的升级／有线救援路径，不启动半包；未具备旧版回滚时明确使用重传或救援 |
| 新程序有效但业务故障 | 有限失败检测与可用恢复入口；不把镜像校验通过等同于新程序可用 |
| 保存或服务退出失败 | 不进入冲突擦写，反馈原因；不能静默丢弃已接受设置或原日志 |
| 升级后的基础回归 | 屏触、BLE、RTC、提醒、计步／腕事件、保存及息屏恢复按新版本复验；不顺带声称深睡电流或七天续航通过 |

实际获准下一步只回STATUS；11/07的静态结论与11/08的软件结果按下节区分。

<a id="ab-foundation-20261108"></a>
## 2026-11-08 第一闭环：包核验与A/B启动模型

### 真实容量与工具结果

普通Product基线构建`20261108-151740-645`成功，BIN3,028,888B；625源码／29产物逐哈希及完整ZIP读回通过。锁定SDK实际选用`example/boot_loader/project/butterflmicro/ram_v2`，不是另一个sf32lb520启动工程。当前main8MiB、下载区1,179,648B与FS／KVDB地址均未修改。

官方`imgtoolv37.exe gen_dfu --img_para main 16 0 --com_type=0 --offline_img=2`首试失败：打包器调用EZIP的`-gzip`接口，而锁定工具显示2.6.4且帮助中没有该接口，输出convert_num0后缺`temp_com.bin.gz`。失败保留在本机`artifacts/ota-preflight-20261108/offline-main01/`，没有更换SDK或工具。

同工具改用受支持的`--com_type=1`生成zlib包1,557,071B。主机检查CRC32/MPEG-2和296个10KiB块，解压后逐字节匹配原BIN及16字节对齐补零；该包需要`DFU_DECOMPRESS_USING_SOFTWARE`，不能交给默认硬件gzip路径。包超过现下载区377,423B；按[SDK非55x NOR4KiB擦除](../vendor/SiFli-SDK/middleware/dfu/dfu_ctrl.c#L337)向上取整需1,560,576B，超380,928B。此结果用于容量比较，不决定A/B必须使用压缩包。

新增[scripts/ota_preflight.py](../scripts/ota_preflight.py)只读验证现有构建和官方main-only包，复用`lab_check.check_build`／`lab_evidence`；对头部固定值、镜像白名单、精确长度、CRC、受限单流解压、对齐及实际擦除范围分别检查。超容量退出2，格式／构建绑定错误退出1。它不打包、部署或打开设备；legacy包头不在其CRC覆盖内，主机白名单和往返匹配不构成设备端签名认证。审查发现原始相对路径可能重新指向未核BIN，已改从已验证images清单读取绝对路径并复核SHA，补对应回归。

### A/B范围与映射证据

用户已选择自动回滚目标。容量预算为A=`0x12020000`、B=`0x12420000`，各4MiB，可容纳当前普通main，每槽余1,165,416B；FS与KVDB地址优先保持。地址及容量是原型输入，尚未修改正式ptab或冻结分区，也没有实测映射启动。

[实际boot代码](../vendor/SiFli-SDK/example/boot_loader/project/butterflmicro/board/main.c#L166)在NOR非加密路径也先配置alias再跳转。[HAL](../vendor/SiFli-SDK/drivers/hal/bf0_hal_mpi_ex.c#L441)和[52x寄存器字段](../vendor/SiFli-SDK/drivers/cmsis/sf32lb52x/mpi.h#L433)提供1KiB粒度range／offset，静态上可表达B物理地址映射到A链接地址。但[NOR普通读](../vendor/SiFli-SDK/rtos/rtthread/bsp/sifli/drivers/drv_spi_flash.c#L169)走memory copy，B运行时读A可能读到B；不能用它冒充inactive A物理校验。优先评估在独立loader内完成下载／写擦／物理核验，避免应用映射运行期间访问待写槽；仍须核ROM实际链接、缓存、Flash忙期间取指和复位后映射。

默认BLE V2还跳过镜像签名，并在[发送结束转换](../vendor/SiFli-SDK/middleware/dfu_v2/transport/dfu_transport_ble.c#L1428)中用自身累计CRC作期望值；[commit写fwinfo](../vendor/SiFli-SDK/middleware/dfu_v2/engine/dfu_engine.c#L684)禁用。旧offline的安装返回值／状态错误路径也未达到本项目恢复要求。SDK能力可以复用，产品提交门槛与完整验证需由项目适配明确实现，不以配置宏存在代替证明。

成熟工具链参考：[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html)提供非活动槽、试运行确认及回滚，[MCUboot](https://docs.mcuboot.com/design.html)区分swap、overwrite与direct-XIP等模式。它们是取舍依据，不证明思澈本板已经具备同等机制，本轮不迁移其他平台架构。

### 已实现的软件边界

[core/ota_ab.c/h](../core/ota_ab.h)是纯启动决策模型，拥有128B版本化记录、镜像身份、稳定槽／候选槽及三态，调用者负责同步；没有RTOS、堆、设备I/O、地址常量、擦写或启动调用。板型／长度／哈希／签名等物理镜像验证由后续loader提供，模型输入身份不构成验证实现。

| 状态／动作 | 本模型行为 |
|---|---|
| STABLE → READY | 仅将另一槽的已核身份登记为候选，稳定槽身份保持；真实适配仍须拒绝擦写稳定槽／loader／保留单元 |
| READY → TRIED | 首次试运行前生成更高代次记录，必须精确持久读回后由`resolve()`给出候选槽；失败或旧记录读回则选已核稳定槽 |
| TRIED启动后确认 | 运行槽及完整身份匹配才生成新稳定状态，真实保存成功前不生效 |
| 未确认重启 | 选择旧稳定槽并生成清理候选状态；旧稳定镜像无效则进入恢复 |
| 单条READY存活、另一条损坏 | 保守按TRIED处理并回稳定槽，防止较新试运行记录损坏后反复尝试候选；可能放弃一次尚未完成的升级 |
| 元数据为空／损坏／冲突 | 不自动初始化或默认跳main；首次有线部署需显式可信provision，缺少有效状态进入恢复 |

记录CRC只检测偶发损坏，代次不回绕、相同代次冲突拒绝；两条记录必须由真实适配放在不同擦除单元并保留旧条。模型不证明真实Flash掉电原子性、认证、持久适配、loader充电／BLE行为或实机自动回滚。本轮当前关闭边界仅为这些软件契约与主机验证；下一闭环接项目loader／boot原型，随后再安排具体设备实验。

### 验证与接续

`python -m unittest discover -s tests -p 'test_ota_preflight.py' -v`为11/11通过。`cmake --build artifacts/host-ui-build --target ota_ab`及Release目录`artifacts/flashdb-gc-release`同目标构建后，各运行`ctest --test-dir <目录> -R '^ota_ab$' -V --timeout 60`，Debug／保留断言Release各1/1、2566项检查通过，覆盖候选确认／回退、2048个记录单bit翻转、四种状态提交各129个前缀中断位置，以及空白／短读／代次冲突。

首次只读审查发现“较新TRIED损坏后退选READY再次试启动”的路径，已改唯一READY保守按TRIED处理，补1024项该路径回归；最终定点复核无新阻断。上述是内存记录和纯模型，未操控设备或模拟真实Flash延迟／缓存／供电。

最终普通Product`20261108-154214-928`／UI Demo`20261108-154407-923`均编译通过，627／530源码、29／22产物及完整ZIP读回通过；ELF均不含模型符号。最终main大小仍3,028,888B，重链接字节与基线不同，旧包被精确匹配检查拒绝后按最终BIN重打；最终包1,557,077B，擦除预算不变。首归档命令10秒超时，保留残件并改为新`delivery-*`归档，最终逐项核验。版本、源码哈希、失败及[精简证据](evidence/2026-11-08/ota-ab-foundation.json)一并保存；本轮无设备操作，签名与真实A/B启动仍未接入。

<a id="loader-probe-20261108"></a>
## 2026-11-08 独立只读loader与双记录持久层

本轮从首闭环已合入的`eaba0be`续接，关闭边界为可审阅的软件原型、准确链接容量、受守卫只读入口和双记录故障模型。正式boot路由、RAM解除alias、B槽执行、真实签名／Flash写擦和设备实验各保留后续准入，不能由本轮编译或主机测试替代。

### 所有权、候选地址与启动保护

项目独立工程在`apps/ota_loader`，复用锁定SDK与官方SCons；共享的Product禁充逻辑及core源通过官方VariantDir编译到本工程构建区，避免对象文件落入源目录。独立候选ptab保留A=`0x12020000`、B=`0x12420000`各4MiB，loader=`0x12820000`／384KiB；下载区、FS、BLE和settings地址沿用现布局。现`KVDB_DFU_REGION`仅在该诊断工程候选为两份8KiB元数据单元，原DFU FlashDB被排除；这不是正式分区变更或未知旧数据的格式化许可。

loader以强`BSP_PowerUpCustom`覆盖板级弱实现，保留PA38／PA26／PA30及500μs时序，复用`aw32001_inhibit`设置CEB并核寄存器；禁充失败则读取入口拒绝。warm调用使旧成功状态失效。关闭自动充电、PM、LCD／触摸、音频、BLE、DFU、FlashDB、DFS和FinSH；启动时仅输出一次诊断。SDK禁FlashDB后仍包含FAL，调试shell的`regop`又可任意写寄存器，因此最终直接关闭整个shell，并在配置和ELF中检查。vendor保留的RAM区低层写擦函数可能仍存在；项目不引用它们，也没有journal写回调或可写shell入口。寄存器禁充读回不等于物理充电电流实测。

### 物理读取与映射边界

`physical_read`每次至多256B，只接受四个已限定区域内的偏移，防加法溢出／越界；读取前后均核准入，短读或准入变化会清零结果并冻结该reader。平台端与journal共用busy锁，要求精确NOR ID／16MiB／dual0／4KiB SDK擦除对齐、同一handle、AES关闭，以及`AASAR==AAEAR==AAOAR==0`。8KiB是元数据分配单元，不是单NOR的硬件擦除对齐。该检查复用[既有FS读取的元数据观测](HARDWARE.md#2026-11-01-fs_region读取及普通记录器恢复)，本轮没有重新读取设备。

槽诊断只输出各槽前8B，元数据经`wf_ota_journal_load`解析；前缀不构成镜像完整性／身份认证。映射预算按[HAL实现](../vendor/SiFli-SDK/drivers/hal/bf0_hal_mpi.c#L474)输出B→A的相对起点`0x00020000`、终点`0x00420000`及偏移`0x00400000`，仅打印而不apply；寄存器值本身为低10位对齐的字节地址，不再次左移。

**实际限制**：官方[boot启动路径](../vendor/SiFli-SDK/example/boot_loader/project/butterflmicro/board/main.c#L171)即使源地址等于执行地址，也会设置非零range的identity alias。当前守卫会保守拒绝该状态，不能据本轮构建称实机物理读取已可用。必须进一步在项目自有boot入口／RAM代码中验证解除与切换映射、缓存、B执行及复位路由；不能从正在执行的Flash代码中直接试清映射。当前原型不生成FTAB、bootloader或部署清单，`deployable=false`。

### 双记录持久契约与故障测试

`core/ota_journal`为A/B模型增加平台回调层：两个独占擦除单元、同一锁内重载当前代次及完整身份、只擦写另一单元、全单元空白读回、精确写入／sync／128B读回、旧记录复核与最终重新选择。仅接受模型允许的状态迁移，不允许READY直接确认、伪造稳定镜像身份或代次回绕；任何I/O／超预算错误冻结该实例写入，失败readback清零。首次provision只接受两个全空白单元及generation1稳定状态，不会因加载失败自动擦除。软件预算不能抢占阻塞的底层调用，平台仍须提供有界访问与真实持久语义。

Debug及保留断言Release分别通过：journal17071项检查（164个回调故障、516个编程前缀截断、16388个擦除前缀截断及生命周期／拒绝路径）、物理读取守卫21项、原A/B模型2566项。覆盖四种模型迁移，短读写／假成功、旧记录保全、非法迁移、越界／溢出及读取前后准入变化。实际适配仅绑定journal读取，写擦／sync均未绑定；内存NOR前缀故障模型不证明真实Flash任意位序、缓存或断电持久性。

### 构建、失败与交付证据

`pwsh -NoProfile -File scripts/Build-Ota-Loader.ps1 -Jobs 4`最终`20261108-162953-142`退出0：BIN134344B／SHA`bbe84ad6…`，map中ROM384KiB内占141335B，静态RAM42564B。脚本检查强启动覆盖、必需读取实现、关闭服务／shell、无journal提交／provision／alias设置符号及无部署清单。24份项目输入、7份产物逐哈希和完整ZIP读回通过，归档仅本机`artifacts/ota-loader-prototype-20261108/`保存。

五目标Hello／BLE／Bringup／UI Demo／Product本地基线全过，源码／产物及ZIP读回均通过；普通Product3,028,888B、UI Demo2,719,516B，ELF均排除OTA实现。全量Debug94/94，相关Release3/3；命令为`cmake --build artifacts/host-ui-build`及`ctest --test-dir artifacts/host-ui-build --output-on-failure --timeout 60`，Release配置后构建`ota_physical_read ota_journal ota_ab`并运行`ctest --test-dir artifacts/flashdb-gc-release -R '^ota_(ab|journal|physical_read)$' -V --timeout 60`。固件环境沿用Python3.13.15／ArmGCC14.2.1／SCons4.10.1，SDK及子模块锁保持。

前序额度中断保留三份失败日志；接续修复Kconfig未select自定义map和外部对象落源目录。中间符号检查拒绝SDK保留的低层写擦／可写FAL shell；审查进一步发现regop可绕过guard，最终关闭整个shell。另一审查发现把8KiB分配单元误作4KiB硬件对齐会恒拒绝，已分开表达并复核。最终独立只读复核无新阻断。[精简证据](evidence/2026-11-08/ota-loader-prototype.json)保存基线、哈希、失败与边界；本轮无设备操作，正式分区、真实映射执行、签名与自动回滚均未放行。

<a id="ram-boot-20261108"></a>
## 2026-11-08 项目自有RAM boot软件原型

从PR209／`2381d08`进入本闭环。已确认官方boot整个执行区在片内RAM且`cache_enable`为空，因此把项目映射修改也限定在复位后的RAM boot阶段；loader保持物理读取环境，不在本轮引入运行中的RTOS／XIP直接切换。关闭边界为真实RAM链接、可检查的寄存器窗口、结构计划模型与无镜像启动的诊断入口；B执行及复位恢复仍需设备实验。

### 工程、入口与旧路径隔离

`apps/ota_boot`独立使用官方SCons／NO_OS与板支持，`link.lds`直接包含锁定SDK的RAM boot链接脚本；候选ptab明确分开Flash存储`0x12010000`／64KiB、代码／常量／向量表RAM`0x20020000`／64KiB、数据RAM`0x20040000`／64KiB。栈8KiB和堆3KiB沿用官方脚本，A/B、loader及现FS／KVDB地址沿用候选账单。没有修改Product正式ptab或SDK，也不调用AddFTAB、AddBootLoader、GenDownloadScript。

项目拥有`entry`、`hw_preinit0`、MSP/cache钩子及`boot_error`，通过VariantDir引用官方board／Flash／efuse支持。排除vendor `main.c`，避免旧`boot_images_help/select_boot`的配置写回与自动选镜像；错误只在RAM记录并停止，不自动重启。早期钩子保留官方低版本电源处理和efuse bank0读取，不依赖已初始化全局变量；RESERVE0 RAM hook／resume旁路在该诊断中不支持，不能据此改变现Product的PM契约。

官方`dfu_flash_init()`的switch仍带SD后端链接依赖，因此引用其官方支持源，但entry在调用它之前就拒绝非NOR。**初始化仍要求现有FTAB magic有效**；它会先读旧FTAB再归一化alias，任意继承alias若使FTAB不可读会停止，不是无FTAB依赖的恢复程序。没有新的FTAB或从Boot ROM进入该原型的实际部署证据。boot诊断也尚未完成电气准入，loader的禁充读回不能倒推为boot阶段已有物理保护证明。

### RAM窗口与结构计划

诊断entry先拒绝异常上下文、非零CONTROL、boot RAM外的VTOR及开启的I/D cache；按官方顺序初始化Flash，核16MiB／dual0的NOR handle及AES关闭。`wf_ota_ram_apply`在RAM参数／栈／向量表、无其他Flash使用者的前提下，保存PRIMASK并禁IRQ，DSB／ISB后关闭原range与offset，再设置新值，屏障后精确读回；不一致就停在RAM。该窗口没有日志、回调、HAL调用或Flash常量依赖。本诊断实际只应用全零alias，然后读取loader前两个向量，记录结构计划并停止。

`boot_plan`检查Flash／槽／loader的1KiB对齐、范围／加法溢出、连续且不覆盖的两槽与loader、镜像长度内的Thumb PC和RAM内8B对齐SP。A和loader计划无alias；B仍以A为链接向量地址，生成相对start=`0x20000`、end=`0x420000`、offset=`0x400000`。它只是结构计划，SP／PC看起来合理不构成镜像完整性、签名、板型或健康认证；当前诊断用loader区上限作结构边界，也没有声称知道真实镜像长度。

保留的`wf_ota_ram_jump`只供静态检查：停止SysTick并清SysTick/PendSV pending，按ICTR清所有外部IRQ enable／pending，再设置VTOR、CONTROL／BASEPRI／FAULTMASK／MSPLIM／MSP并分支。它不被诊断入口调用，认证身份、试运行持久化和电气准入均未绑定。启用前仍须验证目标栈与异常帧覆盖、NMI／HardFault、真实外设／LCPU／DMA清理，以及目标初始化行为；结构模型通过不放行跳转。

### 检查与证据

`Build-Ota-Boot.ps1 -Jobs 4`最终`20261108-171758-563`成功：BIN31436B／SHA`24f11089…`，代码RAM占32327/65536B，数据RAM占33880/65536B。`ota_boot_check.py`调用官方nm／objdump，核所有ALLOC／LOAD section、必需符号与数据的RAM放置，拒绝旧选镜像／写配置／shell入口；按nm精确尺寸核映射与跳转函数没有调用、PC相对常量或窗口外分支。最终apply36条、jump41条，均在代码RAM；源码及反汇编未发现诊断路径调用jump。检查器只检查符号化外部引用，不是通用间接控制流证明。

`python -m unittest discover -s tests -p 'test_ota_boot_check.py' -v`为9/9，通过正常叶函数及Flash地址、调用、外部分支、PC literal、嵌入数据、缺屏障和错误跳转尾部的拒绝回归。Debug／保留断言Release的`ota_boot_plan`各1130检查，覆盖A/B/loader计划、SP／PC边界、布局破坏、1023个未对齐偏移及审查发现的Flash基址反例；地址模型采用预期半开窗口，不证明硬件终点比较或MPI内部预取语义。

全量Debug95/95、相关Release4/4（`ota_boot_plan ota_physical_read ota_journal ota_ab`）通过；五目标本地基线全部成功，普通Product／UI Demo ELF排除OTA实现。18份boot项目输入、7份产物及六个目标的源码／产物ZIP完整读回通过，本机归档在`artifacts/ota-boot-ram-20261108/`，[精简证据](evidence/2026-11-08/ota-boot-ram-prototype.json)记录命令、哈希与版本。

首构建因板级charger配置残留、同名board.h及重复efuse声明失败；修正后官方Flash初始化带来的SD链接依赖暴露，补官方源而不增加可选运行路径。检查器随后分别误分类copy/zero启动表、误把jump自身的IRQ循环当外部引用，均按实际section／符号边界修正并增加回归。只读审查另发现Flash基址未对齐会产生错误alias，以及未来jump需清IRQ／SysTick，已修复并定点复核；最终无新源码／构建阻断。

**设备与接续门槛**：本轮没有设备操作，不能声称清alias、B执行、Boot ROM搬运、真实reset范围或自动回滚已通过。SYSRESETREQ不能仅由源码存在就当成MPI／LCPU／DMA归零，冷启动／无其他Flash使用者仍须实验确认。正式分区不冻结；受控A/B执行实验须先形成准确镜像、写入区、供电、ROM有线救援及备份恢复评审包，再讨论部署。活动顺序只回STATUS。

<a id="mapping-experiment-preparation-20261108"></a>
## 2026-11-08 受控映射实验准备（阶段未关闭）

用户继续并确认目前只有原USB串口。主机只运行sftool0.2.5的`--help`、`read_flash --help`及`write_flash --help`，未给端口、未接触设备；公开CLI没有RAM load/run。SDK虽有J-Link和串口现场调试桥说明，它们不证明本机已具备调试连接或ROM冷入口RAM执行。此阶段按USB路线准备，不采购或改接线；当前成果作为同阶段检查点保存，UART观测、执行boot与设备准入未齐，不单独关闭／开PR。

### 最小镜像与固定范围

`apps/ota_slot_probe`复用官方SCons／锁定工具链，拥有专用向量、Reset_Handler与极小linker；不使用SDK SystemInit或C运行期初始化。256项向量都指向明确入口／停止处理器，代码／常量链接A=`0x12020000`；候选部署仅B=`0x12420000`首4KiB。栈固定`0x20001000–0x20002000`、结果`0x20003000–0x20003100`，与RAM boot代码／数据区不重叠。没有.data／.bss初始化依赖，不调用HAL、RTOS、擦写、充电或复位；故障处理在结果首字写故障标记后停止。

运行时预期记录真实PC／SP、VTOR／cache／PRIMASK、alias元组及从A视图实际读取的专用marker，完整写结果后最后提交magic。目前仅留RAM，不含UART结果帧或采集适配，所以尚不能用现有USB工具验收B执行，也不是可部署镜像。后续USB适配需有界发送与机器解析，不能原样复用官方无限TXE轮询；将继续评估共享Console／Journal用于实际多步实验，先模拟操作／取消／恢复。当前仅离线准备，没有启用协作设备窗口。

`Build-Ota-Slot-Probe.ps1 -Jobs 4`最终`20261108-185741-039`通过：BIN1188B／SHA`4b69c59e98a491fbfeb6b8f9e30bc639932b516a095295ea341707a17a71d713`，15份项目输入、7份产物及完整ZIP读回通过。SDK大小表与BIN跨度不同，以实际BIN字节计入擦除账单。检查器核1KiB完整向量表、代码／marker地址、低RAM固定区域和无HAL／C运行库依赖；不生成FTAB、bootloader或下载清单。首版SDK链接参数将入口默认为main，虽向量正确仍可能绕过Reset；已显式绑定Reset_Handler、用readelf与向量交叉检查并移除孤立init/fini片段。相关检查器Python12/12（含原boot9项＋新入口3项），备份检查8/8。

### FTAB长度绑定与恢复依据

锁定SDK从实际bootloader BIN长度生成FTAB镜像头，参见[生成代码](../vendor/SiFli-SDK/tools/build/sdk_resource.py#L1648)；官方RAM启动按[img_hdr长度搬运](../vendor/SiFli-SDK/example/boot_loader/project/butterflmicro/board/main.c#L166)。已核主机Product产物的boot头长度58496B，boot执行地址`0x20020000`，新诊断boot31436B。设备表尚未读回：不能据“新boot小于64KiB”断言可保留旧FTAB直接替换；较大镜像可能截断，较小镜像会多搬尾部。保留旧表／制作精确兼容长度镜像／修改FTAB的取舍，须依据设备表、flags及ROM实测单独评审。此时不预先授权写FTAB。

`ota_backup_review.py`只读两份显式16MiB文件，要求路径不同且全部字节一致；识别锁定SDK的普通未加密FTAB、running指针、boot精确长度／RAM向量／PC后至少2B的取指边界，以及当前8MiB main声明长度未跨入候选B。输出全片及连续覆盖所有区域的SHA。未知magic／指针／布局、加密或异常flags拒绝，报告排他创建，不覆盖备份；任何成功报告仍为`deployment_authorized=false`、`rom_rescue_verified=false`。相同文件只证明字节匹配，不证明真实独立读取、设备身份或坏boot救援。

8项主机测试包含损坏／短文件、指针／flags、main跨B、向量及两份备份不一致的拒绝；只读审查发现PC距声明尾部仅1B会被误接受，已加完整2B边界及反例。另以已逐哈希核过的Product FTAB／boot／main在内存拼接主机fixture，解析长度和向量与生成表一致；这个fixture没有保存成设备备份，也不含设备上的设置／日志证据。

### 建议先做的零Flash擦写窗口（待现场条件与授权）

先核当前固件连续性、完整归档、端点及供电，使用已有runtime导出／device_snapshot保全前态；运行期RAM尾段即使导出到电脑，也不能自动注回原固件。随后一个sftool进程可按公开多文件语法读取两个完整NOR文件：`read_flash <first>@0x12000000:0x1000000 <second>@0x12000000:0x1000000`，使用SF32LB52／nor／500000波特、有限连接次数，结束后回原程序。按无压缩8N1线速，仅32MiB数据约11.2分钟，需预留额外协议时间；不是无需复位的应用层只读查询。

本窗口明确不调用write_flash／erase，不改boot／B／FTAB。ROM连接与RAM stub会中断原应用，可能丢失未持久化的RAM状态、改变RTC及启动计数；校时或恢复写入仍须对应授权。串口与sftool须独占同一端点，保留原始日志和两份文件；失败保留现场，不盲重试。复用既有PortLease／Journal及绑定核验，不绕过端点与清理守卫；实际执行适配尚待按确认的当前版本绑定。

两份一致后离线核FTAB与分区摘要，再确认原程序、显示／触摸及保存末态。正常旧boot仍在时能连ROM，不等于坏boot状态的强制下载入口已经验证；具体黄山派V1.1启动／复位操作还需可复现证据。上述缺口未齐前不进入boot/B写入实验，阶段分支保持；[准备证据](evidence/2026-11-08/ota-mapping-experiment-preparation.json)保存本轮版本和边界，唯一活动项回STATUS。

### 11/08运行记录先行保全

用户确认11/03后未刷机并要求保全有效记录；在新鲜USB／接线固定／人在旁／显示触摸及常亮条件下，已用11/03完整构建归档绑定现设备完成runtime只读导出及CycleLog状态查询。两会话均关闭，原始UART／CRC重解码一致，累计与saved摘要、最近64项／8事件、未持久尾段1732280ms已存电脑；无重启、保存控制、校时或Flash工具。boot由14增至23，用户补充观察时间重置、怀疑触摸／接触相关掉电，根因仍未知；不把历史累计时长当连续运行或用本次查询定位供电故障。

用户确认临时常亮已关、屏触时间正常；[本轮证据](evidence/2026-11-08/runtime-preservation.json)和[硬件记录](HARDWARE.md#2026-11-08-长期待机后的运行记录保全)保存事实。此步骤只完成应用记录复制，尚未进入ROM备份。后续ROM窗口仍须独占／超时／停止与恢复适配、事前模拟及具体授权，执行前再取得新鲜前态，不能复用已过时快照作为复位放行。

<a id="rom-backup-adapter-20261108"></a>
### 11/08 ROM双备份适配与模拟（首版检查点3f4e266）

`scripts/ota_rom_backup.py simulate|capture`复用Console／Journal、PortLease、原runtime快照协议和缓存查询。主适配从应用前态、UART关闭、ROM读取到应用后态连续持有同一端点租约；每次UART打开和ROM启动前重新核工具、原构建完整归档、SDK锁及USB／驱动身份。`runtime_export.request_snapshot`仅抽出原有固定查询顺序和deadline，原独立导出入口行为保持。真实入口要求原固件归档及完整BIN、本轮同板／供电确认与显式`--allow-rom-read-reset`；参数和网页不能代替当次聊天授权。

六步流程为准备确认→新鲜应用前态→自动ROM双读→原程序返回确认→应用后态→关闭常亮及末态确认。前后各最多60秒，只读runtime完整CRC快照、日期、steps／pm缓存及CycleLog状态；记录必须运行、有saved且存储无失败，CycleLog必须无活动／待保存项。前态UART成功关闭且仍在120秒内才可启动ROM。前后原始UART、解码、缓存与事件分别保存，不沿用复位前响应缓冲。读取只生成固定SF32LB52／nor／500000、3次有限连接、`default_reset`／`soft_reset`的单次sftool0.2.5调用，参数仅含两个完整16MiB `read_flash`目标，无自定义stub或写擦入口。

ROM阶段沿用协作页900秒上限，不延长通用Console；32MiB无压缩8N1线速下限约671秒，超时即失败保留，不补第二进程或盲目重试。Windows使用STARTUPINFOEX的JOB_LIST在创建时加入非继承、关闭即终止的Job，明确传递日志／空stdin句柄；挂起创建后才恢复执行。API依据见[微软进程创建属性](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute)及[Job语义](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)。取消／超时先终止并确认本次进程树退出，再释放租约；构造期清理失败也保留所有者并报告资源不确定。失败不由适配器追加RTS、复位、重连、校时或Flash恢复；sftool自身失败时的复位行为仍须实机记录。外部不遵守租约的串口软件仍可能竞争端点。

退出0后人工确认原程序可用才重开UART，要求恰好一个新增应用boot、已验证saved代次及累计／场景时长不倒退、RTC相对主机经过时间偏差不超过60秒；偏差则失败交接，不能自动校时。首版把双NOR核验放在后态之后；下述实机遇RTC变化后，独立核双份并修正后续工具：退出0即分别记录逐字节／FTAB／区域SHA结果，继续取得恢复证据，两个结论互不掩盖。所有不完整文件原样保留。机器恢复只覆盖这些运行记录与时间条件；显示／触摸／KEY1及常亮退出另由人工确认，设置及所有历史文件没有逐项恢复证明。`hardware_verified`／`deployment_authorized`／`rom_rescue_verified`仍为false，具体设备结论由证据审阅判断。

首版相关Python61/61通过，其中适配／原生进程16项，含端点变化、未保全／异常启动、取消／超时、单次进程、不一致双份、RTC跳变／保存回退、关闭及构造期清理失败；Windows真实主机子进程测试覆盖正常退出、带空格／中文目录、子孙终止及父进程被强杀。测试没有打开设备。Edge模拟实际点击完成六步，成功页显示4/4；读取阶段确认按钮禁用，取消页显示未完成及资源关闭。首轮发现Console终态误传对象导致`checks.every`异常，已改为验证列表并重演，失败日志不覆盖。首次静态审查发现ctypes结构基类及创建后再加入Job的清理空档，均修正后才运行原生测试。

以上为设备窗口之前的主机软件证据，保存于`3f4e266`，未重编boot／loader／Product，SDK锁与分区保持；该检查点当时未打开COM或复位。完整命令、源码／日志哈希与模拟路径见[适配证据](evidence/2026-11-08/ota-rom-backup-adapter.json)。随后实机与后续工具修正如下，活动及唯一下一步回STATUS。

<a id="rom-backup-device-20261108"></a>
### 11/08 ROM双备份与独立校时恢复（实机）

用户确认同固件、USB／电池接线固定、人在旁且屏触正常，明确授权本次ROM复位与双备份；协作页准备动作确认后执行`3f4e266`工具。仍绑定11/03 CycleLog完整归档及COM5／CH340位置1-1.1，禁Modem握手复核。前态boot23／generation159、saved累计752131934ms、当前753343039ms、保存失败0／活动fault0、CycleLog无活动；未持久尾段1211105ms（20:11.105）及原始UART已保全。用户在读取阶段报告按键不能唤醒，此时原应用已暂停；ROM期间观察不作恢复后KEY1故障结论。

一个sftool进程用时709.366秒（11分49.366秒）／退出0，无追加进程或Flash写擦命令。两份各16777216B，逐字节一致，整片SHA256=`9140bb4e1bb700c8a7b1d06f29f510c3f18b13fb6da86e34b7f453fec639ab74`。离线FTAB识别boot长度58496／flags2、执行`0x20020000`；main长度3055760／flags2、8MiB原声明且未跨入候选B。boot／main／FTAB各自实际长度的字节哈希均与11/03归档一致，补足此次固件字节连续性证据，不冒充芯片唯一身份认证。

用户在页面确认原程序返回后，后态boot24／saved代次160、saved累计752133024ms，保存失败0／活动fault0；持久记录未倒退。RTC却回到`2000-01-01 00:00:00 UTC`，原流程按约定报错、关闭全部资源，没有自动校时；原`backup.json`的失败及当时尚未执行的pair判定保持不改，双份由独立`offline-pair-review.json`补验通过。用户随后明确授权校时，复用`device_snapshot startup`仅单发`wf_time 1794146052`并读回`2026-11-08 13:54:12 UTC`（北京时间21:54:12），无第二次复位或Flash操作。末次runtime只读导出CRC重解码一致：boot24／generation160／flags7／累计752348299ms／保存失败0／活动fault0；用户最终确认显示、触摸、KEY1唤醒及时间正常，临时常亮已关。RTC在本次ROM窗口中重置的事实不解释此前14→23的未知重启，也不定位重置具体发生于ROM进入、stub还是退出。

收尾发现原适配在runtime解码后继续向同一UART文件追加缓存查询，`records.capture_sha256`因而只覆盖runtime前缀。已按Journal的response_span定位前缀，两份前缀CRC重解码与原JSON完全一致，整段UART重解码的所有记录内容也一致，仅capture哈希不同；新增`before/after-full-records.json`，不覆盖原件。后续工具明确另存`*-runtime-prefix.json`，串口关闭后重解码整段并生成最终`*-records.json`；同时独立保存双份核验结果，RTC失败也不再掩盖已验证备份。新哈希反例在旧版复现失败，修正后相关Python62/62（ROM适配／原生进程17项），本次实机仍以`3f4e266`执行源保存，不用新代码回填原流程为通过。

完整双备份、原始失败、前后记录、单次校时、末次导出及精确执行源共57文件封装于本机`artifacts/ota-rom-backup-20261108.zip`，4823500B，逐成员读回与ZIP检查通过；SHA256=`26a8f156f05fd0ee0ffe5879b05f2658c4189108ca867d5b2189fe3b4017d67d`。[精简证据](evidence/2026-11-08/ota-rom-backup-device.json)区分原流程失败、独立备份通过、单独校时及人工末态。

**后续建议，未作为部署决定**：保持原FTAB时，可评估将当前31436B新boot前缀原样保留并增加27060B确定尾填充，形成精确58496B候选；相应RAM搬运区间为`[0x20020000,0x2002e480)`，在候选64KiB代码RAM内。SDK公开boot按头长度搬运，不是芯片掩膜ROM实现的直接证明；本次正常旧boot下读Flash也没有验证坏boot强制入口。尚需USB有界报告、固定镜像白名单、可返回的执行路径、真实B映射及复位域证据；当前未制作兼容部署包，不写boot／B／FTAB，正式分区保持候选。

<a id="execution-review-20261109"></a>
## 2026-11-09 USB观测与固定探针执行软件评审包

本闭环从`f256e7b`接续，关闭边界为两份可审阅实验镜像、完整固定镜像准入、电脑可解析结果、实读FTAB长度兼容及原区域恢复文件。默认boot继续诊断后停止；显式`-ProbeBinding`才编入固定B探针执行路径。新模式没有生产升级、签名／试运行journal或自动回滚入口，探针发送结果后停住，恢复原Product需明确的有线恢复流程。本轮未枚举／打开端点、复位或写入设备；手机传输和完整OTA仍未接通。

`apps/ota_probe_support`独立持有纯帧协议、整份镜像比较与UART适配，仅由boot／slot两个实验工程显式VariantDir引用，普通Product的core通配构建不纳入这些源。白名单是编入RAM boot的完整探针BIN；清alias、缓存关闭且禁IRQ后，每次最多256B读取固定B地址，逐字节比较所有镜像字节，前后核同一MPI2／16MiB／AES关闭／无alias，短读或差异即拒绝。结构计划另限制探针SP=`0x20002000`、PC在A链接镜像内，应用B→A映射后再读向量和marker匹配；专用jump清SysTick／外部IRQ并保持PRIMASK=1。通用jump仍保留且不被默认诊断或探针入口调用。固定字节白名单仅用于这份最小实验镜像，主机SHA为归档标识，生产认证仍保留独立门槛。

原CH340对应的调试发送为USART1／PA19，固定1M／8N1、仅TX；slot继承boot建立的UART，不调用HAL／SystemInit。初始化按锁定SDK52路径配置CR1/CR2/CR3、UE后设置BRR=48及采样位。52没有通用USART1门控枚举，已按官方条件编译路径省略；首次引用该枚举的编译失败保留。帧固定80B、小端、schema1，带128位本包tag、阶段、真实PC／SP／VTOR／CCR／PRIMASK、alias三元组、marker及IEEE CRC32。PREPARED→MAPPED→EXECUTED必须完整且有序；接收器允许非帧UART噪声，拒绝坏CRC／tag、缺失／重复／乱序／错误阶段、上下文或映射不符。发送每帧总计最多200万次flag读取，含最终TC，预算耗尽停止，不依赖中断tick、不重发或自动复位；这是软件工作量上界，实际波特和耗时仍待设备确认。

`scripts/Prepare-Ota-Probe.ps1`只做离线备份复核→构建带tag探针→冻结产物并生成字面白名单→构建可选执行boot→兼容填充／恢复范围／归档。`ota_probe_package.py`复核原构建源与7份产物、冻结副本／result哈希、真实ELF向量与LOAD末端、嵌入完整探针及tag；所有ALLOC段仍受RAM范围检查，切换36条／专用jump40条指令无调用或PC常量依赖。boot和slot的ELF入口均显式指向Reset_Handler，交叉匹配BIN向量；首版boot ELF原入口指向C entry的问题已修正，默认诊断BIN字节仍为原`24f11089…`。默认诊断复构建通过，`probe_execution_bound=false`，执行函数不链接进该模式。

最终包`artifacts/ota-execution-probe-20261108-006`：slot1684B／SHA`d46f4a7d…`，RAM boot34240B／`5c45fe8f…`。在不改FTAB的候选方案下，将该boot精确填充到58496B，前34240B不变、其余24256B为FF；B候选文件填充到4KiB。评审范围仅boot存储`0x12010000..0x12020000`与B首扇区`0x12420000..0x12421000`；按4KiB几何，候选文件名义擦除量分别61440B／4096B，传输工具实际擦除仍待执行前审查。原boot64KiB与B首4KiB直接从已一致的双NOR提取并封存SHA，可精确覆盖恢复；FTAB、A、loader、FS和KVDB均不在提议写入范围。包不含下载脚本或设备命令，`deployable/deployment_authorized/rom_rescue_verified=false`是当前状态声明。

全量Debug96/96、相关保留断言Release5/5及Python68/68通过。发送模型覆盖TXE永不就绪／TC不完成／总预算耗尽，白名单逐一拒绝4096个单字节破坏；C生成的真实80B帧与Python逐字节匹配，解析器覆盖协议及上下文反例，恢复切片与兼容填充均有主机检查。最终ZIP76成员逐项读回，476811B／SHA`62c81356…`。首轮SCons宏tuple格式、52时钟枚举及RAM常量nm类型分类失败留在001–003；004先通过，005将实验协议隔离到专用支持目录，006补齐构建SDK提交绑定反例后最终封存。默认诊断产物也独立冻结，避免后续复构建覆盖证据。[精简证据](evidence/2026-11-09/ota-probe-execution-package.json)保存准确源码／产物／命令与失败。

**设备准入仍未成立**：目前只有正常旧boot下ROM双读证据。写入新boot前还需可绕过故障Flash boot的强制ROM入口、复位／无其他Flash使用者条件、原区域有线恢复步骤与新鲜现场确认；不能从编译、模拟帧或字面白名单推导这些硬件结论。本软件包形成后，活动与唯一下一步仍回STATUS，不自动启用Flash写入。

2026-11-10交付边界复核：上述固定探针构建包、解析器及恢复输入作为独立软件成果交付；默认诊断不跳转，实验入口与普通Product隔离。隔离交付树Python26/26（无跳过）、相关CTest5/5通过，20份既有构建源SHA与交付树一致，固定boot／slot及默认诊断的冻结ELF／BIN再核通过；[交付证据](evidence/2026-11-10/ota-probe-delivery.json)。后续ROM／stub工具和B执行是分别验收的变更，不作为这份软件合入条件。上段设备准入为11/09本包形成时的历史边界，后续现场事实见本专题对应窗口，当前动作仅回STATUS。

<a id="rom-rescue-plan-20261109"></a>
## 2026-11-09 强制ROM入口核查与零Flash写入演练契约

本设计检查点从`07a96af`接续，只核入口机制、证据判据及恢复边界，形成可评审的演练设计。当时没有枚举／打开COM、运行sftool、复位或写入设备；设备末态仍引用11/08已恢复的原CycleLog固件。下列流程在本检查点尚待实现、模拟及现场授权；后续主机实现见[真实时钟适配](#rom-rescue-device-20261109)，现场结论仍待取得。现有`ota_rom_backup.py`不能替代该演练。

### 已核依据与推断边界

- 官方[52串口自动下载设计](https://wiki.sifli.com/hardware/SF32-auto-download.html)说明52通过外部负载开关切芯片电源复位，上电后约2秒可用UART debug接管ROM；[烧录FAQ4](https://docs.sifli.com/projects/solution/7.FAQ/production_line/burn.html)对52给出Sifli_Trace的52X_BOOT流程。56／58的BOOT_MODE管脚方案不直接套到本板，KEY2与ROM入口没有本板证据。黄山派V1.1的具体电源域／负载开关拓扑仍不能由其他版本图纸推定。
- 读取官方sftool **0.2.5标签固定提交`be2adab37bfb0ef4eb623b7e47e7b8693918b1e1`**，不是master。其[52实现](https://github.com/OpenSiFli/sftool/blob/be2adab37bfb0ef4eb623b7e47e7b8693918b1e1/sftool-lib/src/sf32lb52/mod.rs#L91)先RTS true100ms／false100ms，再UART Enter；随后设置DEMCR的复位捕获、写AIRCR请求复位、重新Enter、halt、清捕获，再下载并启动RAM stub。电脑端这条连接／装载路径不调用原Flash boot；这支持救援路线，但源码不证明本板已经在故障boot条件下接管成功。
- `--before no_reset`仍会连接并下载stub；`no_reset_no_sync`才跳过装载，[定义](https://github.com/OpenSiFli/sftool/blob/be2adab37bfb0ef4eb623b7e47e7b8693918b1e1/sftool-lib/src/lib.rs#L83)如此。后者不是公开的ROM寄存器观测命令，也不能当成“绝无复位”的保证。原备份调用的`--connect-attempts 3`在52循环中最多执行4次RTS／Enter，末次结果不再接受；此前“有限连接”事实保持，不能据参数值声称只可能3次复位。新演练应限制为一次入口尝试，不沿用该循环。
- 锁定SDK的[mem_map.h](../vendor/SiFli-SDK/drivers/cmsis/sf32lb52x/mem_map.h#L18)定义HCPU ROM为`[0x00000000,0x00010000)`，并明确bootloader ROM从0开始。没有取得本芯片修订的maskROM Reset_Handler固定地址，应现场读取ROM向量并核PC，不能拿应用的Reset_Handler符号代替。`RESERVE0`是Flash boot／待机恢复钩子；BMR读取、RTC OTA failed状态及PMU reboot均没有足够依据被当作强制maskROM下载开关。
- Arm的[复位捕获说明](https://arm-software.github.io/CMSIS_5/5.8.0/Pack/html/dbg_debug_sqns.html#dbg_debug_sqns_reset)支持在复位入口截停CPU，也要求适配具体设备。sftool在复位后又发halt，因此单独复现其成功日志不足以区分“复位捕获”与“稍后停住”。演练须在复位后的首次显式halt之前保全捕获状态和PC；否则只能保留较弱的位置观测。
- SDK [HAL_RCC_Reset_and_Halt_LCPU](../vendor/SiFli-SDK/drivers/hal/bf0_hal_rcc.c#L1686)显式处理LCPU CPUWAIT及复位；[PMU reboot](../vendor/SiFli-SDK/drivers/hal/bf0_hal_pmu.c#L605)还关闭LDO2处理NOR寻址状态。这些操作与HCPU SYSRESETREQ不等价。HCPU停止不能证明LCPU／DMA或其他Flash使用者静止，本轮不关闭该写入准入项。

### 演练设计：先证明截停与退出，不改变Flash内容

复用现有Console／Journal、连续PortLease、完整构建／工具及端点绑定、`ota_rom_backup.application`前后快照逻辑；新增专用UART debug适配，不把原应用文本白名单放宽为任意二进制命令。默认只提供不枚举COM的模拟；真实入口须等协议／清理审查、故障模拟和网页操作均通过后再开放。相对时限使用主机单调时钟；原始收发、每次控制线／寄存器写入意图与实际完成分开记录。

| 步骤 | 拟定动作与边界 | 接受证据／失败处理 |
|---|---|---|
| 前态 | 当场核同板同固件、固定USB／电池、人在旁、屏触及临时常亮；最多60秒保全runtime／RTC／缓存与CycleLog，确认无活动及待保存项并关口 | 有saved、存储无失败、CRC完整，RAM尾段另存电脑；距关口超过120秒或绑定改变即不复位 |
| 入口 | 固定1M／8N1，按官方52节奏只做一次RTS入口脉冲并发Enter；从RTS释放起1900ms内取得完整Enter响应，否则不继续捕获、不重试 | 原始响应及单调时间线；约2秒是官方描述，1900ms是保守的本实验预算，不是芯片时限保证 |
| 捕获 | 先使能调试并确认HCPU halted；保存调试寄存器、清既有捕获状态并读清旧reset标志，再设置VC_CORERESET，仅一次SYSRESETREQ，重新Enter | 先读DHCSR／DFSR／DEMCR、VTOR、ROM首两字及PC／SP，**不先补halt来制造成功**；AIRCR响应丢失只保留单发意图，以后续读回判断，绝不重发 |
| 停留 | 捕获成立后，在主机计时不少于2500ms的两端重复读状态、PC／SP及向量 | 必须有新的复位／vector-catch证据、S_HALT及debug使能，PC（去Thumb位）对应现场ROM复位向量且位于64KiB ROM，向量／VTOR相符且两端保持；只落在ROM范围或后补halt均不得通过强判据 |
| 退出 | 正常路径仅清本次捕获并读回，再关闭halting debug、Exit并关口，等待原程序；具体写值／顺序在实现与模拟中审查 | 不改PC／SP，不从被改的寄存器值推定恢复。若未恢复或通信失败，停止实验；仅当本轮已授权包含该恢复分支，才允许一次退出RTS脉冲，无连接重试／追加SYSRESETREQ；否则关口保留现场并请求该具体动作 |
| 后态 | 人工确认原程序已返回后重核绑定，最多60秒读取全新runtime与RTC；人工确认显示／触摸／KEY1并关闭临时常亮 | 预期仅一个新增应用boot，saved代次及已保存时长不倒退、保存失败0；RTC变化独立报告，校时仍需单独授权，不自动修复或用后态失败掩盖入口观测 |

拟定ROM自动阶段总上限30秒，命令响应最多1秒、核心寄存器传输有总deadline，不按收到噪声延长；整个协作窗口不超过5分钟，等待人工动作每步最多120秒，各步均取自身上限与全局剩余时间的较小值，不按步骤相加延长总窗。一次入口RTS、一次内部SYSRESETREQ及最多一次有条件的退出RTS是不同动作，授权说明须分别列出；取消、短写、坏帧、失联或超时保留原始流及未恢复状态，停止新增实验动作，只执行已授权且可确认归属的退出步骤。关闭主机串口不等于CPU已恢复。

UART debug操作拟严格白名单：Enter／Exit；只读固定ROM向量及HCPU调试／SCB寄存器；写入仅DHCSR、DEMCR、DFSR清状态、AIRCR单发SYSRESETREQ、DCRSR的PC／SP**读选择**，拒绝DCRDR写、任意RAM／Flash写、Flash控制器／OTP／RTC／BMR／RESERVE0写及任意stub。读取PC也需要向DCRSR写选择器，故应称“零Flash写入”，不称“所有寄存器只读”。协议帧、地址／值和状态顺序在实现中固化；现有官方解析器没有完整校验通道／尾字节，也没有给本演练所需的收包长度／整帧deadline保证，不能原样当成新守卫。

**结论边界**：若上述强判据通过，可记录“本次HCPU在ROM复位入口被截停并退出恢复”，作为绕过Flash boot的现场证据；本次不破坏Flash boot，不声称真实坏boot、LCPU／DMA静止、NOR擦写或恢复写入已验证。若仅UART连接成功、PC快照或原程序返回，分别记录这些较弱事实，不能自动设置`rom_rescue_verified=true`。

### 原区域有线恢复的评审顺序（后续写入窗口专用）

已有006评审包封存原boot64KiB与B首4KiB；恢复前必须重新核这两个文件与双NOR的对应切片及精确哈希，独占ROM／RAM stub并证明所有Flash使用者受控，再核sftool实际擦除粒度和可用的逐区域读回。未来写入实验若获授权，应先写／读回B探针，再最后替换boot，避免半完成B触发执行；恢复则先还原boot并读回，再还原B并读回，全部一致后才退出RAM工具启动原程序。传输失败不启动应用、不追加重试或擦全片，FTAB／A／loader／FS／KVDB不在范围。当前包有恢复文件，但尚无获准的恢复写入入口或经过审查的设备命令；演练成功也不能自动放行这些写入。

官方固定版本源码与本机sftool哈希封存在本机`artifacts/ota-rom-rescue-research-20261109/`；[精简研究证据](evidence/2026-11-09/ota-rom-rescue-plan.json)保存来源、已知事实、设计与未闭合项。本轮仅验证文档／来源快照，没有新增固件构建或硬件结论，阶段分支保持。

<a id="rom-rescue-simulation-20261109"></a>
## 2026-11-09 ROM观测状态机与协作模拟

本闭环从`bedcd1a`接续，实现上节演练的主机模型并完成故障与页面验收。`ota_rom_rescue_lab.py`**仅开放simulate／status**；没有COM参数、端点枚举、真实串口、复位或Flash入口。模型使用显式逻辑协议时钟；2500ms保持和30秒ROM预算是模拟条件，不是设备时序证据。应用前后态复用原runtime／RTC及CycleLog查询、CRC解码与恢复判定，输入是显式模拟fixture；整个六步Console／Journal会话持有同一模拟端点PortLease，业务步骤共享300秒单调时钟预算。

### 协议依据补足及实现边界

本轮补核发现，项目**锁定SDK内已有**[sifli_uart_dap.py](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L39)，可作为优先依据，不需升级SDK。它定义52 UART Debug IP的ROM逻辑地址映射到A区、核心调试E区映射到F区；[响应解析](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L265)检查`10 00` marker与末尾`06`。末字节是terminator，不是CRC；[核心寄存器读序](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L194)使用SP=13、PC=15并等待S_REGRDY。sftool0.2.5的identity映射与此不同，本工具按SDK主机DAP路径实现，不混用两者；此前双备份结果仍保持原工具和证据边界。额外远端官方新版本只作交叉参考，协议实现锚定本地锁定文件。

`ota_rom_protocol.py`只允许ROM首两字及固定调试寄存器，根本不映射Flash地址。写守卫限定一次DHCSR halt、一次DFSR旧状态清除、一次本次DEMCR捕获、一次SYSRESETREQ及PC／SP读选择器；I/O前消耗次数，短写或复位响应丢失不重发。禁止DCRDR写、核心寄存器写位及任意RAM／Flash／RTC／OTP写。收包最多512B，单个响应有固定总期限，检查marker、预期响应种类、单字读长度与terminator，拒绝额外帧及待发送前的孤立字节。Enter／写ACK精确2B是工具的保守准入规则，官方通用实现并未保证所有硬件版本只返回2B；未知形状须保留原始字节并停止，不能由模拟冻结真实响应形状。Exit只记录已发送，不声称得到回执。

`ota_rom_rescue.py`先读取并拒绝已有调试所有者／捕获，清旧sticky后再单发复位；复位后**没有补halt**，首次读取即须有新reset／VCATCH／halt证据，PC与ROM复位向量、SP与ROM栈向量及VTOR=0相符，逻辑等待后再次一致才接受模型观测。核心寄存器传输的100ms总期限传入读操作，不能以循环内单条1秒请求延长。取消／错误后仅清理已取得的本次调试状态；退出失败最多一次已允许的模拟RTS，不重连／继续实验，关闭失败与控制线释放不确定分别保存。观测、调试退出、应用恢复和主机资源状态独立报告；所有真机／部署标志固定为false。

### 主机验收与保存

首轮20项通过；检查进一步发现peer创建应置于raw文件成功打开之后，且Link构造异常也要关闭已取得peer，已调整并加入构造失败及自动阶段实参超时两项回归。最终新增22项，连同现有相关OTA Python为76/76通过；既有SDK Python环境执行26.376秒、退出0。覆盖每处分片、坏marker／terminator／长度、额外帧、越权地址／值／次数、PC／VTOR错误、旧捕获／缺新reset、寄存器不就绪、短写、复位响应丢失、取消、退出RTS上限、关闭失败、预算与异常资源边界。两个只读复审均无剩余阻断；没有固件源变更，不新增固件构建或C设备结论。

Edge实际点击完成准备→自动ROM→返回→后态→末态，成功页4/4；另一会话在自动阶段验证确认按钮不可用并点击取消，结果为cancelled、调试退出／主机关闭通过，模型观测未完成，未伪造应用后态。初次归档检查检出页面运行早于最后的RTS预算守卫，保留原结果；用最终源码重演为`ui-success-final`／`ui-cancel-final`，逐一复核两份绑定的全部适配源哈希。两会话进程退出、浏览器任务页关闭；原始UART、Journal、独立结果和截图在本机`artifacts/ota-rom-rescue-20261109/`，完整包与源码哈希见[本轮证据](evidence/2026-11-09/ota-rom-rescue-simulation.json)。协议结束标记与映射已获源码依据，真实ACK形状、ROM捕获及退出仍待现场证明。

本机模拟命令（fixture路径指本轮本机证据，不是其他机器已具备的输入）：

```bash
python scripts/ota_rom_rescue_lab.py simulate --out artifacts/<new-run> --before-fixture <explicit-before.bin> --after-fixture <explicit-after.bin>
python scripts/ota_rom_rescue_lab.py status --run artifacts/<existing-run>
```

**本检查点边界**：`3f3d0fc`已完成模型闭环；真实时钟、端点与阻塞I/O适配由下节接续。原固件、完整双备份和006实验包保持，真实其他Flash使用者、擦除范围及有线恢复写入仍未准入，活动只回STATUS。

<a id="rom-rescue-device-20261109"></a>
## 2026-11-09 真实时钟UART适配与假串口验收

从`3f3d0fc`及上一对话中断留下的未提交适配接续。本轮完成`ota_rom_device.py simulate|capture|status`及内部worker，默认模拟；`ota_rom_rescue_lab.py`继续保留纯逻辑模型入口。实机capture具备软件入口，但尚未运行，不能据此写实机救援通过；本轮没有枚举／打开COM、复位、校时或Flash写擦，也没有新增固件构建。

父进程复用Console／Journal，连续持有端点PortLease，仅启动一个受Windows Job管理的worker；worker独占全部UART句柄，顺序完成构建／端点绑定、应用前态、ROM及人工返回确认后的后态。绑定完整Build／Flash结果、原构建ZIP、预期main哈希、原记录根与USB位置；每次开口前重核源码／输入／SDK／端点。前态必须在同一worker内成功、CRC及saved／CycleLog无活动检查通过且单调时间距关口不超过120秒；不接收历史快照替代。应用后态仍要求仅一个新增boot、已存时长不倒退和RTC连续，间隔由同一worker的单调时钟传给既有恢复检查器；保留UTC日志，不自动校时。

`ota_rom_transport.py`固定1M／8N1，先以`port=None`构造并关闭DTR／RTS及硬件流控，再赋端口和打开；读10ms／写50ms超时，省去可能无限等待的`flush()`。控制线及开／关口仍可能卡在驱动，故由父进程期限兜底；模拟不证明真实驱动开／关口的电气边沿。ROM共享状态机以单调时钟执行，活动最多22秒、退出最多8秒，合计不超过30秒；请求1秒总deadline包含预读／发送／响应，迟到ACK保留字节但拒绝接受，核心寄存器传输仍限100ms。捕获后至少保持2.5实际主机秒；本轮对端为假串口，仍不是设备时序证据。

整个窗口最多300秒，前／后态各60秒、绑定30秒、人工每步120秒，均取全局剩余期限；末尾预留8秒协作退出及5秒强制终止／确认。取消先通知worker执行所属退出并关口；驱动阻塞则只终止本次进程树，不另开串口补复位。IPC写入或`poll()`异常也在独立`finally`尝试杀进程／回收；无法确认退出时保留租约。强制停止且ROM响应缺失时入口动作标未知，主机资源关闭与设备恢复分别记录，应用后态不伪造。实机明确要求本次一次入口RTS、一次SYSRESETREQ和异常时最多一次退出RTS授权；没有Flash／RTC／任意RAM写入或重试入口。

最终新增16项测试；`python -X utf8 -m unittest discover -s tests -p 'test_ota*.py' -v`共92/92通过，53.385秒／退出0，包含原生Windows Job正常退出、后代终止及父进程死亡回收测试。覆盖假串口开／读／写／关挂起、取消后退出、关闭不确定保留租约、身份变化拒绝ROM开口、输入变化、后态失败保留观测、迟到响应拒绝和活动超时保留退出时间。初轮14项通过，追加两项deadline回归后按最终版本重跑92项。PATH默认Python3.14.3未装pySerial，仅作模拟；复用现有SDK Python3.13.15／pySerial3.5补跑同一92项，60.403秒／退出0，实机入口使用已有SDK环境，无安装或环境变更。审查发现并修正清理前存储／poll异常跳过close，以及主机实时时钟跳变影响本适配RTC判定两处问题；独立只读复审无剩余阻断。

Edge实际点击七阶段成功流程为4/4；另一会话在自动ROM阶段确认按钮禁用并点击取消，worker退出0、调试释放／RTS释放／端口关闭通过，未运行后态且明确需恢复核验。两会话所绑定适配源码均与最终文件逐哈希相符。取消自动阶段截图、成功页截图、原始流、Journal、进程日志及源码快照保存在本机`artifacts/ota-rom-device-20261109/`，完整ZIP逐成员读回；[精简证据](evidence/2026-11-09/ota-rom-device-adapter.json)记录哈希、命令和未验边界。SDK锁／vendor源／Product／UI与006镜像均未改。

```bash
python -X utf8 scripts/ota_rom_device.py simulate --out artifacts/<new-run> --before-fixture <explicit-before.bin> --after-fixture <explicit-after.bin>
python -X utf8 scripts/ota_rom_device.py status --run artifacts/<existing-run>
```

实机调用需按`capture --help`填写当次绑定和授权，历史命令不构成窗口授权。真实ACK形状、ROM捕获、退出与原程序恢复仍待现场；HCPU halt不证明LCPU／DMA或其他Flash使用者停止，擦除／恢复写入、B执行及自动回滚仍是后续门槛。同一受控映射阶段尚未关闭，继续积累在原分支，活动和唯一下一步回STATUS。

<a id="rom-rescue-device-attempt-20261109"></a>
## 2026-11-09 首次现场入口与独立校时恢复

执行源码为`93664cf`。用户当次确认原板原固件、USB／BAT接线固定、人在旁及屏触正常，授权一次入口RTS、一次内部SYSRESETREQ及异常时最多一次退出RTS，最多5分钟、零Flash写入且不自动校时。主机重核11/03 CycleLog原构建／传输／完整ZIP与main哈希、COM5／CH340位置1-1.1及握手配置；使用原SDK Python3.13.15／pySerial3.5，没有重编或更换板上固件。

`capture-001`的准备确认120秒超时，worker未启动、无串口打开或复位。保留该记录；用户随后确认现在可操作，第二窗改为直接在Edge显示准备页。`capture-002`的prepare／return均由用户点击，全窗62.749秒、退出1。前态CRC及saved／CycleLog门槛通过，工具完成一次入口RTS；紧接着在发送首次Enter前的预读取得5B `53 46 42 4c 0a`（`SFBL\n`），触发`Unsolicited bytes before a new request`。**Enter、调试halt、内部SYSRESETREQ及PC／SP／向量观测均未执行**，没有ROM捕获证据。随后只发送一次清理Exit并关口，RTS释放；没有退出RTS、强制终止或复位重试，worker退出0、父进程及租约关闭。`debug_release_confirmed=false`在本次表示尚未进入可确认的调试阶段，不能解读为已取得调试控制后无法释放。

SDK的[UART示例](../vendor/SiFli-SDK/example/rt_device/uart/README_EN.md)记录复位启动输出以`SFBL`开头；封存官方sftool连接路径是RTS后发送Enter，响应接收器会扫描帧头并跳过非帧字节。本次字节与启动文本相符，但没有证明具体发送源、ROM Enter会成功或原程序已经越过不可挽回的时限。本地发送前守卫无条件拒绝任何待收字节，因而首先阻断了本轮；后续修正应有界处理首次入口前的启动文本并保留原流，不能泛化为允许任意旧响应或重发控制命令。

后态完整CRC读回：应用boot24→25、saved代次166→167、已保存总时长826027717→826028807ms、分场景已存时长均未倒退，保存失败0／活动fault mask0，累计fault仍3，CycleLog无活动。复位前71786ms未持久RAM尾段已复制电脑，没有写回设备；跨复位current总时长下降不等于已存数据倒退。RTC从2026-11-09 UTC变为2000-01-01，使恢复检查失败；当日步数随时间无效暂为invalid，但原持久day／generation490仍在。工具总detail显示RTC失败，ROM失败另存`response-2.json`，两项都保持失败，不能只归为校时问题。

用户确认临时常亮已关、显示／触摸／KEY1正常，并另行授权单次校时及日期／步数回验。复用`device_snapshot.py startup --query date --query steps --seconds 30`及同版完整归档绑定，仅发送一次`wf_time 1794220209`，前后`show_date`和`wf_steps status`回读通过，退出0且关口；北京时间2026-11-09 18:30:09，steps valid1／restored1，原day20766／generation490及小时mask恢复。没有追加复位或Flash写擦命令；校时成功不改写原演练的失败结果，也不构成ROM救援通过。

原始前后UART／CRC记录、5B ROM流、控制意图及完成事件、原始退出1报告、独立应用核验、单次校时回验与截图在本机`artifacts/ota-rom-rescue-device-20261109/`；[精简证据](evidence/2026-11-09/ota-rom-rescue-device-attempt.json)记录版本、哈希、命令和边界。此轮没有源码变更／新增固件构建或主机回归；仅文档与证据检查。SDK锁、原三镜像、11/08完整双备份和006评审包保持。

**接续边界**：本次一个入口RTS已执行，窗口结束。先用本次`SFBL\n`及分片／旧帧反例修正首次Enter前的有界预读，并让ROM失败与RTC失败独立展示，完成假串口及取消／deadline回归；之后另开明确的现场复位窗口，不在本次失败后自动重试。正式OTA boot／B写入仍未放行，活动只回STATUS。

<a id="rom-startup-fix-20261109"></a>
## 2026-11-09 首次Enter启动文本与独立错误展示修正

从`bef1ef9`接续，本轮仅修改主机工具及测试，没有枚举／打开COM、复位、校时或Flash命令，也没有新增固件构建。上节失败及校时恢复记录保持，设备末态仍为boot25和18:30校时后的原CycleLog固件。

`Link`仅在有本次入口RTS释放证据、尚无历史请求且不是清理阶段的**一次首次Enter**前，准许一个完整`SFBL\n`或`SFBL\r\n`，分片间空读可等待但不续期；完整后还须一次空读，所有已收字节先保留到raw。准入长度最多6B；未知文本、重复行、NUL／非ASCII、待收旧D1／D2／D3和部分帧头仍在发送前拒绝，第二次Enter及所有读／写／Exit保持原严格预读。失败也消耗首次例外资格；不引入连接重试或控制命令重发。新增[5B原始fixture](../tests/fixtures/ota-rom-entry-sfbl-20261109.uart)与本次实机原流逐字节一致，沿用`.uart`的binary属性保留换行。

释放deadline从RTS拉低调用前取保守单调时间，仅成功返回后保存释放证据，因此驱动／日志延迟不能推迟释放基准。首次响应须在`min(请求起点+1秒, 释放基准+1.9秒, ROM阶段期限)`内完整结束；预读、日志和发送均计入，实际write前再检查期限及取消。半行最多等到固定deadline；持续噪声不能增长前缀或延长窗口。对发送后才到达的迟延旧响应，UART协议本身没有请求nonce，这一因果歧义仍不能靠本次预读规则消除；首次D1也不是ROM截停证据，仍须后续reset／VCATCH／PC／SP／向量及保持判据。

device报告新增按阶段`errors`，收到ROM失败时立即保留；同一UART已关闭且用户独立确认原程序返回后，仍可采后态并独立判定恢复。ROM与RTC同时失败会各自保留原因，总detail汇总，错误存在时不显示末态通过。取消后迟到的完整ROM结果、未知ROM结果及进程清理错误也分别保存。通用Console只增加可选验证项`detail`的`textContent`显示，既有无detail会话保持原样；页面同时列出ROM观测与退出、应用记录与RTC、主机资源及末态，不能以校时成功抹掉ROM失败。

新增17项（首次入口15、真实时钟worker2）；最终SDK Python3.13.15在同一进程运行相关OTA109项及原Console3项，共**112/112**、68.039秒／退出0。覆盖实测LF及CRLF全分片、片间空读、半行超时、持续噪声、旧帧／部分头、首次例外单次及阶段限制、日志耗时／取消、RTS驱动耗时、释放期限边界、取消后所属清理／可选退出RTS，以及ROM与RTC双失败。首轮53项、补审58项通过；第三次全量运行期间主代理把fixture从`.txt`调整为`.uart`，已加载旧路径的单项报文件缺失，该112项失败日志保留；文件名固定后完整重跑112项通过，没有放宽断言。

Edge实际点击三会话：带`SFBL`的完整成功页4/4；启动文本后混旧帧并叠加RTC异常的失败页分别展示两项原因；自动ROM阶段确认按钮禁用、点击取消后进程／租约关闭且未伪造应用后态。取消与双失败进程退出1均为预期结果，成功退出0；三个会话与浏览器页均关闭。两项只读复审无剩余阻断，源码绑定与原流、测试、页面结果已逐哈希核验。完整证据包在本机`artifacts/ota-rom-startup-fix-20261109/`，版本、命令、失败及哈希见[精简证据](evidence/2026-11-09/ota-rom-startup-fix.json)。

**接续边界**：本软件修正已验，真实首个ACK、ROM捕获及退出仍须新的当次同板／同固件／固定USB与BAT／人在旁条件和一次入口RTS、一次SYSRESETREQ、异常时最多一次退出RTS窗口。前轮授权已执行完毕，不自动追加复位；RTC仍可能重置，校时授权独立。OTA boot／B写入及其他Flash使用者／擦除／恢复门槛保持，同一映射阶段继续保留原分支，活动只回STATUS。

<a id="rom-capture-exit-20261109"></a>
## 2026-11-09 HCPU ROM截停实证与退出回包异常

执行源码为`99a3eca`。用户新确认现场条件并授权本次一次入口RTS／一次SYSRESETREQ／异常时最多一次退出RTS，零Flash写入、总窗最多5分钟。`artifacts/ota-rom-rescue-second-20261109/device-001`准备120秒超时，worker未启动、无开口或复位；浏览器窗口控制也因无法可靠识别URL而停止。用户随后要求继续并确认临时常亮已开，沿用未执行的授权，经新鲜原构建／完整ZIP／传输记录及COM5端点核验后，直接链接协作页启动`device-002`，没有借准备超时追加设备动作。

本窗完成一次入口RTS、初次Enter、一次初始halt、一次SYSRESETREQ及其后重新Enter；总共两个Enter分别属于初连和内部复位后的接管，没有连接重试或reset后补halt。首次前缀`SFBL\n`按新守卫保留并接受，实际D1／D2／D3格式在原始流中取得。HCPU首次DHCSR=`0x02030003`、末次=`0x00030003`，新RESET／HALT／DEBUG与DFSR=8的VCATCH证据成立；两端DEMCR=1、VTOR=0，现场ROM向量PC=`0x7D5`、实际PC=`0x7D4`，SP及向量SP均为`0x20001020`。两端单调时间相隔**2.9621724秒**，满足既定至少2500ms保持判据。原始流、事件及独立只读审查支持“本次HCPU在ROM复位入口被截停并保持”；不把它写成LCPU／DMA静止、真实坏boot或完整救援通过。

退出先将DEMCR从1清为0，并收到D3及D2读回0确认。随后发送一次DHCSR=`0xA05F0000`关闭调试，接收流offset`[429,437)`为完整D3 ACK `7e7902001000d306`，紧随的`[437,442)`为`SFBL\n`。严格解析器因同段额外启动文本报`Extra bytes/frame cannot become a later reply`，并非混入第二个协议帧。关闭调试命令及原始ACK已取得，但没有后续DHCSR读回，Exit尚未发送，故`debug_release_confirmed=false`。清理按当次授权仅做一次退出RTS后释放并关口；没有追加SYSRESETREQ或进一步重连／复位。本次需要区分捕获成功、关闭调试命令已发送、退出确认失败和随后原应用恢复这四项事实。

用户在协作页确认原程序已返回后，完整后态CRC读回：boot25→26、saved代168→169，已存总时长828871111→828872201ms，分场景已存时长未倒退、保存失败0／活动fault0、累计fault仍3，CycleLog无活动。前态343072ms RAM尾段仅保全到电脑，未写回设备。RTC回到2000年，恢复检查独立失败；两项原因均在结果页和JSON中保留。全窗88.886秒／父退出1，worker退出0，无强杀，串口及租约均关闭。HCPU捕获的通过证据不因总窗退出1丢弃，也不把本窗记成整套ROM救援通过。

用户确认临时常亮已关、屏触及KEY1正常，并另行授权本次单次校时和日期／步数回验。第一次只读`show_date`无字节超时，尚未发送`wf_time`且关口，不能记为校时已执行；用户重新点亮并开启临时常亮后，新窗口成功单发`wf_time 1794223437`，前后日期及步数回验通过，北京时间2026-11-09 19:23:57、steps valid1／restored1、原day20766／generation491／小时mask`0x0fffff`。原应用同时记录preferences GC59ms后alarm保存成功，保留原日志，不扩大为存储排障。最终用户确认常亮已关、时间／显示／触摸／KEY1正常，主机会话全关；没有更换固件或发送Flash写擦命令。

本机`artifacts/ota-rom-rescue-second-20261109/`保存准备超时、实际ROM442B原流、控制意图与完成、全部应用前后态／CRC、原始双失败报告、独立观测与应用核验及两次校时入口记录；[精简证据](evidence/2026-11-09/ota-rom-capture-exit.json)记录版本、哈希和剩余边界。两个只读复核分别检查了ROM／退出字节及应用／资源，结论与上述分项一致。此轮没有源码变更／新增固件构建或主机回归，只检查受影响文档与证据；SDK锁、原三镜像、11/08双备份和006评审包保持。

**接续边界**：本次入口与有条件退出RTS均已执行，窗口结束。先核对官方关闭调试／Exit顺序，用本次D3 ACK＋`SFBL\n`尾段及旧帧／分片反例修正退出处理，保留独立状态确认、无重试与清理期限，完成主机模拟后再开明确的新现场窗口。当前HCPU ROM捕获已有实证，正常退出确认仍未闭合；OTA boot／B写入、其他Flash使用者／擦除和有线恢复写入仍未放行，活动只回STATUS。

<a id="rom-release-fix-20261109"></a>
## 2026-11-09 关闭调试ACK与启动尾段主机修正

从`2826ed6`接续，只修改主机适配和回归；未枚举／打开COM、复位、校时或发送Flash写擦命令，没有新增固件构建。上节真实HCPU截停、退出失败及校时恢复证据保持；本轮模拟不改变设备末态，也不自动取得新现场窗口授权。

锁定SDK的[sifli_uart_dap.close](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L118)只发Exit再关口，没有定义“清DEMCR→关闭DHCSR调试→独立读回→Exit”的顺序；[write32](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L171)只核D3种类，[发送及接收](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L231)会清输入缓冲并跳过非帧字节，不能直接替代本实验的原流保留与因果检查。本项目仍保留所属状态清理及独立读回判据；SDK没有证明关闭debug后UART DAP必然不可读，真实可用性仍须新窗口取得。SFBL的目标发送源没有新增独立证据。

新增`DebugReleaseReply`仅用于已拥有调试控制的cleanup、单次`DHCSR=0xA05F0000`精确事务。它从首字节核完整8B D3 ACK，可选紧接一个`SFBL\n`或`SFBL\r\n`，最多14B；分片或半行空读不续期，完整后须一次空读才结束。ACK与尾段各有原流偏移事件，尾段不转作后续响应。首次Enter、其他写／读及所有发送前预读保持原守卫；重复行、旧帧、未知字节和半行超时失败。关闭事务例外资格与写次数在I/O前消耗，不重发。

cleanup分别报告`debug_release_acknowledged`、`debug_release_readback`及`debug_release_confirmed`。完整ACK或SFBL不能单独确认释放；随后单发DHCSR读取，强制帧头严格模式，在响应期也拒绝前导文本；完整合法D2且HALT／DEBUG均清零才confirmed，再发Exit。无读回、状态仍置位或错误仍失败，仅执行当次已授权的最多一次退出RTS，保留原观测并关口。单请求1秒、cleanup最多8秒及ROM总窗30秒保持，剩余清理时间还会进一步截短请求。UART协议没有请求nonce，对发送后迟到且形状完全匹配的旧D2仍不能给出协议层因果保证。

新增13B[真实尾段fixture](../tests/fixtures/ota-rom-release-sfbl-20261109.uart)，与现场442B原流`[429,442)`逐字节一致。新增14项定向及2项worker回归；最终SDK Python3.13.15／pySerial3.5执行`python -X utf8 -m unittest discover -s tests -p 'test_ota*.py' -v`，**125/125通过，79.495秒／退出0**。覆盖全分片、LF／CRLF、无尾段、片间空读、ACK损坏、重复行／旧帧、半行／无响应、迟到ACK／空读、下一预读及读回响应期迟到文本、独立读回失败、取消、单发和剩余250ms清理期限。未更改固件工程、SDK锁或vendor；本变更只影响主机工具，没有对应固件构建目标。

Edge实际点击最终源码三会话：带尾段成功页4/4；DHCSR仍有HALT／DEBUG的反例保留ROM保持成功、ACK已接收但释放失败，页面2/4且应用后态与资源通过；自动ROM阶段确认按钮禁用、取消后worker0／资源关闭且没有伪造后态。独立复审发现普通响应解析会跳过响应期迟到文本，已将独立释放读回收紧并补测；复审最终文件无剩余阻断。首版123项及成功页通过后新增上述收紧；中间一次日志偏移局部编辑误用`Reply.ACK`被定向测试检出并纠正。一次全量进程缺结束记录，保留不完整日志后重跑取得125项最终结果。首批失败／取消页面准备超时、worker未启动，保留原记录并逐个重开验收。

原流、配置／源码绑定、测试日志、页面结果及截图封存在本机`artifacts/ota-rom-release-fix-20261109/`；[精简证据](evidence/2026-11-09/ota-rom-release-fix.json)保存来源、版本、归档哈希及边界。正式映射阶段仍未关闭，不按本轮小修单独创建PR。下一步仅由STATUS指定；新窗应复核同板同固件、固定USB／BAT、人在旁与临时常亮，限一次入口RTS／一次SYSRESETREQ／异常时最多一次退出RTS、总计最多5分钟且不写Flash。RTC仍可能回到2000年，校时继续单独授权。

<a id="rom-release-device-20261109"></a>
## 2026-11-09 独立释放实证、SP期限拒收与分窗接续决定

用户确认现场条件并授权本次演练，执行主机`ab0a479`、板上保持11/03原CycleLog固件。重核最终源码哈希、原构建／完整ZIP／传输记录及COM5位置1-1.1；准备与原程序返回分别由操作者在页面确认。全窗63.6009218秒，父退出1、worker0，无强杀，端口及租约关闭。一共一次入口RTS、一次初始halt、一次SYSRESETREQ、两个分别属于初连和内部复位后的Enter、一次Exit，没有reset后补halt或退出RTS。

ROM318B原流中，新DHCSR=0x02030003／DFSR8／DEMCR1／VTOR0、ROM PC向量0x7D5、实际PC0x7D4及向量SP0x20001020均已取得。首次SP选择DCRSR=13后，ready读回成立；原流`[249,261)`保存完整D2 `7e7906001000d22010002006`，SP=0x20001020正确。但工具把ready与最终DCRDR读取共用100ms硬deadline，在raw保存／flush后检查时已越界，抛出`Response arrived after the transaction deadline`，没有将结果交给parser，因此`observations={}`，本窗首次快照与2.5秒保持未完成。日志没有即时read返回时刻，不能分离设备、驱动、调度和取证开销，也不能把它说成设备返回错误或响应慢。

锁定SDK[_read_core_register](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L194)仅在进入ready轮询时检查100ms；ready成立后DCRDR走[_read_response](../vendor/SiFli-SDK/tools/sdk_py_actions/sifli_uart_dap.py#L241)独立3秒预算，没有最终100ms返回检查。项目额外收紧整段期限的语义并非官方设备约束。工具潜在修正可把ready窗口与单发DCRDR事务预算分开，并记录read即时返回时刻；本轮用户决定优先继续OTA，不把这项主机工具修改或重复整窗设为前置，未改代码／新增测试。

本窗清理取得完整独立证据：DEMCR从1恢复0并读回；DHCSR关闭写后的`[293,301)`为精确D3 ACK，`[301,306)`为`SFBL\n`，专用尾段守卫通过；单独发送DHCSR读取，`[306,318)`为合法D2=0x05000000，HALT／DEBUG均为0，`debug_release_confirmed=true`。Exit单发完整write完成，不要求ACK；随后ROM串口关闭。它补足上一窗的正常释放缺口，不能因SP观测总失败丢弃该分项结论。

应用前后原始CRC独立重解码一致，boot26→27、saved170→171、已存总时长835795459→835796550ms，已存分场景均未倒退、保存失败0／活动fault0、累计fault仍3，CycleLog无活动；193872ms RAM尾段仅保全到电脑。RTC由2026-11-09 UTC变为2000-01-01，使恢复判定独立失败；steps暂invalid，原day20766／generation493／小时mask0x3fffff仍在。原ROM窗口没有校时或Flash写擦命令；后续独立校时如下，原失败保持。

用户指出正确数据可用于继续实验，明确接受不因工具总标志反复演练。**本轮接续决定覆盖“须再完成同窗全通过才继续”的旧下一步**：保留原失败报告，独立采用上一窗2.962秒ROM保持与本窗释放读回／Exit证据，转向其他Flash使用者、精确擦除和原区域有线恢复的离线审查；不把分窗组合改写成单次完整救援通过，不跳过boot／B写入准入与新设备动作授权。

RTC原因按已有[软件reboot与RTS对照](PRODUCT-RUNTIME.md#2026-10-12-rtc-软件重启与-rts-对照)及本轮源码复核判断：Product启动只读RTC，SDK冷启动／备份标记决定NORMAL或SKIP，而`HAL_RTC_Init`不写日期时间；无century标记的年份0解释为2000。首个11/09窗口仅入口RTS、未执行SYSRESETREQ已丢时，内部SYSRESETREQ不是必要条件；RTS关联供电复位路径最符合证据。本板RTC轨及开关实装未测，当前已接BAT，不能套用旧USB-only缺电池解释。该已知有线实验副作用与软件OTA保时、B执行分别验证，不扩成新的供电排障前置。

本机`artifacts/ota-rom-release-device-20261109/`保留原报告、318B ROM流、独立SP／释放及应用CRC核验、执行源与时序；[精简证据](evidence/2026-11-09/ota-rom-release-device.json)保存版本、哈希和用户决定。源码检查、125项旧主机证据、本次设备分项和完整OTA／坏boot恢复的未验范围分开记录；SDK锁、Product／boot／B均未改变，阶段分支保持。

用户随后确认已亮屏，独立授权本次单次校时与日期／步数回验，最多30秒、不复位。复用`device_snapshot.py startup --query date --query steps --seconds 30`及同一原构建／完整ZIP／传输／端点绑定；仅依次发送`show_date`、`wf_time 1794231231`、`show_date`、`wf_steps status`。1.105秒／退出0，回读UTC13:33:51、北京时间2026-11-09 21:33:51，steps valid1／restored1、day20766／generation493／小时mask0x3fffff、verified1／dirty0／failed0，串口关闭。原应用伴随普通alarm保存generation1263，原日志保留；无追加复位或Flash写擦命令，不改变ROM原失败。[独立校时证据](evidence/2026-11-09/ota-rom-release-clock-recovery.json)及本机11成员ZIP已读回一致；后续接续时用户明确确认“已关闭，时间／显示／触摸／KEY1 正常”，补齐本窗人工末态，原采集报告中的待确认字段保留为当时状态。无源码变更／新增构建或设备操作。

### 后续写入范围核查（固定官方工具，只读）

本轮按用户决定转查实际写入影响，版本保持sftool0.2.5／`be2adab37bfb0ef4eb623b7e47e7b8693918b1e1`，没有运行sftool或新增设备动作。主代理抽查固定提交的[写入路径](https://github.com/OpenSiFli/sftool/blob/be2adab37bfb0ef4eb623b7e47e7b8693918b1e1/sftool-lib/src/common/write_flash.rs#L74)与[stub选择](https://github.com/OpenSiFli/sftool/blob/be2adab37bfb0ef4eb623b7e47e7b8693918b1e1/sftool-lib/src/ram_stub.rs#L5)，只读研究结论如下。

- SF52按CLI文件顺序逐个执行，先比较目标原地址／长度的CRC，相同则跳过；不同就将同一地址／长度发给`burn_erase_write`，`--verify`执行CRC校验。没有主机侧地址对齐或邻区备份／恢复，文件之间没有事务回滚。失败经`?`返回，CLI末尾的正常after reset不会执行；连接阶段的RTS重试是独立路径，`--connect-attempts 1`也不能声称只会一次入口。
- [read_flash](https://github.com/OpenSiFli/sftool/blob/be2adab37bfb0ef4eb623b7e47e7b8693918b1e1/sftool-lib/src/common/read_flash.rs#L80)检查传输CRC与末尾OK后生成文件，可作为独立导出后逐字节／SHA核验入口；`--verify`本身不等于取得邻区字节证据。`--after no_reset`及已运行兼容stub下的`--before no_reset_no_sync`可表达保持工具态，但后者不是冷ROM入口，不能直接照搬执行。
- 固定树给出SF52 NOR的132816B `ram_patch_52X.bin`，本轮未取得该bin对应的可审阅C源／构建映射。实际擦除对齐、MPI2选择及LCPU／DMA处理在stub内；主机源中的“4KB sector”是超时估算，不足以证明本次只擦4KiB。主机装载段只显式处理HCPU、BKP0R及PA21，再运行stub；SDK具备LCPU CPUWAIT／复位接口，不代表该stub调用了它。
- 这些是**指定区域外数据可能被改变、其他总线主设备是否访问Flash**的核心写入问题，影响下一物理动作；缺少stub C源本身不等同官方工具不可用，不把完整逆向工具另设为默认目标。后续最小方案应在固定工具／stub身份基础上，选择有边界的来源核验或目标与保护邻区的前后读回等直接证据，明确可接受擦除包络、备份与恢复，不重做已证明的ROM保持／释放。

006已封存原boot64KiB `[0x12010000,0x12020000)`与B首4KiB `[0x12420000,0x12421000)`恢复文件。候选操作顺序仍为B探针先、boot最后；恢复为boot先、B后，每处读回一致才进入下一动作。当前只完成主机路径核查与下一方案所需缺口定位，未批准写擦、扩大擦除范围或运行恢复命令，活动仍只回STATUS。

<a id="write-recovery-review-20261109"></a>
## 2026-11-09 最小写入与恢复命令离线审查

本检查点从`09de899`接续，关闭边界是固定文件复核、工具会话语义及有条件的命令评审表；不关闭受控映射阶段。实际仅运行无端口的`--help`与`write_flash --help`、离线字节／来源检查，没有枚举或打开设备。完整结果见[精简证据](evidence/2026-11-09/ota-write-recovery-review.json)，可复核材料在本机`artifacts/ota-write-recovery-review-20261109/`。

### 固定输入与保护范围

双16MiB备份重新逐字节一致，仍为`9140bb4e…`；006四份候选／恢复文件的长度、SHA及原区域切片均通过。boot兼容文件58496B、B候选4096B保持，原boot末7040B均为FF。**B不是空白区**：原B首4KiB有3895个非FF字节，后续60KiB `[0x12421000,0x12430000)`有53547个非FF字节，下一64KiB也有61170个非FF字节。这些是旧备份中的真实字节，不据此推定它们当前被Product使用；仍须保全，不能以当前main长度未跨B为由擦掉。

候选写入包络仍仅boot64KiB和B首4KiB。若按4KiB向上取整，boot58496B对应61440B擦除长度，余下4096B位于该假设包络外；实际边界仍待闭合。即使以完整64KiB处理boot也没有越出原提议范围。B若向64KiB扩大擦除，会影响提议范围外的53547个非FF字节，原4KiB恢复文件也不足以恢复它们。因此**4KiB与64KiB擦除差异实质影响B操作，须在执行前闭合**，本轮不扩大B写入范围。

固定sftool提交的SF52 NOR stub与锁定SDK `tools/uart_download/file/ram_patch_52X.bin`按Git blob及SHA核对相同：132816B／`d4db9c27…`；Impeller同名stub不同，不能混作证据。固定工具本机哈希仍为`1ccc425e…`。后续可用本机已支持的`--stub`显式指定上述原版bin以消除选择歧义，不修改stub。它与SDK同身份不证明本机EXE可复现构建或所有调用路径已知。

对SDK `rt_nor_erase_rom`的源码及stub字符串作了有界核查，前者有sector对齐及在剩余长度内选择64KiB／sector的实现，但没有该bin的构建映射。现存Product ELF中LCPU复位函数84B、`rt_flash_erase`432B在stub均无完整字节匹配，静态`rt_nor_erase_rom`无独立符号；这既不建立调用证据，也不证明stub缺少同等功能。未继续完整逆向。LCPU／其他DMA及具体擦除包络仍未由本轮离线检查关闭。

### 会话与命令的评审结论

固定[构造入口](https://github.com/OpenSiFli/sftool/blob/be2adab37bfb0ef4eb623b7e47e7b8693918b1e1/sftool-lib/src/sf32lb52/mod.rs#L244)每次以1M打开串口，并主动设置RTS=false；`no_reset_no_sync`只跳过stub装载，不保证串口重开没有线路变化。[速度设置](https://github.com/OpenSiFli/sftool/blob/be2adab37bfb0ef4eb623b7e47e7b8693918b1e1/sftool-lib/src/common/speed.rs#L11)先向stub发送换速命令，再改变电脑波特率。因此不能把原双备份的500000波特直接沿用到跨进程、保留500000状态的stub：新进程先以1M发送命令，双方可能不在同一速度。

推荐后续适配维持**一个受管进程、一个已确认归属的1M stub会话**，逐项写入、独立导出与主机比较。原CLI可表达下表的命令语义，但多进程重开与RTS行为尚未验证；表格不是可直接运行的恢复入口。共同参数为`-c SF32LB52 -m nor -b 1000000 --before no_reset_no_sync --after no_reset --compat false`，仅在已经确认同一原版stub正在1M运行、端点独占及Flash使用者受控时才有意义；冷ROM入口另有复位／装载动作，不能用该参数替代。

| 顺序与条件 | 固定命令后缀／地址 | 主机放行条件 |
|---|---|---|
| 候选B，最先执行 | `write_flash --verify probe-b-sector.bin@0x12420000` | 导出B首4KiB与候选逐字节一致，保护区无变化后才讨论boot |
| 候选boot，最后替换 | `write_flash --verify boot-compatible-58496.bin@0x12010000` | 导出完整boot64KiB，前58496B匹配候选、末7040B匹配原FF；保护区一致 |
| 恢复boot，先于恢复B | `write_flash --verify restore-boot-64k.bin@0x12010000`；`read_flash boot-restored.bin@0x12010000:0x10000` | 完整65536B与原备份切片一致，才进入恢复B |
| 恢复B | `write_flash --verify restore-b-first4k.bin@0x12420000`；`read_flash b-restored.bin@0x12420000:0x1000` | 完整4096B与原备份切片一致，保护区一致，才允许明确的启动动作 |

相应文件全部绑定006精确SHA，命令表不允许替换成目录、其他BIN、`erase-all`或整片恢复。设备输出文件须使用当次新目录，不覆盖旧证据。`--verify`是stub CRC，不替代独立导出与字节比较；固定读路径有传输CRC及末尾OK检查。目标相邻4KiB的短读回可作快速哨兵，**不能独立证明实际擦除只限4KiB**。首次物理边界验证应以同一stub内的新鲜完整NOR前后读回比较所有目标外字节；FS／KVDB等可变区以当次前态为基准，不能拿11/08全片哈希要求运行一天后的设备相同。即使前后未变，也只证明该窗字节保持，不自动证明所有总线主设备始终静止。

固定CLI的正常after动作只在命令成功返回后执行；普通写入失败不会自动走该末尾reset，但串口重开、连接重试、构造异常和stub自身状态不能由此推定安全。失败／取消保留原始流及目标状态，不跳过比较、不自动重试／启动／扩大擦除。恢复后最终启动是单独受控步骤，仍有RTC丢时副作用；不在恢复命令中隐含校时。

### 相关上游报告与尚未闭合项

[issue55](https://github.com/OpenSiFli/sftool/issues/55)报告固定0.2.5在SF32LB52原始数据传输中遇到Debug IP字节组合后不完成，关联[PR56](https://github.com/OpenSiFli/sftool/pull/56)核查时仍开放。这是贡献者的另一设备证据，不能直接当成本板故障。四份固定候选／恢复文件已按完整连续字节扫描`7e79`／`4072`／`4077`，包括内部块边界，全部零命中；该特定已知payload风险不新增为这四份固定文件的阻断，也不据此宣称任意固件传输已解决。未采用PR补丁、压缩或自行编码协议，SDK锁保持。

本次离线审查已给出准确输入、保护字节、恢复顺序及命令条件；执行准入仍缺**同一stub会话中的其他Flash使用者控制与物理擦除包络**。应先形成只覆盖这些缺口的会话／观测适配和模拟，明确原版stub的控制来源、单次入口及异常收尾，再评审最小设备窗口；不重做已证明的HCPU ROM保持／释放，不先替换boot来验证工具。真实写擦、恢复写入、B执行及A/B自动回滚均未新增验证，阶段分支继续保留，活动顺序只回STATUS。

<a id="stub-observer-20261110"></a>
## 2026-11-09至10日 原版stub单会话观测软件与最小现场合同

用户要求以首版A/B自动回滚OTA为目标，持续自主推进到确需协作。从`22f8537`接续，本轮成果是可评审的**零Flash写入观测入口**，为写入方案补充原版stub运行时的LCPU／DMA事实及稳定读回证据；不重复HCPU ROM保持／释放，不把已完成的正确原流再跑成同窗全通过。真实擦除包络、写入／恢复、B执行和完整OTA仍分别保留门槛。[软件证据](evidence/2026-11-10/ota-stub-observer-software.json)记录准确输入、源码、构建、模拟及失败。

### 固定实现与输入

`scripts/ota_stub_bridge`是主机Rust入口，直接调用固定官方`sftool-lib 0.2.5`／`be2adab3…`的SF52装载、调试读及NOR读取API，不实现另一套线协议或自制stub。原CLI config仅能选一种操作，不能表达逐项读回后的主机门槛，因此这里维持同一次串口open及同一个工具实例；原库内部复制句柄仍属该次连接，不重新开端口。固定1M／8N1，显式DTR=false，不换速；原版132816B stub按`d4db9c27…`核验并冻结到本次新目录。Cargo.lock保留原依赖serialport4.7.0；初次新解析因该版本已撤回而拒绝，沿用原锁后构建成功。DTR显式设置不依赖该版本的默认高电平；没有升级SDK、替换stub或声称CH340实际线态已验。

`ota_stub_build.py`只构建主机程序，显式锁定GNU target，执行Rust测试与Release构建，构建前后重核源码，再复制冻结EXE及SHA；不产出固件、下载清单或设备命令。`ota_stub_observe.py simulate|capture|status`复用`ota_rom_device.py`的固定stub-observe profile、Console／Journal、连续PortLease与外层Windows Job；普通ROM profile仍为原300秒契约。capture另须原Product构建／Flash／完整归档／BIN／端点绑定、新鲜现场条件、ROM复位／原版stub观测／返回原程序及有条件退出授权。两个原NOR备份、主机EXE／源码／SDK锁／stub与寄存器头文件均纳入哈希绑定；缺件、源变更、未知参数或任意用户地址在开设备前拒绝。

### 采集与证据强度

入口为一次主动RTS脉冲，随后调用原库NoReset装载路径，只单发一次SYSRESETREQ。该库`attempts=1`在初连失败时仍最多发送两次Enter，第二次结果不再接受；它不新增RTS重试，失败后不重开端口或重跑整窗。装载沿用官方DEMCR／DHCSR／RAM／PC／SP及BKP0R=`0xA640`、PA21设置，运行的仍是原版RAM stub；所以称“零Flash写入”，不称“全部寄存器只读”或“绝无RTC寄存器写入”。

stub ready后及两份NOR读取后各做一次有限快照：`LPSYS_AON.PMR`／CPUWAIT、`LPSYS_RCC.RSTR1`，DMAC1和DMAC2各8通道的CCR／计数／传输端点，以及EXTDMA同类寄存器。地址／位／数量来自锁定SDK头文件；DMA传输方向按CCR解释，UART1与RAM端点只作线索。LP访问失败、缺字、非法形状或未知通道不会补零；CPUWAIT=1与复位位清零可以并存，单次DMA EN=0不证明此前或之后一直静止。**不要求所有DMA全零，也不自动置`all_flash_users_controlled=true`。** 这些快照不能穷尽全部总线主设备或替代真实写擦准入。

读取地址仅固定NOR `[0x12000000,0x13000000)`，分别取得两个16MiB文件；原库核传输CRC及末尾OK。第一份先核原FTAB／boot／A／B首扇区等固定输入，才请求第二份；FS／KVDB可变内容不与11/08旧全片哈希硬比。第二份须与同窗第一份逐字节一致；父worker再独立解析文件及核SHA。寄存器失败、CRC错误或固定区变化会中止后续采集并保留已有流，不开放写擦、任意地址、通用指令、换速、自动重连或校时入口。

原始TX意图、实际write返回的字节及RX分别保存，单调时间／偏移与复制句柄事件另记。日志失败形成共享sticky fault：已经发出的字节仍向BufWriter返回已消费长度，避免析构重发；之后拒绝新的协议I/O和入口高电平，仍允许已授权的清理RTS及释放低电平。取消／总预算在开口及入口前同步检查；正常退出单发`burn_reset`，失败或取消在确认本次入口已尝试且当次授权包含时，最多一次退出RTS。阻塞OS调用由内层watchdog回收Rust子进程，外层Job继续兜底；回收不能证明CPU已返回，仍须操作者实际确认后才读应用后态。损坏／异形结果把入口保留为未知，不伪称未操作设备。

应用保存记录与RTC分项判断：RTC回到2000年时，合法stub观测／NOR数据与保存记录仍可采用，失败独立保留；本入口不自动校时。有限观测或双读成功不会放行B写入，也不把邻区未变改写成擦除包络已验。

### 软件验收与现场协作边界

最终主机包为本机`artifacts/ota-stub-observer-build-20261109-005`，Rust1.90.0／GNU target的4项守卫测试及Release构建通过，源码／冻结EXE哈希一致。OTA相关Python **138/138**通过，包含新13项：CRC、固定区／双份变化、LP访问失败、LCPU仍运行、挂起及回收、取消、坏结果形状、任意地址拒绝和RTC分项；原ROM profile回归保持。浏览器实际点击四条模拟流程：成功5/5，RTC单项失败4/5，寄存器失败3/5，自动阶段取消1/5但进程与租约关闭；确认按钮在自动阶段禁用。失败／取消如实保留，没有伪造物理确认。构建／依赖下载／模拟唤醒空行／测试目录冲突等开发失败留在本机日志；没有新增固件构建、COM枚举、设备操作或硬件结论。

拟议实机窗口总上限15分钟，stub阶段最多600秒，准备／返回／末态每次人工确认最多120秒，前后应用快照各最多60秒；共享总期限及退出余量，不按分段相加延长。1M下双16MiB的8N1纯数据约5分36秒；建议现场预留完整15分钟，实际读取速率仍待观测。必须当场确认原板原CycleLog固件、USB／BAT接线固定、人在旁、显示触摸正常，开启临时常亮供前后态读取；RAM尾段只保全到电脑。此次需要授权的具体动作是：**一次主动入口RTS、一次SYSRESETREQ、固定原版RAM stub装载及上述寄存器观察、双NOR读取、单发burn_reset返回原程序，异常时最多一次退出RTS；零Flash写擦，RTC变化后的校时另行确认。** 旧窗口授权已用完，准备页确认不能替代新的当次授权。

这个窗口的核心问题是“本板原版stub能否保持单次1M会话并取得可归属的状态／稳定读回”，不是重新验证ROM捕获或提前替换boot。取得有效数据后按独立事实审阅，只补影响下一写入动作的最小缺口；不要求为总`passed`重复整窗。当前阶段保留同一分支，实际协作／活动顺序只回STATUS。

<a id="stub-observer-device-20261110"></a>
## 2026-11-10 原版stub实机有限状态、接收缺字及独立收尾

### 当次授权与动作

检查点`dce2748`，用户明确“现场条件满足，授权本次观测”，范围沿用上节，绑定原11/03 CycleLog完整归档、main `10560604…`、原双NOR及主机包`ota-stub-observer-build-20261109-005`。预检COM5／CH340位置1-1.1、disable_modem=1通过。`device-001`准备120秒超时，worker_started=false／entry_attempted=false，无开口或复位；用户答“现在可以，重开准备页”后在`device-002`实际确认。父／worker命令、配置SHA与原始流见[实机证据](evidence/2026-11-10/ota-stub-observer-device.json)，本地两次目录及执行源码另归档，旧失败保持。

实际只一次1M开口对象、DTR=false，一次主动入口RTS及一次SYSRESETREQ，运行同SHA原版RAM stub。ready快照完整：PMR=0x4／CPUWAIT=1，RSTR1=0；DMAC1通道7的CCR@0x50081080=0x10AF、CNDTR=32362、源0x50084024→目的0x2004396C，其他16个采样通道EN=0。该端点为UART1 RDR→RAM配置；不能由此认定DMA已归属stub或LCPU／全部总线主设备持续静止。

### 首份载荷不足与核心证据

唯一`burn_read 0x12000000 0x01000000`请求后，RX offset1578开始载荷，16778642开始完整`CRC:0x6b38d8a5\r\nOK\r\nmsh >`尾文。载荷16777064B，比16MiB少152B；官方同参数CRC为0xCF4AD786，与设备0x6B38D8A5不符。最后16KiB块剩16232B载荷，工具继续把25B尾文纳入数据，仍差127B，最终在0x12FFC000等待10秒超时。这是实际捕获不完整，不是只缺报告或附加时间戳；不能据尾文、放宽期限或旧备份补齐而采纳完整NOR。

1037285条RX事件offset连续，累计16778667B；实际TX与意图134200B完全一致。它们只证明已返回给记录器的字节连续保存，152B丢在设备、驱动或上层读取的哪一层仍未知。捕获前0x421000B与11/08备份逐字节一致；首个差异在0x503D60，后段无法用单个152B删除对齐全部旧数据，且旧片CRC不同。首份最终BIN未生成，第二份及after寄存器未开始。ready有限值和退出证据独立保留，稳定双读不记通过。

从entry开始196.085秒，已授权的唯一fallback RTS high／low完成；未发正常burn_reset，未追加重连或整窗重跑。子进程退出1、外层worker0，无强杀；串口与租约关闭。原程序返回及应用保存记录通过：boot28→29、saved174→175，保存时长及场景未倒退，保存失败0／活动fault0／累计fault仍3，CycleLog无活动，571.213秒未持久尾段仅在电脑。前次boot27到本窗前28的来源未知，不能算成本窗复位。

### RTC与人工末态

RTC再次回到2000年，用户自行通过手机连接校时；主机没有执行后续校时命令，也没有补读手机校时后的UTC／步数。用户最终明确“常亮已关，时间／显示／触摸／KEY1正常”，本窗收尾完成。用户确认RTC本身不阻塞OTA，与升级核心结果分项处理；不会为RTC总失败重复ROM演练。

本机disable_modem=1保持。用户所引官方说明描述“打开串口意外触发RTS复位”的规避；本窗是程序显式RTS复位，不能据此判旧设置失效或要求重设。已有软件reboot保时／RTS丢时对照，且11/09未SYSRESETREQ已丢时，仍指向RTS关联复位／供电路径；具体RTC电源轨未测。设备内OTA的重启与RTC保持另按实际路径验收。

<a id="stub-buffered-20261110"></a>
## 2026-11-10 接收证据记录缓冲与缺字回归

实机约103万次RX返回常为1／31B，每次在互斥锁内同步写原始文件及JSON事件，再继续串口读取。该热路径存在明确小块写盘开销，但不足以把它认定为152B丢失根因。本轮只改项目主机记录层：三类二进制缓冲各64KiB、事件256KiB，总容量448KiB；逐次原始字节、offset、长度和时间仍保留，未更改官方库／stub、串口读策略、1M、块大小或CRC规则。

TX意图在发送前刷新，实际发送后日志失败仍返回已消费长度并锁存错误；阶段事件、下一协议操作及最终退出显式刷新全部流，失败则阻止新协议及正常入口RTS，保留一次已授权清理及low释放。所有复制句柄共用记录器。`trace.flushed=true`表示刷新成功交给OS，不等于掉电持久；强制回收可丢缓冲尾部，不能作为完整流证明。

主机包`ota-stub-observer-build-20261110-001`的Rust7/7／Release与OTA Python140/140（125.580秒）通过。实际固定库的完整双16MiB分片模拟通过，事件／RX底层文件write为1215／678次；负例在正确CRC计算后删152B并保留CRC／OK，仍在最后块超时、未生成NOR文件，按模拟授权fallback且关闭资源。新增缓冲满／最终flush失败守卫及原TX析构不重发、日志恢复不能重开入口均通过；只读审查未见阻断。[软件证据](evidence/2026-11-10/ota-stub-buffered-software.json)保存源码、精确EXE、日志、正负例及逐成员回读包。本软件轮没有设备动作；原四类浏览器交互证据复用，UI流程未改。

实际写擦准入仍需核明固定bin的DMA归属与擦除范围。本地限定目录未取得同版源码／ELF／map；官方[驱动映射](https://github.com/OpenSiFli/SiFli-Wiki/blob/main/source/en/faq/peripherals/flash.md)及[下载流程](https://docs.sifli.com/projects/solution/7.FAQ/production_line/burn.html)也只给驱动选择／调用流程，不能用来证明该SHA bin内部擦除包络。后续采用有界固定bin核验，保留核心证据，不把相似SDK HAL代码当作同版实现。

有界固定bin核验已按原库装载基址0x2005A000定位命令表：`burn_erase_write`指向0x20065068，消费者0x20068A04把长度按运行时粒度取整，再调用0x2006E164；bin自身字符串标识该函数为`rt_nor_erase_rom`。其sector为`4096 << byte(handle+0x24)`，block为`65536 << byte(handle+0x24)`，仅满足长度与地址条件时选block；不能据4096请求直接宣称64KiB擦除。主代理抽查sector构造指令，与只读审查结果一致。

实际地址映射还取决于特殊句柄0x200007AC／范围0x200007E8、五个72B句柄`[0x20041FF4,0x2004215C)`及后备索引0x20000464。命令表指针为`word[handle+0x0c]`，索引27／41对应`T+0x103`／`T+0x181`各9B，opcode再写入控制器；运行时具体值尚未读得。UART1与1M初始化在bin内可见，ready的DMA7仍只由端点间接支持，不能声称已证明写擦期间所有权。下一适配只为固定RAM配置短窗补这类具体缺件；旧ROM／双备份证据继续复用，未授权写擦。

<a id="stub-geometry-20261110"></a>
## 2026-11-10 固定RAM配置短窗软件与现场合同

从`bb785e3`接续，核心问题缩小为本板原版stub的Flash句柄、命令表和UART RX DMA绑定。采用同一原库、原stub及1M单会话，`--geometry-only`跳过两份NOR及after寄存器，只补缺失的运行时配置。[固定bin依据](evidence/2026-11-10/ota-stub-geometry-static.json)与[软件证据](evidence/2026-11-10/ota-stub-geometry-software.json)分别保存，旧152B失败不改写为成功。

### 固定范围及判定

- ready寄存器280B沿用原快照。固定RAM共568B：五个72B句柄、特殊句柄72B、后备索引及共享状态各8B、UART对象120B。
- 仅当UART对象的DMA指针4B对齐且位于`[0x20000000,0x20059FC0]`时，再读一个64B结构。核其Instance／Parent／三回调与先前ready通道、UART RDR及RX目标的关系；结果是顺序采样时的配置绑定，不证明之后持续静止。
- 六个句柄各读命令27／41；每项取覆盖9B记录的对齐12B窗口，合计最多144B，仅接受完全位于原stub镜像`[0x2005A000,0x2007A6D0)`的地址。保存opcode与原镜像逐字节比较；越界、空指针或未选中句柄如实保留未知，不追读其他地址。
- 以上采样最多1056B；装载／调试协议和原stub既有寄存器设置仍按原合同，不把采样字节数解释为总线流量或全部只读。主机独立复核原始块、整数形状、指针和命令索引，并报告目标B地址匹配的句柄、NOR字段及条件sector值；所有结果保持`all_flash_users_controlled=false`及`physical_erase_envelope_verified=false`。

### 软件验证与保全

本机`artifacts/ota-stub-geometry-build-20261110-002`绑定7个项目构建源，Rust1.90.0／GNU target的8项测试与Release通过；冻结EXE为`86cc7c37…`。OTA Python145/145、134.930秒通过，新增固定RAM正例、坏指针拒绝解引用、结果形状与IPC有界读取负例；原双NOR及ROM profile回归保持。正例不产生NOR请求或BIN，正常返回命令单发；坏指针仍保留完整固定块而不生成写入许可。

浏览器已实际点击RAM计划的RTC异常和自动阶段取消：前者RAM／保存记录／资源关闭／模拟末态4项通过，RTC单项失败；后者自动阶段禁用确认，取消后child回收、串口／租约关闭。该页面证据的执行源码已冻结，之后仅`ota_rom_device.load`加入PermissionError的250ms只读重试，页面／动作状态机未变；新版本由145项回归覆盖。原142项回归出现一次Windows文件拒绝访问，不能确定锁持有者；发布本已采用临时文件原子替换，未改成重发请求。中断回归及失败目录均保留，最终完整日志另存。本软件轮没有COM枚举、串口打开、复位或新固件构建。

### 待当次确认的最小设备窗口

总窗上限300秒，stub阶段60秒，bind30秒、前后应用各60秒、人工准备／返回／末态各60秒；这些阶段共享总期限，不相加延长。建议现场预留5分钟，原板原CycleLog固件、USB／BAT接线固定、人在旁、显示触摸正常，关闭其他串口工具，开启临时常亮供前后态读取。

待授权动作精确为：**一次主动入口RTS、一次SYSRESETREQ，装载同SHA原版RAM stub（含原BKP0R／PA21设置），读取上述ready与固定／有界RAM配置，单发burn_reset返回；异常时最多一次退出RTS。** 没有NOR读取或Flash写擦，不自动校时；RTC变化独立记录，升级核心判定不以RTC连续性为前置。应用RAM尾段只保全到电脑，返回后由人确认原程序，再读保存记录并确认末态、关闭常亮。旧观测窗已结束，新短窗尚未授权。

完成条件是取得可校验的固定配置原始值与本板句柄／命令表、UART DMA关系；未知项明确指出对下一写入动作的影响。若核心数据有效，即使RTC或辅助报告失败也按独立事实继续审阅；不为双NOR总passed重复整窗。随后仍须结合其他Flash使用者及真实写擦／恢复准入审查，采样本身不放行boot或B写入。当前活动和唯一下一步回STATUS。

### 本窗实绩与选址修正（合同已执行、窗口已关闭）

上述待确认合同由用户对`2c5dafb`明确授权后执行，002包在原CycleLog上运行2.703秒；ready280B、固定RAM568B及MPI2命令记录24B，共218次读取／872B。主代理从TX中逐一取固定DAP地址、按RX偏移还原全部响应，与报告每个word一致；TX意图与实际相同、RX／TX偏移连续。B目标匹配`0x2004203c`／MPI2`0x50042000`，容量16MiB，`H+0x23=0`／`H+0x24=0`，表`0x200751d4`的命令27为`0x20`，命令41全0且原bin字节一致。容量恰为16MiB时sector函数选27，4096B长度小于65536B不进入block分支，**固定bin＋本板配置支持4KiB sector路径**；本窗没有实际擦除，不能写成物理邻区已验。[现场原始流及复核](evidence/2026-11-10/ota-stub-geometry-device.json)。

本次读取的UART对象`0x20000ad8`有1M／UART1配置但DMA指针0。进一步核固定bin：`0x20062528`初始化该对象，`0x2006673c`用于调试发送；ISR`0x2006250c`及RTOS配置路径使用另一个对象基址`0x20042260`。此前选址把调试对象当成RTOS接收对象，属适配错误；因此DMA绑定未成立，不能把零指针解释为无活动DMA。003包仅把采样和主机复核改为RTOS对象、显式记录／检查对象地址，未扩大字节上限；Rust8/8、Release和对应Python17/17通过，后续全OTA146/146通过。现场002源码已独立冻结，旧证据和错误解释保持可追溯，没有重采设备。

单发burn_reset返回，无fallback RTS；boot29→30／saved176→177，已存时长与场景未倒退，保存失败0／活动fault0／累计fault仍3，206.081秒RAM尾段仅保全电脑。worker／child均退出0，无强杀，串口／租约关闭；操作者在页面确认返回及关闭常亮、显示／触摸／KEY1正常。RTC重置独立记录，主机未校时。该设备窗口结束；后续规则纠偏指令没有新增设备授权。

<a id="ota-delivery-20261110"></a>
## 2026-11-10 交付收敛与下一设备成果边界

用户指出把长期阶段当作单个PR边界，导致软件长期积累，并反复围绕观测／模拟／工具修正推进。此次同步AGENTS、CONTRIBUTING及DOCUMENTATION-WORKFLOW，按可独立审查、验证和回退的变更及时交付；达到有限边界即交付，新目标进入后续变更。[PR211](https://github.com/e-n-ye/wristflow/pull/211)规则修订、[PR212](https://github.com/e-n-ye/wristflow/pull/212)固定探针软件包已分别合入，ROM／stub主机工具及已有现场证据另作一个变更；不等待B执行或完整OTA，不按旧18个检查点机械拆PR。

有效成果分三类使用：11/08双16MiB及原boot／B恢复切片是当时备份；11/09 ROM进入／2.962秒保持／正确SP原流／独立释放和原程序恢复按各自窗口成立，不伪称同窗连续；11/10原版stub有限快照及本板MPI2／命令表配置支持条件擦除路径。首NOR少152B且CRC不符是真实数据缺失，仍不能作为完整新备份。缓冲、IPC短暂拒绝访问和UI报告问题作为辅助工具事实；RTC单独处理。交付审查发现的损坏ROM响应误报无动作及旧备份退出未知释放租约影响软件集成安全，已定点修复／负例验证；不借此扩展观测工具。[最终主机交付证据](evidence/2026-11-10/ota-host-delivery.json)。

**写擦控制的有界判断**：现有快照不足以证明整个写擦期间持续受控；修正UART对象再拍一次也不能独自回答持续性。本轮不再采样。后续离线评审只覆盖固定SHA bin中①运行入口对LCPU／其他主设备的控制来源，②UART RX DMA实际调用／端点及写擦期间访问范围，③每条写擦前后控制和异常退出。输出必须落成单会话、无重连／重试、原程序停止、明确其他Flash访问者控制、精确地址／长度及只执行一次恢复的可执行合同；不能把“DMA全零”当统一条件，也不能把端点线索当全部所有权证明。

**离线停止条件**：上述路径／必要控制来源可以由固定bin与已有有效值闭合时，完成合同和对应软件故障验证；若存在不能定位的运行时所有者、未知总线访问者或控制调用，则列明它将影响哪条写入及缺少的控制，并结束这轮判断。此时替代路线是单独评估可审阅、可构建且显式控制这些参与者的官方RAM下载程序或自有受控写入适配；需SDK／路线取舍时先提供证据与代价讨论，不暗改锁。禁止以不断增加类似短窗延长本次交付。任何新观测提案必须同时给出“缺哪个值／它改变什么决策／哪种结果足以结束”；回答不了则不增加采样。

**下一个设备成果的有限验收边界**：建议只验证B首4KiB`[0x12420000,0x12421000)`的候选写入→独立完整读回一致→恢复同窗新鲜原4KiB→再独立读回一致，并比较后60KiB保护区前后逐字节不变；B后60KiB含53547个非FF字节，不能扩大擦除。保留原boot／FTAB／A／loader／FS／KVDB，不在这一步更换boot。结束时原程序返回、保存记录不倒退、资源关闭及人工末态确认；RTC不参与核心通过判定。该合同必须先闭合写擦控制和异常恢复，再取得具体当次授权；本页只是验收边界，不是执行授权。成功后获得首个真实写入／读回／恢复结果，B启动／boot替换及自动回滚分别进入后续可验收变更。

当前活动和唯一下一步只回STATUS；本节不建立第二任务队列。测试数量、工具完善和检查点提交仅支撑结论，OTA主线以设备写入、启动、恢复和回滚结果推进。
