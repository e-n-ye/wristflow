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
