# 人机协作测试与验收工作流

活动项和唯一下一步只见[STATUS](STATUS.md#current)。本页保存方法和试点边界；历史命令不构成新设备操作授权。

<a id="shared-operator-workflow"></a>
## 复杂人机协作的通用入口

2026-10-24用户明确要求：复杂的人机协作都可以考虑使用既有协作页进行测试验收，并沉入项目文档。该工作方式适用于多次动作确认、等待／倒计时、采集和停止／状态恢复等流程，器件不限于电量。接续时由AGENTS／PROJECT_INDEX进入本页，结合HARDWARE及STATUS判断是否适用；简单查询或短小单步检查仍按实际复杂度安排。

优先复用`scripts/battery_console.py`的`Console`和`scripts/battery_lab.py`的`Journal`：已有统一页面、阶段／倒计时、确认身份、取消、进度与事件持久化及收尾展示。文件名保留既有来源，不表示只能用于电池；下文各电量入口仍只允许各自白名单，不能改标签就当作新器件适配。新器件应添加专用会话／数据适配，在同一协作会话集中准备、提示实际动作、采集、停止和末态确认，沿用通用交互与记录，而不是另外搭建临时确认页面。既有框架确实不适用时先记录具体缺口和选择依据。

实机前先用不访问端点的模拟适配，实际验收按钮、阶段切换、倒计时／确认、取消／异常和资源关闭；“静态页面存在”或源码审查不等于交互可用。说明哪些动作由人确认、哪些由程序计时，自动标记不能冒充人工确认，也不能以点击代替真实设备观测。采集时限必须覆盖操作与交互延迟，超时保留已得证据和未完成项，调整方案后再开始明确的新窗口，不能隐藏失败或盲目重发。

器件适配仍须独立核对精确固件／供电／端点，保存前态与原始字节，限定命令和会话所有权，区分“页面流程完成”“数据满足窗口条件”“设备停止／恢复确认”。取消与异常只清理本工具确认拥有的窗口，关闭串口并保留失败；不借通用协作页获得复位、校时、下载或其他器件操作授权。源码／编译／实机证据继续分别判断，结果与下一步回各专题和STATUS。

25日已交付UI的集中验收用`scripts/ui_acceptance.py`小适配：默认只模拟；设备模式显式传入`--port`、`--result`、`--flash`、`--source-archive`、完整`--expected-bin`及本轮现场`--confirmed`。它复用上述页面／日志和serial_lab绑定守卫，限定20分钟窗口、每组600秒人工确认，动作期间只被动记录，前后只允许`show_date`和ui／steps／wrist／light／pm的缓存status。无控制／重连／复位／校时，异常取消只关闭其主机资源；操作者负责退出测试功能和关闭临时常亮。浏览器七步确认及取消须先在模拟模式验证；`workflow_passed`与`physical_observations_confirmed`分别记录，`hardware_verified`不由工具自动置真。快照超时保留warning，不因此伪造成功回读。流程与本轮数据结果仍分别记录。

26日日常计步用`scripts/imu_acceptance.py`专用适配，同样默认模拟，实机绑定参数沿用上行；现有UI适配不解析完整步数／小时帧，因此在此小适配保留对应判定，不扩成通用步骤引擎。默认准备／非零增量／自然保存／收尾四阶段，人工确认每步180秒、总窗口10分钟。`--offline-walk`增加拔线／回连两阶段：取得基线后先关串口并释放租约，再由人拔USB用原电池走动；人确认接回后才重新核绑定与端点并开口，before／after原始日志分开保存。此模式每步300秒、总窗口15分钟，超时不自动续窗。约40个单脚步后结束动作，即使数字没增加也接回取证，不以一直走动等待成功。

自然保存阶段至少30秒、最多90秒，每5秒只查steps／wrist／light缓存。要求核心行唯一、小时mask完整、同日native／daily增量一致、小时不下降且同小时增量一致；跨小时不推分配。重开后比对start不变及sample的uint32增量与主机间隔（容许70秒采样缓存差），这只是有界连续性检查，不是唯一启动身份认证。verified／clean／同日保存total和mask一致才完成保存阶段，mask不证明保存的逐小时字节。无save、重启、校时、PM、器件控制或自动重连；异常只关主机资源，操作者关闭临时常亮。先验浏览器模拟、取消和解析守卫；`hardware_verified`始终false，真实硬件结果由原始证据审阅判定，未出现新的恢复计数时不写故障恢复通过。下载／启动仍是另行授权的独立步骤，历史执行快照不作工具依赖。

24日地磁旋转曾使用聊天逐步确认，因120秒总期限未采齐；随后临时页面按钮无法工作，状态确认未启动新采集后已停用。这暴露了通用入口不醒目及未复用既有控制页的问题。随后专用适配复用Console／Journal，实际模拟点击验完整流程、取消及超时，再用于设备。五方向与显式停止取得证据，但原workflow因把seq0初始化日志算入普通样本而失败；保留原失败，按原始字节独立核验设备条件，并用聊天末态确认补充人工证据，不能回填为页面确认。过程和脚本指纹见[本轮证据](evidence/2026-10-24/mmc5603-compass-readiness.json)，实际进度只见STATUS。

<a id="local-experiment-tools"></a>
## 本地实验工具复用与接续

新会话先由PROJECT_INDEX按任务进入本节，再用工程地图确定模块；活动任务仍只读STATUS当前区。先查已有入口及`--help`，只有协议、会话归属或清理语义确实不同才增加小型器件适配。版本、端口、时长和动作说明差异优先用参数；一次性探索可保留执行源码快照，但不得成为后续工具的运行时代码依赖。

| 职责 | 正式位置与复用规则 |
|---|---|
| 原始收发、有限等待、短写检查及资源关闭 | `scripts/lab_serial.py`的RecordedClient／RecordedSession；PortLease／SerialTransport沿用既有实现，电量原导入名称兼容。基类默认拒绝全部命令；适配器声明白名单、冷启动标记及完成条件，设备停止由适配器负责 |
| 协作页和阶段记录 | 原Console／Journal；`prompt(..., pump=...)`在等待人确认时持续采集，`automatic(..., pump=..., until=...)`表示程序判定的有界采样／计时；确认按钮不能完成自动阶段。无需复制页面、线程或直接写内部prompt字段 |
| 文件哈希、显式清单与JSON输出 | `scripts/lab_evidence.py`；电量原digest／write_json导出兼容，不按目录扫描所有历史文件 |
| 构建／证据／改动文档检查 | `scripts/lab_check.py`的build／files／docs；读取既有结果，不构建、不下载、不枚举或打开设备。每次重新读输入，不把旧passed报告当缓存 |
| 地磁已完成实验的模拟／回放 | `scripts/mmc5603_lab.py`，纯解析见mag_protocol／light_protocol；本阶段CLI只提供simulate／replay／status，不开放真实端点。五方向策略属于地磁适配，不变成通用步骤引擎 |
| 简单亮度／指南针查询及AOD被动记录 | `scripts/serial_lab.py`复用同一RecordedClient／RecordedSession；light仅允许`wf_light status`，compass仅允许`wf_compass status`，passive不允许发送命令，不要求完整网页。参数表达时长／间隔，纯解析只认缓存字段，不控制页面／传感器／亮度或AOD |
| 构建／下载 | 继续使用Build.ps1／Flash.ps1；实验工具读取其结果，不复制下载包装器，不因电脑端整理重编固件 |
| 长期记录只读导出 | `scripts/runtime_export.py simulate|capture`复用RecordedSession／Journal及serial_lab绑定，最多60秒；主机nonce固定快照，摘要／样本／manifest各自CRC，失败关口保留原UART，不发送start／stop、复位、PM、校时或Flash命令。`runtime_record_decode.py`只处理保存字节；[行为／首窗边界](BATTERY-ESTIMATE.md#runtime-recorder-20261030)，三场景离线长窗另需Console专用适配 |
| 三场景纯电池验收 | `scripts/runtime_scene_lab.py`默认模拟，真实端点显式参数及本轮确认；复用Console／Journal，先关口再拔USB，人工回连确认后重核端点。B1 RAM start900单发、保护旧trace并只停自身身份；长期记录器只读导出、持续自动运行。`--scenes`接续余组，`--protected-trace`核先前冻结导出，`--restore-cache`保全首窗偏好参考；用户选择保留测试设置时用`--keep-settings`对齐提示。场景计时程序执行，首尾电压只作辅助，OFF不新增采样；[边界](BATTERY-ESTIMATE.md#runtime-scenes-20261030) |
| 实验前后快照／下载后启动检查 | `scripts/device_snapshot.py`的capture／startup，具体用法见[快照入口](#device-snapshot)；版本、端口、输出与查询集合用参数表达，不再逐轮复制preflash／post-status包装。startup校时权限独立于capture |
| 板端指南针候选实验 | `scripts/compass_probe_lab.py`提供simulate／device／status及smoke／axes／calibrate有限窗；device需要精确build／flash／BIN／端点和本轮现场确认。沿用Console／Journal／RecordedSession及原始UART，`inputs/start/freeze/stop/status`独立白名单、会话／代次／起点／时长归属检查，只停本次窗口，控制单发；轴向假设、有效配对、校准参数和磁北精度分别判断，见[指南针实验](COMPASS.md#device-probe-20261027) |
| 三图／字库诊断显示验收 | `scripts/resource_acceptance.py`复用Console／Journal／RecordedSession，默认无端点模拟；设备模式只接受显式ResourceProbe构建，绑定result／flash／源码ZIP／完整BIN／端点／本轮确认。默认四视图，`--hours`为十张新增小时图＋缩略图；`--start-mode 4..14`可新建会话只补余项，旧失败独立保留，不复用旧tag。每视图60秒、人工确认45秒，串口回执限3秒、总流程限20分钟；仅status／start／匹配tag的stop，清理另核mode／start，丢ACK记录不确定归属，不盲停／重发；异常保留recovery_required并关闭主机资源。无RTC／PM／传感器／通知存储控制，验后恢复普通Product；[版本证据](FLASH-CAPACITY.md) |
| 输入与证据 | `tests/fixtures/`保存可分发的最小回放输入；`artifacts/`保存运行配置、UART原始字节、事件、报告和必要的执行源码快照；精简交付证据在docs/evidence |
| 厂商算法的纯离线执行 | `scripts/memsic_preflight.py`复用SDK／文件证据检查，用原Arm GCC构建独立ELF，在固定版本Unicorn执行SDK候选；`compass_geometry.py`仅作静态前轴磁北投影参考及另一参考轴诊断。候选门槛与projected门槛分开，前者失败仍退出1并保留trace；无串口或Product构建，型号／接口允许用途／使用条件另核，见[航向边界](COMPASS.md#heading-boundary-20261026) |

```bash
python scripts/mmc5603_lab.py replay --out artifacts/mag-lab/replay-001
python scripts/mmc5603_lab.py simulate --out artifacts/mag-lab/sim-001
python scripts/mmc5603_lab.py status --run artifacts/mag-lab/sim-001
python scripts/serial_lab.py simulate --mode light --out artifacts/serial-lab/light-001
python scripts/serial_lab.py simulate --mode passive --out artifacts/serial-lab/passive-001
python scripts/serial_lab.py simulate --mode compass --out artifacts/serial-lab/compass-001
python scripts/serial_lab.py replay --fixture tests/fixtures/light-status-20261024.json --out artifacts/serial-lab/light-replay-001
python scripts/serial_lab.py replay --fixture tests/fixtures/aod-passive-20261023.json --out artifacts/serial-lab/aod-replay-001
python scripts/serial_lab.py replay --fixture tests/fixtures/compass-observer-20261025.json --out artifacts/serial-lab/compass-replay-001
python -m unittest discover -s tests -p 'test_lab_*.py' -v
python -m unittest discover -s tests -p 'test_compass_geometry.py' -v
python scripts/compass_probe_lab.py simulate --profile calibrate --out artifacts/compass-probe/sim-001
python -m unittest discover -s tests -p 'test_compass_probe_lab.py' -v
uv run --no-project --with-requirements scripts/requirements-memsic-preflight.txt python scripts/memsic_preflight.py --cc <已有Arm-GCC可执行文件> --out artifacts/memsic-preflight/run-001
```

输出目录必须是新路径；status只读取进度，不恢复进程或重启设备窗口。模拟不导入serial、不枚举COM、不打开端点；模拟逻辑时间与快速样本是替身输入，不是设备时序证据。设备适配函数将是否模拟、工具源码指纹、原始UART、启动意图／完整session-start-duration-mode身份、停止确认、端口关闭及人工末态确认分别保存；取消／超时只停止已确认所属窗口，冷启动、异窗口或不确定归属明确保留recovery_required。

`serial_lab capture --help`提供受绑定的真实端点入口，必须显式给mode／seconds、port／USB location、既有Build和Flash记录、完整预期BIN SHA及当前同板／同固件／安全供电接线且人在旁的`--confirm-same-board-and-firmware`。它先复用公共build核验，再在共享PortLease下、每次打开前复核所有绑定文件／工具、SDK及CH340端点／禁Modem握手设置；准备后有任何变化就拒绝开口。参数只是执行者当前确认的记录，不替代聊天中的设备授权，历史命令也不能复用为当前现场确认。没有固件证明协议，板上身份仍明确为未验证。

light／compass最多30秒，passive最多600秒；Ctrl+C按取消保留数据并关闭主机资源，没有自动重连，不发送stop／PM／复位／校时／传感器控制。被动窗口仅收原始流，冷启动字节留作证据；缓存查询遇冷启动即失败。CLI的capture不恢复旧进程或设备窗口；多步动作仍用Console／Journal与专用适配。入口初次交付仅替身测试；2026-10-30[记录器同版补验](BATTERY-ESTIMATE.md#runtime-uart-wake-20261030)已完成30秒passive实机采集、commands为空及资源关闭，具体设备结论限于该窗。

历史fixture包含26,117字节原UART及五方向索引，记录原session／events／workflow文件路径和SHA；旧workflow的error／passed=false原样保留。纯解析保留seq0／invalid行，地磁策略仅识别自检前唯一的全零初始化通知，不能将任意坏帧过滤成通过。离线得到86帧／五窗／停止证据不等于新实机通过，也不证明磁北、倾斜精度或功耗。旧实验仍从MMC5603等专题及docs/evidence中定位；不删除、移动或改写被引用的artifacts。

亮度fixture为原ambient-002的2,284B UART，回放三条同session缓存seq11／16／21；AOD fixture为原natural-001的5,901B纯被动流，原session commands为空。二者都保留来源路径／SHA及原会话元数据，只证明历史字节可读，不重判物理亮度／面板效果。简单采集的`seconds`约束查询与有限重试的发送／接收窗口；窗末只做原始尾采集（名义100ms加一次最多50ms read），不发新查询，窗外返回不能算该窗成功响应。取消后关闭主机资源，不发送器件stop／PM／复位命令。

指南针fixture保留25日device-002全部48,056B UART，`compass_protocol.snapshots`只接受相邻完整缓存／raw两行，得到120份（51份valid）；invalid／stale状态保留，交错或残缺行保留原始字节，不拼补。它不是磁北精度或新的实机验收。新增指南针device_workflow已经复用Console／Journal／RecordedSession，不存在导入旧artifacts代码的链；其startup的复位／校时仍是历史专用流程，不混入只读工具。资源生成脚本和编译保全／map专题分析有独立职责，不能按.py数量机械合并。

整理分四个独立交付闭环：公共基础与地磁模拟／回放；亮度查询和AOD被动记录作为第二调用方；构建／证据／文档检查及失效规则收敛；入口发现、依赖隔离和新会话接续走查。完整范围覆盖这些闭环，实施状态只回STATUS。AOD复位／校时／PM控制、电量离线恢复、旧腕诊断与第三方参考不机械迁移；明确的新设备授权仍另核现场条件。

<a id="device-snapshot"></a>
### 前后快照与启动检查

先查`python scripts/device_snapshot.py --help`及对应子命令。capture不经过网页，只查显式`--query`命名项；默认date／steps／wrist／light／pm，可选compass／probe／threads／memory。不接受任意命令。输入沿用serial_lab的result／flash／完整expected-bin／port／location及本轮同板固件与安全供电的确认；输出必须新目录。最多60秒，保存binding、原始UART、事件及snapshot；超时、冷启动、短写或取消失败关口，不自动重连／重发或操作器件。`passed`仅证明所选完整回执取得并关闭主机资源，健康、停止与保存仍由协议或人工解释。

startup使用相同绑定，另须`--allow-time-sync`，只在RTC与真实UTC偏差超过60秒时单发一次`wf_time`并读回（已校时容差8秒，未校时容差60秒）；丢回执失败不重发。它没有复位／RTS／PM／传感器控制入口，现场校时授权独立核对。准备好后直接运行，不再创建等待另一个实验progress的脚本；器件窗口停止及资源释放仍由原适配负责。

显式`--query storage`只用于`-StorageProbe`诊断构建，发送一次`wf_storage scan`，不能与startup校时合并；默认快照项不包含它。复用上述有限采集而无需多阶段协作页，先用`simulate --query storage`验证交互；真实使用仍核本轮同板／BIN／供电。主机校验字段、四sector次序与字节守恒，完整但`coherent=0`的诊断可成功采集，不能解释成容量准入。只读限扫描路径，启动FlashDB和正常业务可能写入；[具体边界](BATTERY-ESTIMATE.md#storage-metadata-probe-20261029)。

```bash
python scripts/device_snapshot.py simulate --query steps --query wrist --out artifacts/snapshot/sim-001
python scripts/device_snapshot.py simulate --startup --out artifacts/snapshot/startup-sim-001
python scripts/device_snapshot.py capture --result <Build结果> --flash <Flash结果> --expected-bin <完整BIN-SHA256> --port <COM端口> --location <USB位置> --confirm-same-board-and-firmware --query steps --query wrist --out <新输出目录>
```

历史preflash／startup／post-status脚本继续保留为执行证据；新实验通过同一入口选参数。单次分析／证据汇总仍可留在artifacts，通用文件检查使用lab_check。代码与输入未变时，同一最终版本集中执行受影响测试和文档检查；开口前身份重验与封存后读回保留，单纯改电脑端工具不重编固件。

### 公共检查与重验条件

在仓库根目录运行；下例的result、ZIP与flash路径由本次实验明确选择，不自动把日期最新的文件当作正确版本。`--out`放在子命令前且必须为新路径，可省略后仅输出JSON。失败退出1并保留错误；不要通过改写旧报告消除失败。

```bash
python scripts/lab_check.py --help
python scripts/lab_check.py --out artifacts/lab-check/build-001.json build --result <Build结果路径>
python scripts/lab_check.py --out artifacts/lab-check/evidence-001.json files --manifest <证据JSON> --key files
python scripts/lab_check.py --out artifacts/lab-check/docs-001.json docs --base HEAD
```

build核现有Build.ps1的成功记录、SDK／子模块锁及当前干净SDK、非空显式源码清单、全部产物哈希、必须产物及三镜像地址；可用`--flash <Flash结果路径>`核已有成功传输记录与同一三镜像。它不运行Flash／DryRun，不证明板上身份。默认严格比较源码字节；遇Git换行差异时，只有显式`--source-archive <原始源码ZIP>`才允许例外：ZIP条目使用仓库相对路径，先匹配原构建SHA，再严格证明仅CRLF／LF变化，并在报告单列。产物永远按字节。构建记录不能证明依赖收集完整；新纳入源码／配置需重新构建，不能凭旧清单通过代替。

旧源码／产物已被新开发覆盖时，另用`--build-archive <完整ZIP> --recorded-root <原工作树绝对根>`：ZIP按原根相对路径包含构建清单全部源码和产物，逐项核原SHA及存在的大小字段，从归档manifest核三镜像并与原Flash记录绑定；不解压、不按文件名猜映射。此策略与source-archive互斥，报告`source_policy=archived_build`、`current_source_verified=false`，只证明历史构建归档；SDK仍要求符合现有锁。device_snapshot也接受这两个参数，开口前重核ZIP、记录、当前工具／SDK及端点，不要求已覆盖的原文件仍匹配。它不提供新固件构建或下载准入证明。

files只核明确选定的记录列表（`--key sources`或点分字段亦可），拒绝空列表、重复路径、缺文件或哈希／大小变化；不递归猜测历史JSON中的每个SHA。历史工具指纹与当前工具不一致可能是合理历史，选对适用的列表后再核。输入接纳时核验一次，输出完成后再封存读回；后续引用同一记录，变更或跨会话读取时复核。

docs比较`--base`与当前工作树，包含已暂存、未暂存和未忽略的新文件；UTF-8仅查变更文本，Markdown只查新增行的本地行内／引用式链接、ATX标题与显式锚点，围栏代码示例不当链接。可重复`--path`进一步限定变更文件；提交后使用`--base origin/main`等实际基线，避免HEAD已包含改动却检不到差异。目标文件按新增链接读取，不通扫历史文档；远端URL、复杂Markdown语法及文档语义仍由审阅负责。输出记录输入与所读目标哈希，不形成新的任务状态。

| 事实 | 重验时机与复用边界 |
|---|---|
| 编译／链接及配置／map性质 | 按源码、SDK、工具链及产物版本验证；只改电脑端工具不重编固件，map专题断言仍由专题负责 |
| 本地构建／下载记录、源码与镜像身份 | 准备时集中核；输入路径、清单、源码、镜像、SDK或来源变化后重核；实际下载前仍由Flash入口重新核镜像 |
| 解析器／工具／fixture | 相关文件变化后运行受影响主机验证；原始UART不做换行归一化 |
| 端点、驱动与锁 | 每次开口／重连核；本地镜像或COM号不能证明板上版本 |
| 设备会话／停止与恢复 | 启动前、重连后、停止前后核；下载、重启、会话或冷启动变化使旧连续性失效，不由公共检查报告抵扣 |
| 证据／文档 | 接纳输入、封存输出及接续引用时核；同一最终版本集中检查变更文档，修改后重核；不跨会话缓存passed |

## 本轮范围与分工

2026-10-21用户要求借电量排查改进人机协作。PR #120首轮复用B2实机记录，完成可重复B3回放和OpenHTF网页演练；后续用户要求继续，增加模拟采集适配器及共同本地控制页。AI制定实验、分析证据；本地程序持有阶段和输出；人只确认需要判断或实际动作的部分。软件验收由主代理在浏览器操作，没有让用户重复设备输入。

历史回放、引导演练与模拟采集仍由原入口提供；`battery_device.py`只读实机入口保护已有停止态记录，不开放实机start／stop。后续`battery_experiment.py`独立承接获准的新负载记录，先保护再单次启动、纯BAT记录后回连停止／导出。二者均不开放PM hold／allow、charger控制、复位、校时或烧录。实机执行及验收状态只见[STATUS](STATUS.md#current)，不把主机替身通过算作设备结果。仍沿用[硬件条件](HARDWARE.md)、[下载](FLASHING.md)与[电量契约](BATTERY-ESTIMATE.md)；SDK及诊断协议保持，实际BIN按每轮绑定核对，新固件入口见文末。OpenHTF保留phase／measurement／JSON，不再使用其StationServer／SockJS前端；不构成全项目框架选型决定。

## 离线回放

输入`tests/fixtures/battery-b2-20261021.json`来自B2 session3的121条解析记录，保留原始解析文件和8页UART文本的SHA-256及原BIN身份。完整原始文件仍在原工作树忽略的`artifacts/battery-b2-20261021/`，不宣称随仓库分发。fixture不是新采样，也没有补齐OFF间隙。

`scripts/battery_lab.py replay`验证schema、停止状态、数量、会话、连续索引、ADC身份／年龄和记录窗口；初版直接编译当时`core/battery_estimate.c`，自生产停滞修复后显式编译`scripts/battery_baseline_20261021/`固定的`17a9de5`旧生产实现及主机专用`scripts/battery_replay.c`。记录开始前的历史不可重建，因此第0条只作热状态输入；其余各行比较旧生产C的滤波、有效性、候选、百分比、模式及decision。携带旧诊断的上下文行允许NEW／SEED与重复调用产生的CACHED对应，单列索引，不把它算作新ADC。历史匹配不再表示当前固件与旧显示相同。

原无参数入口另运行两个早期纯主机实验及其组合：缺样后30秒内、可信相同放电模式和新ADC恢复时复用历史；1点候选差异跨至少3个独立ADC及60秒后收敛。无效期间仍输出未知，不用旧百分比伪装新测量。30／60秒只是早期实验参数，未决定固件常量。历史结果见[B3](BATTERY-ESTIMATE.md#battery-workflow-pilot-20261021)；该旧路径没有下段新guarded守卫，不用于证明其边界已通过。

从仓库根目录运行；本机已有GCC可用，其他机器用实际编译器路径替换`--cc`：

```bash
python scripts/battery_lab.py replay --cc D:/msys64/ucrt64/bin/gcc.exe --out artifacts/battery-lab/replay-001
```

输出目录必须不存在，防止覆盖旧证据。输出包括`build.log`、精确`engine-input.txt`、引擎输出、四组预测、`replay.json`和便读的`report.md`；JSON记录源码、输入、引擎哈希及编译器／Python版本。失败没有通过报告，保留该次目录与命令输出。

当前生产修复另用以下入口，与历史实现及旧策略wrapper分开链接：

```bash
python scripts/battery_lab.py compare-production --fixture tests/fixtures/battery-guided-20261021.json --cc D:/msys64/ucrt64/bin/gcc.exe --out artifacts/battery-current/compare-001
```

可将fixture替换为原`battery-b2-20261021.json`。输出中的`historical/`先验证固定旧实现，独立的`battery_current_replay.exe`只链接当前core；`predictions.json`并排保存recorded／current，`comparison.json`列出变动百分比及其他字段不变的检查。除percent外的有效性／滤波／候选／模式若改变即拒绝，缓存decision别名仍单列。第0条沿用热状态，新增累计从空开始；不虚构记录前的60秒历史。编译和回放成功不替代新BIN设备验证。规则／本轮结果见[生产修复](BATTERY-ESTIMATE.md#display-settling-20261021)。

<a id="guarded-policy-matrix"></a>
### 带守卫的参数比较

```bash
python scripts/battery_policy_lab.py --cc D:/msys64/ucrt64/bin/gcc.exe --out artifacts/battery-policy/matrix-001
```

新入口显式启用主机`battery_policy.c`，运行18组：保留15／30／60秒 × 收敛30／60／120秒 × 相邻支持6／15秒，至少3份有序新ADC。参数保存在各`replay.json`的`guarded_parameters`与精确引擎命令中，不写入固件。每组独立输出和生产C校验；总报告`matrix.json`记录事件行、参数、源码／输入和子报告哈希，`report.md`便读。目录已存在或任一编译／回放／有效性比较失败即异常退出，不写成功总报告；以显式异常检查，不依赖可被`python -O`移除的assert。

历史期限锚定最后接受的ADC完成时刻，缺样期间PG无效仅允许暂存，恢复须确认新鲜放电新ADC；长调用空档也会过期。未知模式、坏样本、候选改变及支持超时清理相应历史／累计；生产有效性守卫保留同ADC的MODE_WAIT。支持超时不能通过重播种新鲜缓存来跳过等待。引擎验证有界模时间／ADC身份顺序，wrapper假定此前提成立；没有推断真实睡眠时长或未观测插拔。结果与未选参数的理由见[边界专题](BATTERY-ESTIMATE.md#policy-boundaries-20261021)。相关主机回归仍使用下文统一`test_battery_*.py`命令，不含设备操作。

## 网页演练与交接

用独立主机环境安装固定直接依赖`openhtf==1.6.1`和`pyserial==3.5`，不安装到SiFli SDK Python。已实测CPython3.12.9；OpenHTF仅用于离线演练的测试执行和正式JSON，接口来源见[官方Test](https://github.com/google/openhtf/blob/v1.6.1/openhtf/core/test_descriptor.py)。当前控制页由Python标准库HTTP服务提供，绑定127.0.0.1，不使用WebSocket、multicast或第三方私有发布接口。

```bash
uv run --isolated --python 3.12 --with-requirements scripts/requirements-battery-lab.txt python scripts/battery_lab.py guide --report artifacts/battery-lab/replay-001/replay.json --out artifacts/battery-lab/guide-001
```

打开程序打印的`http://127.0.0.1:<port>`。默认网页端口4444，也可传`--port 0`分配临时端口。页面每250ms读取状态，按钮提交当前prompt ID；过期／重复答复拒绝。修改请求校验Host、Origin和本次随机token；没有远程共享或任意命令入口。

三个演练阶段为准备证据、核对供电窗口、判断策略结果；点击“确认并继续”或“取消本轮”。默认每阶段300秒，`--timeout`可设大于0且不超过900秒。已完成数只统计真正完成的业务阶段，后续阶段保留“未执行”。已接受的取消阻止阶段完成；全部步骤完成或终态冻结后拒绝新取消，持久化失败仍降级为异常。**本轮不要拔线、唤醒或操作设备**：网页确认只说明演练阶段已确认。

每次阶段变化立即追加`events.jsonl`并更新`progress.json`；同时复制绑定的`replay.json`。结束时写`openhtf.json`，工具独立读回最终outcome和哈希后才能记`finished`；OpenHTF测试False归为异常，输出回调失败即使框架打印PASS，本工具仍异常退出、记`interrupted`。取消／超时有独立事件、最终`passed=false`和进程退出1，不能当设备故障。

换聊天或程序中断后，先读取同一进度，无需再询问用户上一阶段：

```bash
python scripts/battery_lab.py status --run artifacts/battery-lab/guide-001
```

`running_or_interrupted`表示尚无终态，不能仅凭文件判断进程还活着；尾行不完整记`incomplete`，证据哈希改变则拒绝。当前支持证据接续查看，**没有进程重启后自动恢复执行**。确认旧进程已结束后，可从同一离线报告在新目录重新演练；不能照搬为真实设备实验的自动重启／重新arm。

```bash
CC=D:/msys64/ucrt64/bin/gcc.exe uv run --isolated --python 3.12 --with-requirements scripts/requirements-battery-lab.txt python -m unittest discover -s tests -p 'test_battery_*.py' -v
```

测试覆盖真实C回放、损坏／缺失／混会话、保留期限／充电守卫、缺样仍未知、进度残缺、证据变更、取消／超时、报告写失败、串口协议与资源清理。软件测试输出保留在忽略的`artifacts/battery-lab-tests/`、`artifacts/battery-collector-tests/`及`artifacts/battery-console-tests/`，不删除旧轮次。

## 模拟采集与串口边界

```bash
uv run --isolated --python 3.12 --with-requirements scripts/requirements-battery-lab.txt python scripts/battery_collect.py --out artifacts/battery-collect/run-001
```

这里`--port`同样是网页端口，不能传COM号。流程为准备→启动并关闭模拟串口→确认模拟离线窗口→重连导出→清理，共5步、2次本地确认；不等待真实8分钟。模拟器执行与固件一致的文本协议，以原B2记录分片响应；不是新ADC、电池供电、RAM保持或触摸证据。

`Collector`在开口前取得按给定端点字符串（忽略大小写／前后空格）的内核锁，离线关口期间仍持有；进程退出后内核释放，遗留锁文件不表示仍被占用。它约束使用该适配器的程序，不代替实机驱动独占与设备身份验证。真实`SerialTransport`预先关闭DTR／RTS及三种流控，然后才open；没有复位、reboot或校时入口，实际Windows驱动行为仍待验。

准备阶段保护既有PM hold、运行中及任何非空trace，不能通过新start覆盖。start／stop各发一次，回执丢失后用status消解；无确定身份则失败并保留不确定状态，不盲目再次start。已确认窗口以session／start／duration绑定，导出各页再核state／count／cap；status及同页只读失败最多重读一次，会话变化立即拒绝。坏行不补字节、不拼接会话，收到冷启动标记即撤销连续性。

原始字节先写`uart.bin`再解码；各页每次尝试单独存`.bin`，TX、响应与drain的字节范围进事件记录。发送前、完成标记后和关口前做有界尾部采集：静默10ms／名义100ms上限，真实读取至多再叠加一次50ms读取超时。旧尾部存证后清空解析缓冲，不拼成下一条回执；这不声称记录了截止后才到达或未读取的未来UART。

取消或失败后仍尝试停止本工具确认拥有的同一窗口、恢复本工具可能申请的PM hold，再关闭transport／原始文件并释放锁。清理无回执、冷启动或窗口变化明确记`cleanup_confirmed=false`，不能报软件采集通过。还没有实机I/O失败和物理恢复证据。

中止时实际发生的清理显示为“中止后已清理”，并计入已完成数；例如离线阶段取消显示3/5（准备、启动、中止清理），导出仍为未执行，整体仍是取消／`passed=false`，不是把清理当作实验完成。

输出`binding.json`绑定输入哈希和历史BIN来源，`records.json`是严格解析的回读，`collection.json`保存软件流程结果、清理结果和窗口覆盖指标。`passed`仅表示模拟采集流程；历史记录的BAT观测跨度418.910秒、拔线延迟58.988秒与480秒目标分别显示。USB重新接入后的记录不计入该段跨度，uint32 tick回绕按模处理，OFF及PG未知单列；跨度不证明连续采样。历史／模拟输入的实验`complete`始终false，不能据其宣告新硬件实验通过。接续仍用`battery_lab.py status --run <本次目录>`；不自动重arm或恢复进程执行。

## 已发现的限制与实测准入

PR #120首轮网页通过／取消／真实超时的证据保留于[试点证据](evidence/2026-10-21/battery-workflow-pilot.json)。当前适配器软件轮的命令、版本、36项回归、浏览器结果和未验项见[适配器证据](evidence/2026-10-21/battery-collector-adapter.json)；无固件编译、下载、真实串口或新硬件证据。

**历史问题与覆盖关系**：首轮OpenHTF1.6.1前端的跨线程发布／未运行loop造成收尾警告，summary把ERROR及Waiting都计为执行完成。只读源码核查后，本轮保留其测试执行／JSON，移除该前端链；不修改site-packages或压缩前端资源。共同控制页按实际阶段计数并独立管理HTTP线程退出，原问题不再位于当前页面路径；这不宣称上游缺陷已修复，也不证明长期采集稳定性。

未来设备适配应在同一个本地会话中集中准备、提示动作、收集和清理，并先在模拟串口验收：单拥有者、开口前DTR／RTS关闭、原始字节先存证、基于完整回执等待、会话／页码校验、有限重试、取消后释放资源和恢复状态。设备记录启动时间、网页动作确认时间、PG／页面观测时间分别存储；不能因确认点击就断言物理动作发生，也不能把480秒记录器预算称为480秒BAT窗口。缺少关键窗口应明确未完成。此段是准入契约；获准执行顺序仍只回STATUS。

## 同BIN既有记录保护：受限实机入口

`scripts/battery_device.py`独立使用`ReadOnlyClient`，所有路径仅允许`wf_pm status`、`wf_battery status`与分页dump。固件dump不要求PM hold；PM allow还会触发采样定时器，故首轮不借用其控制命令。已有记录只属于被观察的会话，不赋予工具停止／覆盖它的权力。status／dump可能令已过期记录的元数据转为EXPIRED，不会清空记录。

操作者先确认同板屏／原电池／接线固定／供电与版本连续性。入口核对历史证据绑定的成功build、产物数量／唯一清单及每份哈希、来源BIN，并在每次开口前核对实际Windows COM／CH340位置和`DisableModemOutHandShake=1`；不能由USB标识证明板上固件身份，`device_identity_verified`保持false。实际端口、位置和build参数从本轮现场与证据取值，不复制历史命令自动执行。

页面先提示临时持续亮屏10分钟并保持原供电；确认后才打开串口。工具仅导出匹配既有停止态和全部原始行的记录，先保存`protected.json`及各页原始字节，再关闭／重新打开并完整核对为`reopened.json`。窗口／内容改变、冷启动、页面损坏或有限重读失败均停止，不重新arm。`uart.bin`与事件时间保存实际收发，关重开连续性仅限本次观测，不推断物理深睡／功耗。

随后页面要求操作者实际点击“取消本轮”，验证等待中的退出清理；点确认会记异常且检查未完成。正确取消仍为`outcome=cancelled`／`passed=false`／进程退出1，已保存记录不丢；两次导出匹配、指定阶段取消及清理确认全部满足时，独立字段`bounded_checks_met=true`。这不是新BAT负载实验通过；`new_battery_experiment_complete`始终false。收尾关闭串口／原始文件并释放锁，无start／stop／hold／allow；页面提示用户关闭临时持续亮屏并确认界面正常。

终态页面另列设备准备、记录保护、关重开一致、实际取消清理、资源关闭五项验证；按要求取消且检查齐全时显示“已验证5/5项 · 本轮已取消”。阶段计数仍为4/5，原始`cancelled`事件与`passed=false`不改，避免把刻意验证取消解释为漏项。早期报告缺少`preparation_confirmed`时，已完成的保护导出只用于兼容呈现其准备项；原报告保持原样。主机故障注入不替代实机故障验证，实际结果与展示修正分别绑定源码版本，见[本轮精简证据](evidence/2026-10-21/battery-device-export.json)。

接续读取同一输出目录的`binding.json`、`collection.json`和`events.jsonl`，原`battery_lab.py status --run <目录>`仍可查终态。这里仅提供既有数据保护和一次关重开／取消验证，不提供自动重启恢复、实机新记录或长时放电入口。相关主机测试同原`test_battery_*.py`命令，均用注入transport，不打开设备。

<a id="guided-device-experiment"></a>
## 同BIN新负载记录：真实协作页

`scripts/battery_experiment.py`使用本轮现场确认的`--serial-port`、`--usb-location`、`--build-result`、`--expected-trace`和新`--out`目录，网页端口由`--port`指定。历史命令不构成新运行授权；状态仍由`battery_lab.py status --run <本次目录>`读取，不自动恢复或重启已结束进程。精确已执行命令见[本轮证据](evidence/2026-10-21/battery-guided-experiment.json)。

操作者准备好原供电、普通亮度和临时持续亮屏后，先在页内确认。工具复用历史build／产物／端点核验，完整保护匹配的旧停止态记录及每页原始字节；磁盘文件哈希和紧邻start的精确前态均须一致。仅该独立入口可替换已备份的旧RAM会话，原采集器的非空拒绝默认规则保持。新命令白名单只含status、start900、stop及分页dump，没有PM／charger控制。

新start只发一次，回执超时用status消解；`ownership.json`在发送前持久保存预测会话／时限／旧now起5秒内的start边界，成功后保存精确session／start／duration。串口随后关闭，页面提示拔USB并确认，再自动计时：普通电池页90秒→人工进入手电筒并确认／90秒→人工退出并确认／恢复180秒→继续普通页90秒→接回USB并确认。倒计时阶段禁用继续按钮；确认时间与实际surface／亮度／PG／ADC分别存证，不能以点击证明物理动作。900秒是记录器预算，含准备／切页延迟；接USB后的样本不算纯BAT窗口。

回连只在操作者确认后开口，先核窗口所有权再stop／dump，不先改变PM。离线取消／超时不重开端口，主机资源关闭与设备停止确认分字段；若未确认停止，保存`recorder_recovery_required`及身份，设备至多运行至逻辑截止，但不把未观测的到期称为已清理。重新回连／恢复需依据新现场确认，不盲目start或stop其他窗口。

`records.json`保留全部记录，`analysis.json`仅选首次确认BAT至首次确认USB返回前的可信放电独立ADC，按普通／白屏／退出后时段分组。实验通过另需样本支持普通至少80秒、白屏至少80秒、恢复加末段普通至少260秒、普通亮度单值一致、白屏ack100、BAT段无OFF／未知PG且非FULL；允许10秒采样量化余量。采集完成与窗口通过分开，缺项异常退出，SOC／累计耗电测量始终false。首末跨度不证明连续采样，末段普通页是顺序恢复观察，不是独立、随机或等能量对照。

2026-10-21实际操作者完成11/11阶段并恢复常亮关闭。该次同BIN新数据110条／95个BAT独立ADC，原始坏页及有限重读保留；结果见[电量定因](BATTERY-ESTIMATE.md#guided-load-20261021)。蓝牙远控可另轮复用这套阶段／记录／回执结构，当前没有BLE测试导航或trace导出实现。

### 新固件烧录后的空记录器入口

生产显示修复后的验收不能把旧BIN的fixture身份带过烧录。`battery_experiment.py --flash-result <本轮成功烧录result.json>`与`--expected-trace`互斥；前者核对同一Product build／flash、COM端点、三镜像名称／地址／大小／哈希及全部29产物，仍不声称固件身份认证。旧磁盘记录在烧录前另行确认保留。

新入口只接受初始`session/state/count/start/duration=0`、schema1／cap256的记录器，保存空前态及哈希，并在单次start前再次核对。已有任何记录／运行／旧session均拒绝，不自动stop或覆盖；之后复用同一关闭串口、网页动作、回连所有权校验、stop／导出与取消清理路径。没有新增复位、PM或charger控制。窗口采集完成仍只说明记录可用，显示修复需另核当前生产C及持续1点差的实际触发；缺少触发条件时记缺证据，不冒充通过。
