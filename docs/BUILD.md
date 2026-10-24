# 可复现构建基线

## 2026-10-24 MMC5603NJ限时诊断

从`a76a6ca`主目录`codex/mmc5603-diagnostic`完成[地磁硬件跑通准备](MMC5603.md#diagnostic-20261024)，只加Product状态机／原worker集成及受影响测试；无SDK／工具链／公共构建入口、UI／XML变化。Debug及保留断言Release受影响传感器五项各5/5（0.23／0.29秒），随后补充迟DONE、慢I/O跨会话截止与反向有限诊断互斥，最终两项各2/2（0.16／0.23秒）通过，不重跑无关UI或Demo。主机GCC15.2.0／CMake4.2.3／Ninja1.13.2沿用缓存。

官方`pwsh -NoProfile -File scripts/Build.ps1 -Example product -Jobs 4`最终`20261024-155722-493`退出0，BIN8,028,648B／`5eb28e3c…`，545源码及29产物逐字节重哈希一致；Python3.13.15／SCons4.10.1／ArmGCC14.2.1及SDK／子模块锁不变且干净。旧自动亮度`7d7be017…`29产物在重建前ZIP逐项核验保全；Product增3,780B，无新线程／timer，worker仍6144B／main16384B。最终新地磁文件无新编译warning，既有FinSH转换／链接syscall／entry／RWX／缺dfu提示保留。首个成功构建后仅修正期限说明注释并补测试，被上述最终构建替代；未操作设备。

原始资料、渲染页、主机／编译及DryRun日志存主目录忽略的`artifacts/mmc5603-20261024/`，来源403／404、缺fitz改用现有pypdf／pypdfium2、元数据查询键名修正均记录在[精简证据](evidence/2026-10-24/mmc5603-diagnostic.json)。DryRun不打开端口；本轮软件结论不证明器件自检／XYZ／轴向／停止、电流或航向精度，下一实物窗口回STATUS。

## 2026-10-24 真实自动亮度

从`f5d0bc0`在主目录`codex/automatic-brightness`接入[真实自动亮度](PRODUCT-SETTINGS.md#automatic-brightness-20261024)：原LTR状态机／唯一worker增加持续生命周期，整数附录换算及产品曲线／滞回，UI运行值与手动值分离，v12仍24B保存模式。没有XML／生成资源变化，不重复编辑器导出。

主机GCC15.2.0／CMake4.2.3／Ninja1.13.2，完整Debug63/63（25.79秒）、保留断言Release相关11/11（2.88秒）通过；后续只补测试的独立v11迁移与长续航资格，Debug／Release各2/2（2.14／0.93秒）通过。首轮product_ui断言暴露无display timer的手电筒100被普通亮度覆盖，已修；创建期取值与代次切换重复恢复经只读审查定位修正，I/O内取消＋恢复次数断言通过。补充测试首次在页面过渡未结束就退出长续航而被拒，延长测试等待后通过，不归因为产品故障。失败原日志均保留；默认PATH的CMake3.31.4仅查询版本，正式构建复用缓存工具路径／4.2.3。

官方`pwsh -NoProfile -File scripts/Build.ps1 -Example <product|ui_demo> -Jobs 4`均退出0：Product `20261024-135448-177`／8,024,844B／`b351fb8f…`，UI Demo `20261024-135449-322`／7,762,768B／`c9707536…`；543／486源码与29／22产物逐字节哈希一致，SDK／子模块锁定干净，Python3.13.15／SCons4.10.1／ArmGCC14.2.1不变。保留原FinSH转换／RTC signedness／链接RWX、ftab syscall／entry等提示，没有为修告警改厂商树；Product相对页面版增加3,032B，worker仍6144B、main仍16384B，最终版设备栈观察见下文硬件证据。旧Product29／Demo22产物在重建前分别ZIP保全并逐项核对。

原始主机／编译／保全记录在主目录忽略的`artifacts/automatic-brightness-20261024/`。编译后Flash DryRun通过；用户随后另确认同板屏／原BAT＋USB与配合，实际下载／效果按本轮硬件证据记录，不能由本节成功编译推导。

**同轮遮光修正**：首候选实机持续有效0/1帧被比例守卫拒绝，错误超时关闭；按附录ELSE=0修正后，纯换算及真实worker连续12帧／序号／时间戳、真无新帧仍回退的Debug／Release各2/2通过。新断言最初要求恰好10%，忽略既定±2死区，已改为10–12范围，未放宽模式／新鲜度断言。该次官方Product `20261024-141600-439`／8,024,852B／`8e7d54c7…`及Demo `20261024-141604-163`／7,762,768B／`c9707536…`退出0，543／486源码与29／22产物字节匹配，SDK锁不变；相关新增代码无新告警。Product较页面版增3,040B。首候选543源码及29产物ZIP、Demo22 ZIP已完整核验保全，固件效果按版本分验；软件指纹及命令见[软件证据](evidence/2026-10-24/automatic-brightness.json)。

**随后摘要修正（本轮最终构建）**：`8e7d54c7…`的明暗／退出恢复及AOD短睡获有界通过，用户随后指出父页仍显示55%，该版未作普通reboot。543源码／29产物及旧软件证据已ZIP逐字节保全。新增父页断言在旧实现失败，仅修改`settings_view.c`的原行文案绑定后，返回、手动原百分比、父页停留时真实sample同步故障回退及字形检查的Debug／Release各1/1（2.02／1.05秒）通过。官方Product `20261024-143812-050`／8,024,868B／`7d7be017…`、Demo `20261024-143816-853`／7,762,784B／`01ed29c7…`均退出0，543／486源码、29／22产物逐字节核对，SDK／子模块锁定干净、双DryRun通过；与8e7保全源码比较，仅上述一处生产文件变化。无XML／生成输出变化，不重复编辑器导出或无关全量测试；最终Product较页面版增3,056B。

**后续设备收尾（独立于编译结论）**：最终7d7三镜像write＋verify退出0，父页摘要／55恢复与普通reboot保存获人工及UART有界通过；醒后自动有效、无故障。最终快照main栈16KiB／水印46%、worker6KiB／15%，串口释放后用户确认常亮关闭、末态正常。早前明暗／手电筒／AOD窗属于8e7；原始记录、版本边界与未验项见[硬件证据](evidence/2026-10-24/automatic-brightness-hardware.json)，无新增功耗结论。

## 2026-10-23 亮度设置模板复用

从`3e5125e`完成[亮度页面](PRODUCT-SETTINGS.md#brightness-page-20261023)，直接复用通用设置列表／单行模板，把旧设置行的开关抽为共同XML组件，原行API及默认隐藏保持；Product无自动模式提供者，未改采样、亮度策略或v11保存。最终相关Debug `ui_shell_lifecycle|ui_apps|settings_ui|endurance_ui|aod_ui`5/5（5.45秒）通过，不重复完整61项。主机GCC15.2.0／CMake4.2.3／Ninja1.13.2，命令与源码指纹见[证据](evidence/2026-10-23/brightness-page.json)。

官方Editor2.0.1 Community导出／预览编译成功，预览与固件均LVGL9.4.0。初版独立页面／中间组件复用候选被最终整页模板复用替代，日志保留；初次编译成功后出现空预览／No draw buffer，Close Project停在加载页，正常退出并重开恢复，根因未知。最终完整页面通过真实LVGL快照核对，四种渲染与收拢前逐像素相同；不要求每个设置子页有独立XML。

常规`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example <product|ui_demo> -Jobs 4`仍调用官方SCons。最终Product`20261023-223227-596`／BIN8,021,812B／`2a06be47…`及UI Demo`20261023-223340-327`／7,762,576B／`c6dbfd2e…`均退出0，540／484源码与29／22产物逐字节重哈希一致。SDK／子模块锁定干净，Python3.13.15／SCons4.10.1／ArmGCC14.2.1不变。既有FinSH及链接syscall／entry／RWX／缺dfu提示保留，最终受影响主机编译无warning；新官方`settings_switch_gen.c`保留末尾空行，默认diff检查仅报告此项，排除`blank-at-eof`后通过。旧已下载LTR版29产物先保全到忽略目录ZIP并逐项核验，原始日志／截图／来源附录存主目录`artifacts/brightness-page-20261023/`。本轮无设备操作；UI Demo旧设置入口未迁移，编译不等于新页实机可用或自动亮度接通。

## 2026-10-23 LTR-303限时诊断

用户要求开始LTR硬件跑通，先实现固定配置／双通道原值／有效性／停止恢复，复用已有传感器worker与I2C3，无UI或SDK改动。相关Debug `ltr303_diag|product_imu_diag`2/2通过；初版有限腕pending互斥断言失败已修复，手册合法旧RATE6／7误拒绝经复核修正。DATA部分失败释放锁、取消、恢复失败保留责任、总线准备失败及Flash耗时跨截止均定点覆盖，不重跑无关61项。

最初复用C树的Product`20261023-193919-504`退出0；用户随后要求迁回主目录，旧树全部归档并清理，见[恢复映射](handoffs/WORKTREE-MOVE-20261023.md)。D盘主目录常规`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 4`成功，记录`artifacts/product/20261023-204817-177/result.json`，BIN`da25db88…`，537源码／29产物字节哈希一致，SDK及子模块固定干净。迁回后相关2项通过，16KiB／overflow与retained钩子／DryRun复核成功；UI Demo源码未变，无重复构建。

新旧构建、测试与失败日志保留在主目录`artifacts/ltr303-20261023/`及完整旧树ZIP；[精简证据](evidence/2026-10-23/ltr303-diagnostic.json)绑定源码／构建／下载。后来新授权的同BIN独立条件原值及到期／stop恢复有界通过，结果回[LTR专题](LTR303.md)，不从编译推导lux或功耗。

## 2026-10-23 AOD A1设备反馈修正

从`55b73d7`在原隔离树`codex/aod-a1-hardware`修正样式应用返回／横滑和原尺寸缩略、亮度后进入Idle的SDK适配顺序、按样式异步电量请求及8%亮度。官方Editor2.0.1导出／预览编译与100%和真实LVGL检查完成；全页预览空白经正常重开同项目恢复，根因未知。运行既有Debug构建中的定点`ui_apps|settings_ui|aod_ui`3/3及`aod_display|product_power_service`2/2，不重复完整61项。亮度替身加入SDK DisplayOn副作用后旧代码失败、修正版通过；电量请求的取消／残留bit竞争已覆盖，独立只读复核无确定阻断。

继续复用下节隔离包装器／官方环境，`-Example product -Jobs 4`最终为`20261023-180424-440`／BIN`767e399c…`，535源码／29产物逐字节哈希一致；共享UI Demo最后为`20261023-162433-829`／`06895415…`，481源码／22产物一致，其后只有Product变化。两目标官方SCons退出0，SDK锁／子模块不变且厂商树干净。原三份新官方字体C带末尾空行，保留生成输出，排除`blank-at-eof`后的diff检查通过；既有编译警告保留。

四版下载及分版本有界设备结果见[精简证据](evidence/2026-10-23/aod-a1-hardware.json)；原A1完整29产物ZIP及顺序修正版三镜像已本地保全，选择版事后归档发现产物改写、保留失败与不完整ZIP，不声称完整归档。原始设备记录在主目录`artifacts/aod-a1-hardware-20261023/`，构建／预览／主机日志在隔离树`artifacts/aod-a1-20261023/`。最终电量／亮度已获用户确认；普通reboot和17:00–17:03自然时窗对应前一BIN，不扩大为最终版全量验收，无电流／续航结论。

## 2026-10-23 AOD A1设置与驻留（原软件轮）

从`b833666`在隔离工作树`C:/Users/13984/.codex/worktrees/aod-stack-regression/wristflow`完成[A1设置／独立显示状态](AOD.md#a1-20261023)。完整Debug61/61（38.62秒）、全库保留断言Release61/61（17.99秒）；ARM首次暴露回调`open()`与系统声明冲突，改名后受影响UI目标Debug／Release各3/3通过。主机工具为GCC15.2.0／CMake4.2.3／Ninja1.13.2。官方Editor2.0.1最终XML导出／预览编译与100%尺寸、实际LVGL渲染核对完成，固件与预览均为LVGL9.4.0。

本机隔离包装器`artifacts/aod-a1-20261023/Build-Isolated.ps1`复用主目录`Set-LocalEnvironment.ps1`的官方环境及锁，目标工程／源码／产物指向上述工作树；分别以`-Example product -Jobs 4`和`-Example ui_demo -Jobs 4`执行官方`scons --board=sf32lb52-lchspi-ulp -j4`。最终Product`20261023-145827-448`／BIN7,985,056B／`b476e97f…`及Demo`20261023-145937-074`／BIN7,731,224B／`cb608ecf…`均退出0、产物校验通过，532／478源码和29／22产物字节重哈希一致。Python3.13.15／SCons4.10.1／ArmGCC14.2.1及SDK／子模块锁不变且干净；后续在主目录使用常规`scripts/Build.ps1`入口。包装器及原始长日志仅存本机，不新增共享环境配置。

首次导出停滞及清单写入错误、导出清理与构建冲突、ARM命名冲突均保留失败日志／被替代产物；最终构建在导出完成后进行。保留既有FinSH转换、RTC signedness、LVGL及链接RWX等警告；六个新官方生成C带末尾空行，默认`git diff --check`仅报这六项，保持生成物原样，排除`blank-at-eof`后无空白错误。两路独立只读审查无确定阻断。完整哈希、命令、失败与未覆盖项见[软件证据](evidence/2026-10-23/aod-display.json)。A1无下载或设备I/O，主机／编译通过不证明新页触摸、真实保存恢复、面板分钟／时段切换、物理深睡或功耗。

## 2026-10-22 Product主线程栈溢出候选

同版原AOD固件经新授权一次RTS恢复后，普通KEY唤醒捕获`thread:main stack overflow`、8KiB／100%且SP低于栈底20B；本轮未运行AOD，原冻结／IMU故障不追认同因。[有界修复](AOD.md#2026-10-22-恢复捕获与主栈候选)仅将Product主栈8192→16384，保留优先级及overflow check，额外8192B SRAM分配预算。

`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 4`官方SCons退出0，Product `20261022-230404-769`／BIN7,973,872B／`e3bc2058…`；508源码及29产物字节重哈希通过，与旧508源码比较505字节同／2仅Git换行／1配置，SDK及子模块锁干净。生成rtconfig为16384／overflow check1；`Flash.ps1 -Example product -Port COM5 -DryRun`退出0，不打开设备。重建保留原FinSH转换、RTC signedness、LVGL null-dereference及链接RWX等警告，未修改SDK。原29产物先按哈希ZIP归档；纯栈配置无主机行为变更，不以无关主机用例代替栈峰值实测。后来新获下载授权，三镜像write＋verify退出0，用户集中触摸／导航／短睡恢复正常，main两份水印均16KiB／max50%、无新overflow；新版AOD未启动、长时稳定性仍未验。命令和完整指纹见[设备／候选证据](evidence/2026-10-22/aod-panel-hardware.json)。

## 2026-10-22 AOD A0面板诊断软件

从`bf43474`准备[原生Idle有界诊断](AOD.md)，正式AOD设置／保存／样式／显示状态未接入。GCC15.2.0／CMake4.2.3／Ninja1.13.2，完整Debug59/59（13.31秒）及全库保留断言Release59/59（6.07秒）；最后增加缓存有效位和恢复等待边界后，受影响诊断目标Debug／Release各1/1再次通过。独立审查发现的“恢复失败仍放行正常LCD写入”已改为独占恢复等待，定点复核无剩余确定阻断；主机不代表实际面板恢复。

`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 4`调用官方SCons，最终Product`20261022-213241-312`退出0／BIN7,973,872B／`971a955b…`，508源码及29产物逐项字节重哈希一致；较前轮设备BIN增2,852B。Python3.13.15／SCons4.10.1／ArmGCC14.2.1与SDK／子模块锁不变。初版及恢复修正版构建保留，最终版消除新增的wait参数signedness警告，并补跨恢复结束的KEY整组消费；相关Debug／Release各1/1及定点独立复核通过。保留原FinSH宏转换（含新命令同类）、RWX／ftab syscall／entry及既有配置提示。`Flash.ps1 -Example product -Port COM5 -DryRun`退出0，仅本地三镜像核验，无开口／复位／下载。没有共享UI／Demo依赖或XML资源变化，不重复编译Demo或官方GUI导出。完整命令／日志／指纹及未验项见[精简证据](evidence/2026-10-22/aod-panel-probe.json)。

## 2026-10-22 长续航息屏USB退出修复

初版同BIN验收发现黑屏拔USB15秒接回未退出。新增实际charger service→空ADC快照断言在旧服务失败复现；[修复](ENDURANCE.md)由charger发布独立资格化PG／低高接入代次及事件，shell进入时取最新无I/O基线，保留充电控制与ADC／估算路径。最终Debug58/58（24.46秒）、全库保留断言Release58/58（12.08秒）及两项独立只读复核通过，覆盖启动高、未知／状态INT、无ADC／无ACTIVE、合并末态低／未知和旧UI快照等边界。

原官方`Build.ps1 -Example product/ui_demo -Jobs 4`分别编译成功：Product`20261022-200833-351`／BIN7,971,020B／`ead3a735…`，Demo`20261022-200904-895`／7,722,600B／`5318707d…`，506／456源码与29／22产物字节重哈希一致；主BIN较初版增432B。SDK及子模块锁干净，Python3.13.15／SCons4.10.1／ArmGCC14.2.1与主机工具不变。既有FinSH、RWX／ftab syscall／entry和缺dfu提示保留；无XML／资源变更，未重复官方GUI导出。retained钩子仍在0x200033bc，假分支不调用Flash。先期成功编译被最终进入基线／合并末态补强版替代；[设备轮证据](evidence/2026-10-22/endurance-hardware.json)区分初版通过／失败、新版下载及USB／BLE补验，不混用版本。

## 2026-10-22 长续航模式软件轮

从`9836b21`实现[长续航模式](ENDURANCE.md)，v10／24B保存、KEY1三秒／新USB退出、保留计步与提醒、BLE连接／广播及腕暂停恢复。官方Pro2.0.1 Community经Ctrl+B导出并成功编译预览，LVGL9.4.0；极简页、电池页100%及实际LVGL滚动／中文／确认／退出环检查完成。新增字形检查修复“点”缺字；电池滚动容器前景继承已在XML显式修正并官方重导出，未手改生成C。

GCC15.2.0／CMake4.2.3／Ninja1.13.2，最终完整Debug58/58（24.27秒）、全库保留断言Release58/58（6.57秒）通过。Release先前一次未改动的product_motor段错误保留于独立日志；该目标定点连续10次及随后完整58项通过，未复现，不宣称已修复电机。两项只读复核提出的BLE屏障／回调交错和提醒合并重按／保存反馈问题已修正，定点复核无剩余确定阻断。详细失败与软件边界见[证据](evidence/2026-10-22/endurance.json)。

分别执行`pwsh -NoProfile -File scripts/Build.ps1 -Example product -Jobs 4`及`-Example ui_demo -Jobs 4`，最终Product`20261022-183427-113`／BIN7,970,588B／`27e02ab5…`，Demo`20261022-183529-561`／7,722,560B／`02004873…`均退出0；506／456源码与29／22产物逐项重哈希一致。Python3.13.15／SCons4.10.1／ArmGCC14.2.1、SDK及子模块锁定干净，保留既有FinSH／RWX／ftab syscall／entry与缺dfu提示。Product BIN较灰阶版增7,832B，协议队列每包增4B，无新字库／worker；未测运行时栈峰值。无串口／下载／设备操作，真实按键、无线、Flash重启及功耗另验。

## 2026-10-22 控制中心灰阶底色修正

从`main@2f6d819`进入同版上板验收，用户指出默认圆钮偏蓝，修正控制中心及轻量提示为中性灰；开启蓝色保留。只改本页XML、官方输出及运行时颜色，设置／业务逻辑不改。Pro2.0.1 Community GUI Ctrl+B成功，LVGL9.4.0，100%预览及真实LVGL关闭／开启／提示截图通过；既有`settings_ui`和`ui_navigation`两项Debug回归2/2（3.07秒）。

分别执行`pwsh -NoProfile -File scripts/Build.ps1 -Example product -Jobs 4`及同入口`-Example ui_demo -Jobs 4`，仍调用官方SCons；Product`20261022-151715-051`／BIN`7a313abc…`及Demo`20261022-151812-642`／BIN`528c6743…`均退出0，497／447源码及29／22产物字节重哈希一致，SDK锁保持。链接RWX与缺dfu等既有提示保留；[本轮证据](evidence/2026-10-22/control-center-hardware.json)区分初版`1ab8fd5d…`硬件第一组与本次配色固件的独立设备结论。

## 2026-10-22 控制中心与共用选时

从`ac74f27`实现[九格控制中心](CONTROL-CENTER.md)、定时勿扰与静音偏好，记录v9仍24B；按用户指出的重复选时，将原闹钟的双滚轮与确认按钮抽为`time_picker.xml`供闹钟／勿扰共同使用。Pro2.0.1 Community GUI末次Ctrl+B预览编译成功，LVGL9.4.0，100%检查及实际LVGL九图标／中文／`~100%`／选时与确认框通过。GCC15.2.0／CMake4.2.3／Ninja1.13.2，最终完整Debug55/55（24.89秒）、保留断言Release相关12/12（6.11秒，含闹钟）通过，定点独立复审无剩余阻断。

分别执行`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product/ui_demo -Jobs 4`调用官方SCons。最终Product`20261022-143831-791`退出0／BIN7,962,756B／`1ab8fd5d…`，Demo`20261022-144005-146`退出0／7,717,400B／`991a2889…`；497／447源码及29／22产物逐项重哈希一致。SDK锁／子模块固定干净，Python3.13.15／SCons4.10.1／ArmGCC14.2.1不变，既有FinSH、RWX、ftab syscall／entry及缺dfu警告保留。首轮旧滑条／DND测试假设、长按路由及图像名遮蔽均已修正；用户要求的共用组件后重新完成全部验证，较早成功版本不作为最终候选。完整命令、哈希、失败与未验项见[软件证据](evidence/2026-10-22/control-center.json)。本轮没有串口／下载／设备操作，真实触摸、Flash恢复与定时边界仍待同版验收。

## 2026-10-21 持续1点差显示停滞修复

从`17a9de5`在生产估算器加入可信放电的60秒／有序新ADC等待，保持IIR、曲线、采样、充电和PM；[算法契约](BATTERY-ESTIMATE.md#display-settling-20261021)区分历史基线与新生产回放。复用已配置主机环境，完整Debug55/55（23.24秒）、保留断言Release相关6/6（1.26秒）、独立CPython3.12.9电量工具86项（29.739秒）通过。两组旧生产分别120／109行匹配，新生产只改变预期百分比；缓存、模式未知、时间／序列回绕及支持中断边界覆盖。独立只读审查无剩余阻断。

`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product/ui_demo -Jobs 4`分别调用官方SCons。Product`20261021-230056-732`退出0／BIN7,805,836B／`8a35f2ec…`，Demo`20261021-230228-556`退出0／7,560,656B／`19a4b0a2…`；468／418源码及29／22产物逐项重哈希一致，Demo重编后BIN与上一版相同。Python3.13.15／SCons4.10.1／ArmGCC14.2.1及SDK锁／子模块固定干净。保留既有FinSH函数类型、RWX、缺dfu、ftab syscall／入口警告，所改估算器无新增警告；map中静态`battery_estimator`24B（此前12B），无新timer／worker／Flash或设备I/O。完整可执行命令、版本、警告和清单指纹见[软件证据](evidence/2026-10-21/battery-display-settling.json)。未开串口／下载，实机仍旧诊断BIN；编译和旧数据回放不抵扣新BIN硬件验收。

## 2026-10-21 电量限时RAM诊断（B1）

从`cd3f4aa`实现[可选估算诊断与Product有限记录](BATTERY-ESTIMATE.md#battery-trace-20261021)，不改ADC、曲线／滤波策略、充电控制、SDK或正常采样／PM。GCC15.2.0／CMake3.31.4／Ninja1.13.2，完整Debug55/55（25.56秒）、保留断言Release相关5/5（0.69秒）通过。独立审查指出超长间隔应先按锁内时间过期，修复后Debug／Release受影响各2/2（0.09／0.08秒）通过，定点复核无阻断；覆盖截止／容量、PG补全／去重、start前旧UI帧、跨tick／长间隔及导出时会话更换，控制台极值行小于128B。

官方`Build.ps1 -Example product/ui_demo -Jobs 4`分别调用SCons，最终Product`20261021-185828-667`退出0、BIN7,805,588B／`1d6df1a4…`，Demo`20261021-185858-943`退出0、BIN7,560,656B／`19a4b0a2…`；468／418源码及29／22产物逐项重哈希一致。Python3.13.15／SCons4.10.1／ArmGCC14.2.1及SDK锁不变，SDK／子模块干净。新增命令沿用FinSH签名转换警告，既有RWX／ftab syscall／entry和缺dfu警告保留；首版Product成功构建已由最终锁内时间修复版替代。ARM `trace`为14,352B静态RAM，诊断副本16B；没有新worker／timer／PM请求，未测运行期栈峰值或IRQ耗时。完整命令、清单／日志哈希和失败见[软件证据](evidence/2026-10-21/battery-trace.json)。本轮无串口／下载或设备结果，板上仍为旧`e5b9c342…`；RAM保留及负载恢复窗口归B2。

## 2026-10-21 主页环收尾与缓存诊断

从`17cf239`修复[整页零距离release漏提交](UI-NAVIGATION.md#carousel-scroll-state-20261021)，并准备只读`wf_ui status`取证。完整Debug54/54、全库保留断言的Release54/54通过。默认完整Release52/54的倒计时失败原样保留：既有`product_countdown_host`静态库的NDEBUG移除了stub RT_ASSERT中的调用；使用`cmake -S tests -B artifacts/host/native-wrist-wake-release '-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -UNDEBUG'`补核，不改公共CMake或倒计时源码。

本工作树直接调用`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example <product|ui_demo> -Jobs 4`，仍为官方SCons。Product`20261021-145119-846`／BIN7,803,048B／`e5b9c342…`，Demo`20261021-145234-419`／7,560,656B／`34b8c078…`均退出0、产物校验通过；464／418源码及29／22产物重哈希一致。SDK／子模块锁、Python3.13.15／SCons4.10.1／ArmGCC14.2.1不变，FinSH callback转换、RTC signedness及链接等警告保留。无串口／复位／烧录，新版触摸及完整单向故障根因未验；命令、哈希、失败和资源见[精简证据](evidence/2026-10-21/carousel-scroll-state.json)。

## 2026-10-20 INT1总路由诊断

从`583c719`在`D:/MY_Desk/project/wristflow-snapshot-hardware`的`codex/wrist-int1-route`增加[显式限时route／PA31读源前后电平](HARDWARE-CAPABILITIES.md#wrist-int1-route)，不改SDK锁、厂商或UI生成物。主机使用GCC15.2.0／CMake4.2.3／Ninja1.13.2，`WRISTFLOW_SDK_ROOT`指向主目录锁定SDK；最终Debug53/53（12.87秒）、Release相关5/5（0.17秒）通过。覆盖CTRL4借用／恢复失败责任、四个其他源起始非零／运行漂移／读取失败、GPIO无效、事件帧保留、期限／取消、status无I/O及计步共存。

沿用本机忽略的`artifacts/build-isolated.ps1`加载主目录已注册SDK环境，再以隔离目录的实际`Build.ps1`调用官方`scons --board=sf32lb52-lchspi-ulp -j4`。首版冷构建成功后补四路由守卫；最终增量Product`20261020-215940-822`退出0、产物校验通过，BIN7,799,628B／`17f1b775…`，461源码／29产物字节哈希一致，未变Demo418／22重核、隔离副本语义相同，未重复编译Demo。Python3.13.15／SCons4.10.1／ArmGCC14.2.1及SDK／子模块锁固定干净；最终增量保留9条`warning:`（3条既有FinSH转换、RWX及ftab的syscall／入口），另有既有缺dfu大写WARNING。腕对象text1894B、状态292B／缓存260B，worker仍6144B，未测运行时栈峰值或传输时长。完整命令、日志／源码／产物哈希和未验项见[软件证据](evidence/2026-10-20/wrist-int1-route.json)。本轮无串口、下载或硬件电平结果。

## 2026-10-20 腕部寄存器与XYZ快照

从`main@b1e16d1`为同一腕会话增加启用前／运行中／停止后新读帧，status仅打印同步缓存，无新worker／命令或检测算法。完整Debug53/53（14.16秒）及保留断言Release相关5/5（0.32秒）通过，含三阶段／传输失败／XYZ符号格式／bank保护、after-only遥测失败和帧中取消／到期；独立审查无剩余阻断。

`pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Example product -Jobs 4`调用官方SCons，Product`20261020-195623-368`退出0／产物校验通过，BIN7,798,612B／`df9c37d6…`，461源码／29产物字节哈希一致；未变Demo418源码／22产物重核一致，未重复编译。SDK／子模块锁固定干净，Python3.13.15／SCons4.10.1／ArmGCC14.2.1不变，增量保留原10条FinSH／链接等warning。腕模块text1446B，腕状态168B／缓存140B（较前增108B各一份），既有6144B worker栈未变；未测栈峰值或硬件传输时间。完整证据见[本轮记录](evidence/2026-10-20/wrist-register-snapshot.json)，本轮无串口／烧录／设备验证。

## 2026-10-20 原生腕部限时诊断

从`main@c7dd3c5`新增LSM6DS3TR-C内置AWT配置／锁存源诊断，复用唯一计步worker，不改SDK、UI、公共构建入口或设置格式。完整Debug53/53（13.73秒）及保留断言的Release相关5/5（0.10秒）通过；最终测试包含daily读取成功后腕准备失败的查找／配置双注入，审查无剩余阻断。

`pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Example product -Jobs 4`调用官方SCons，Product`20261020-190507-506`退出0／产物校验通过，BIN7,797,820B／`83c9bb50…`，461源码／29产物重哈希一致。UI Demo编入源码未变，复核旧`20261020-165557-466`的418源码／22产物一致，未重复编译。Python3.13.15／SCons4.10.1／ArmGCC14.2.1及SDK／子模块锁不变且干净；增量构建10条`warning:`，新命令额外触发一条已有FinSH类型转换警告，另保留已有链接／入口／syscall警告。腕寄存器模块ARM text1182B，腕状态60B＋缓存32B；不代表运行时峰值。完整命令、源码／产物哈希及资料核对见[精简证据](evidence/2026-10-20/native-wrist-tilt.json)。本轮无串口、烧录或设备效果结论。

## 2026-10-20 真实步数小时图

从`main@c0e5186`扩展同小时可确认区间及v2小时检查点，兼容旧v1总量，不改XML／官方输出、原生参数或SDK锁。Debug52/52（22.94秒）、Release相关5/5（1.29秒）通过；审查补强非零首样本与非零旧桶换日测试后，Debug／Release相关2/2再次通过。实际LVGL确认旧48小时未知、新5仅形成当前小时柱且总量53；源码审查无剩余阻断。

`Build.ps1 -Example product/ui_demo -Jobs 4`调用官方SCons，Product`20261020-165441-652`／BIN`2a1a39aa…`、UI Demo`20261020-165557-466`／BIN`c568c377…`均退出0；459／418源码全部字节相同、29／22产物重哈希一致，SDK及子模块锁定干净。Product14／Demo3条警告，所改文件仅原有RTC signedness两条，小时逻辑无新增警告。ARM符号尺寸与内存／未验边界见[小时图证据](evidence/2026-10-20/hourly-steps.json)；本轮未开串口／烧录，设备仍是旧BIN。

## 2026-10-19 今日步数检查点恢复

基于`main@0d67f92`，按用户每分钟有变化才保存的选择，新增同日部分总量双记录／CRC／写后回读及一次性恢复，不改原生计步参数、XML／官方输出或SDK锁。完整Debug52/52（22.74秒）、Release相关5/5（1.52秒）通过；最后消除新等待比较的符号类型警告后，实际worker Debug／Release各1/1通过，独立只读审查无剩余阻塞。

直接调用`Build.ps1 -Example product/ui_demo -Jobs 6`及官方SCons，最终Product`20261019-212437-661`／7,793,904B／`e7cca572…`、UI Demo`20261019-212258-850`／7,560,536B／`fe351303…`均退出0，459／418源码与29／22产物逐项重哈希一致，map确认恢复／存储接口，SDK／子模块锁定且干净。较早两次Product成功构建被cadence／类型修正替代，未抵扣最终版本。完整命令、哈希、边界和审查修正见[精简证据](evidence/2026-10-19/daily-steps-persistence.json)。本轮未开串口／烧录，实际Flash及重启恢复未验。

## 2026-10-19 原生今日步数接口

从`main@ba92169`接入运行期原生计数／UTC+8今日总量，复用唯一IMU worker及既有页面／组件，不改XML、官方输出、SDK或锁；保存与小时图后续。完整主机Debug51/51（22.10秒）、Release相关5/5（1.54秒）及两项独立只读审查通过；纯日累计、真实worker和真实LVGL绑定分别验证。

直接调用`Build.ps1 -Example product/ui_demo -Jobs 6`，Product`20261019-202037-031`退出0、7,791,648B／`4cd7c09b…`，457源码中443字节一致／14仅LF与CRLF不同；UI Demo`20261019-202351-168`退出0、7,560,536B／`cedcc981…`、416源码一致；29／22产物重哈希，SDK及子模块锁定干净，Product map确认新生产者／日模型／快照接口。原型首轮UI测试新增截图后未恢复滚动位置的测试假设失败已修正；证据含完整命令及哈希，见[计步接口](evidence/2026-10-19/native-daily-steps.json)。无新XML导出、串口、烧录或设备精度／功耗结论。

## 2026-10-19 统一活力指标页面

从`main@eabe3ba`新增官方XML活力长页／数据说明和五组件统一入口，独立显示契约仅供绑定，全天采集另轮。Pro2.0.1／LVGL9.4.0重新导出成功，100%页面及真实LVGL快照／字形／大数检查通过；完整Debug50/50、Release相关5/5及只读审查无阻断。

本工作树直接调用`pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Example product/ui_demo -Jobs 6`，底层仍官方SCons。Product`20261019-192724-164`／7,789,596B／`18db1984…`及UI Demo`20261019-192948-540`／7,560,536B／`dd64fd00…`均退出0，454／414源码、29／22产物重哈希一致；map包含新页及绑定，SDK／子模块固定干净。命令、版本、初期测试失败和完整哈希见[证据](evidence/2026-10-19/activity-page.json)。本轮无下载／串口／硬件验证，主机滚动不抵扣真实触摸验收。

## 2026-10-18 电池入口修订

按用户决定仅在Product菜单过滤SYSTEM，电池组件／设置路由和READY保持；XML／生成物／资源不变。真实指针设置入口、列表／三列无电池及已有组件／返回／生命周期相关Debug6/6（7.50秒）、Release Product UI1/1（0.80秒）通过；实际LVGL列表／三列画面检查和只读审查无阻断。

既有隔离包装器调用当前`Build.ps1 -Example product/ui_demo -Jobs 6`和官方SCons，Product `20261018-152731-871`／7,761,984B／`ce007a7e…`，UI Demo `20261018-152755-060`／7,535,936B／`8966481d…`，均退出0、419／383源码及29／22产物重哈希一致，SDK／子模块锁固定干净。完整命令／哈希见[入口修订证据](evidence/2026-10-18/battery-menu.json)；软件轮没有设备操作，先前BIN`7b4c5768…`的用户正常反馈单列在[原版硬件记录](evidence/2026-10-18/battery-page-hardware.json)。

同日PR #91合入`dfe26b1`后，无新源码／编译；419源码／29产物及锁／DryRun／retained钩子重核，用户确认断电池仅USB后COM5首次write+verify退出0。一次仅USB启动、禁充／估算无效、校时读回及allow恢复／自然息屏后15:41:15关闭串口；用户对菜单无电池、设置／组件进入及返回回复“均正常”。[同BIN下载／入口证据](evidence/2026-10-18/battery-menu-hardware.json)只关闭受影响路径，不补新SOC或电气结论。

## 2026-10-18 正式电池主页面

`codex/battery-page`基于`46a2c23`新增独立官方XML及80px数字字体，复用电源快照，不改SDK／充电控制。Pro2.0.1／LVGL9.4.0重新GUI导出与100%预览、真实LVGL七态／字形／`~100%`边界通过；Debug完整CTest45/45、Release相关7/7。首轮旧PLACEHOLDER断言失败，更新正式页READY预期后通过。

隔离包装器调用当前`Build.ps1 -Example product/ui_demo -Jobs 6`及官方SCons。Product `20261018-142914-525`退出0，7,761,984B／`7b4c5768…`，419源码／29产物；UI Demo `20261018-143009-422`退出0，7,535,928B／`f85674e0…`，383源码／22产物，均重哈希一致。实际编译清单和map包含新页面及字体，锁定SDK／子模块干净，工具版本沿用既有环境；完整版本／哈希／命令及失败见[页面证据](evidence/2026-10-18/battery-page.json)。软件轮没有设备操作或硬件验收。

同日软件PR #89合入`71fc716`后的硬件轮未重新构建：原Product419源码／29产物与锁／三镜像DryRun重新核对，用户确认断电池仅USB后COM5首次write+verify退出0。一次RTS仅USB启动、禁充／估算无效、校时与allow恢复／串口正常关闭有证据，实际页面反馈待验；[本版下载记录](evidence/2026-10-18/battery-page-hardware.json)不替代上段编译证据或完整UI／电气验收。

## 2026-10-18 电压估算电量

基于`d9cb294`新增纯C估算及原ADC采样身份／时间，统一现有显示与充电比例环；SDK锁、充电参数和公共构建入口不变。完整主机45/45（21.21秒）及保留断言Release相关6/6（1.11秒）通过。最后修正FULL样本失效后环仍全亮的问题，补实际像素回归，相关Debug1/1（0.83秒）／Release1/1（0.47秒）通过。官方Pro2.0.1 GUI导出／编译成功，LVGL9.4.0，100%预览及实际LVGL多比例／失效帧通过；两轮只读审查已关闭发现项。

隔离包装器调用官方`Build.ps1 -Example product/ui_demo -Jobs 6`及SCons。最终Product `20261018-133643-575`退出0，7,740,016B／`fc4d6703…`，415源码／29产物重哈希一致；UI Demo `20261018-133804-427`退出0，7,513,952B／`067caa99…`，379／22一致。Python3.13.15／SCons4.10.1／ArmGCC14.2.1，SDK及子模块固定干净；既有编译／链接警告保留。首个构建在审查发现环失效问题后主动终止，被最终版本替代；接触表转图脚本首次语法错误已修正。未运行云端基线，没有串口／烧录／设备验证。算法未用本电池校准，源码、编译与精度不可混为一项，详见[契约](BATTERY-ESTIMATE.md)及[精简证据](evidence/2026-10-18/battery-estimate.json)。

随后软件PR #86合入`42a29f5`，用户新确认断电池／仅电脑USB；相同BIN的415源码／29产物及SDK锁、三镜像DryRun复核通过，COM5首次write+verify退出0。仍仅USB的一次150ms RTS取得启动／禁充与无电池估算无效日志，RTC重新校时读回，allow恢复1并自然息屏，13:59:57正常关闭串口。硬件记录PR #87合入`ca25f42`后，用户回报BAT电量页`~91%／4.04V`、无表，换接失时后已连接同步；USB充电`~94%`、比例环／触摸返回／同次不重弹正常，普通页自然息屏KEY1后百分比几乎开屏恢复。记同BIN有界显示／交互通过，非独立定量时延、表测或SOC准确度；此接续没有新源码／编译／设备操作。[本版证据](evidence/2026-10-18/battery-estimate-hardware.json)区分烧录启动、显示、准确度和物理终止。

## 2026-10-17 充电画面软件闭环

从`e526a3f`新增充电周期缓存／邮箱、官方XML覆盖层与触屏／KEY1消费，充电控制参数与SDK锁不变。官方Pro 2.0.1 GUI导出／预览编译成功，日志为LVGL9.4.0，100%检查390×450；固件锁定源`lv_version.h`同为9.4.0。完整主机44/44（20.42秒），最后按住插入补测Debug1/1（0.66秒），Release保留断言相关3/3（0.47秒）通过；最终跨模块只读审查无新阻断。真实LVGL四态快照及字形／几何／原页／提醒／通知／呼吸确认检查通过。

隔离包装器继续调用官方`Build.ps1 -Example product/ui_demo -Jobs 6`及SCons。Product `20261017-220135-981`退出0，7,737,856B／`e9c07669…`，413源码／29产物重哈希一致；UI Demo `20261017-220513-489`退出0，7,513,664B／`d6422209…`，377源码／22产物一致。Python3.13.15／SCons4.10.1／ArmGCC14.2.1，SDK及两个子模块固定干净；原有工具链／SDK链接警告保留，未运行云端基线。本轮无串口／烧录／硬件验证。首轮GUI停滞及测试用例错误、完整哈希和命令见[契约](CHARGING-UI.md)与[精简证据](evidence/2026-10-17/charging-ui.json)。

## 2026-10-17 USB运行期输入事件修正

在共享资源版上修正PA44状态脉冲导致的“已停止／输入变化”：首边沿150ms合并后读PG，活动PG=1只监测，确认PG=0停充并重臂。完整主机43/43（11.31秒）通过后，审查提出的stale事件可能饿死监测已补强，最终相关Debug4/4（0.12秒）／Release4/4（0.10秒）通过；覆盖真实拔插状态序列、重复脉冲不续时、停止优先、pending保留、回绕、未知及故障锁止和持续边沿下原deadline停充。独立只读复核无剩余源码阻塞。

最终Product `20261017-204653-924`编译退出0，7,734,872B／`8a9f7bb6…`，408源码／29产物重哈希一致；未变UI Demo372／22一致，实际LCD优化调用仍进wrapper，强钩子仍在retained SRAM `0x200033bc`，三镜像DryRun退出0。前一`20261017-204557-937`在最后监测补强前编译，被替代且未下载。SDK／UI源码／公共构建入口不变，既有警告保留。用户重新确认断电池、仅电脑USB后，下载`20261017-205021-141`三镜像write+verify退出0；本次不再开串口／额外复位，用户对运行期拔插／息屏步骤回复“现象正常”，后续钳表回报约200mA／波动±3mA。完整充满、限压、温度及终止电流仍未验；本次仅增补反馈，无源码变化或新构建。完整记录及当前实机边界见[精简证据](evidence/2026-10-17/usb-auto-charge.json)和[事件契约](CHARGER-PREFLIGHT.md#usb-input-events-20261017)。

## 2026-10-17 充电与LCD共享资源保护

在入口VIN修正版上增加Product板级适配：通过目标局部的两个链接wrap保护LCD与charger共用电源／I2C2，SDK源码／锁及UI不变。完整主机43/43（18.75秒）、Release保留断言相关3/3（0.08秒）通过，覆盖lease下关屏不切rail／mux、LCD亮时不错误关电、部分配置／归还失败、worker失败持有责任及VIN资格；独立只读审查无源码阻塞。MSVC主机路径使用测试用符号别名，GNU路径实际链接wrap；MSVC路径未在本地执行。

最后将enable停止／截止检查放在共享供电准备之后，注入prepare阶段停止的Debug3/3（0.09秒）／Release3/3（0.12秒）及只读复核通过。最终Product `20261017-202959-849`编译退出0，7,734,688B／`da6a2872…`，408源码／29产物重哈希一致；UI Demo未变，旧372源码／22产物再次一致。最终ELF优化后的`lcd_hw_open.isra.0`／`lcd_hw_close.isra.0`真实调用均进入Product两个wrapper，三镜像DryRun／retained SRAM通过。前一`9b7595ce…`已仅USB下载并验证无电池拒充与自然息屏，但缺最后停止边界，不用于最终充电验收。SDK干净固定，保留已有FinSH／链接警告。

首次新板级测试把“检测输入前准备失败”误期待为会话UNCONFIRMED，改为显式会话准备失败用例后相关及全量通过；首次Product链接将带空格的单字符串LINKFLAGS当成文件而失败，已改为SCons列表并成功重建。首轮反汇编查询未含编译器`.isra.0`后缀未命中，按真实符号确认两个调用点通过；失败产物不具备下载资格。原`7d366c58…`真实启充后通信失败、用户已拔USB，当前新候选实机息屏回归仍待验证；见[共享资源契约](CHARGER-PREFLIGHT.md#charger-lcd-bus-20261017)及[精简证据](evidence/2026-10-17/usb-auto-charge.json)。

## 2026-10-17 启充前VIN历史故障资格确认

基于`origin/main@1b10441`，仅扩展自动入口的纯VIN历史锁存资格确认和日志，保持200mA／4170mV、单一worker、活动故障停充及SDK锁。相关Debug2/2（0.15秒）、Release保留断言2/2（0.12秒）通过：覆盖两次禁充等待、持续／新故障、PG／ADC／参数／I/O失效、逐笔取消、等待中停止／截止／输入边沿，以及最终enable拿锁边沿；无资格时38笔、资格时56笔事务均验证。只读Sol复核修正了新增路径测试计数缺口，最终37–56故障／取消及service锁+57均已补强。

Product `20261017-195201-254`编译退出0、7,733,728B／`7d366c58…`，406源码／29产物重新哈希一致；UI Demo源码未变，沿用`15bc9bf3…`并复核372源码／22产物一致。三镜像DryRun及retained SRAM位置检查通过；没有修改SDK／公共构建入口或UI，未重复全量42项或运行云端基线。编译保留既有FinSH函数指针、RWX／newlib桩及entry链接警告。现场原版故障与修正版下载／验证分别见[充电契约](CHARGER-PREFLIGHT.md#vin-entry-qualification-20261017)及[精简证据](evidence/2026-10-17/usb-auto-charge.json)，不能由编译通过认定已充电。

用户确认断电池仅USB后，两次sftool在stub连接阶段返回101、均无镜像verify完成；用户拔USB重接后第三次`20261017-195900-567`三镜像write+verify退出0。受控150ms RTS后固件／LCD／触摸初始化正常，无电池约177mV仍按电压保护禁充，REG02=18／PG=1；该次未触发历史VIN资格分支。RTC回到2000年后以当前UTC校时并读回，hold已恢复allow=1，串口正常关闭。没有手动start或实际电流／温度数据，接BAT＋USB验收待用户反馈。

## 2026-10-17 USB自动充电与状态反馈

基于`979174a`，新增项目PA44插拔检测，扩展同一charger worker及真实状态UI；SDK锁、厂商驱动、XML／生成物与公共构建入口不变。完整主机42/42（13.47秒）后，输入代次／计时保护及最终200mA增量均定点重验：最终Debug2/2（0.21秒）、Release保留断言2/2（0.16秒）；此前包含检测及真实LVGL的Release4/4（0.62秒）通过，200mA未改变UI源码。真实LVGL四态渲染和中文全字形已检查。只读审查发现并修正stop／拔线并发事件丢失与已接受手动请求丢失，后续200mA编码／写序／读回审查无阻塞。

最终200mA Product `20261017-184542-837`为7,733,568B／`d74f5c2a…`，406源码／29产物重新哈希一致；UI Demo `20261017-183338-648`为7,512,440B／`15bc9bf3…`，372源码／22产物重新哈希一致，两目标编译退出0。隔离包装器复用官方`Build.ps1 -Example product/ui_demo -Jobs 6`及SCons，ArmGCC14.2.1／Python3.13.15／SCons4.10.1，SDK固定干净。三镜像DryRun和retained SRAM位置检查通过。用户确认断电池／仅USB后，Product 三镜像write+verify退出0；受控150ms RTS启动读取成功，LCD／触摸初始化成功；无电池VBAT约178mV，自动会话按电压策略保持禁充。初轮UI旧预期失败及Release新目标因NDEBUG删除assert表达式而失败均已修正；所有本轮80mA BIN已被200mA候选替代。完整命令、哈希及未测边界见[本轮证据](evidence/2026-10-17/usb-auto-charge.json)，行为／现场条件见[充电契约](CHARGER-PREFLIGHT.md#usb-auto-charge-20261017)。

## 2026-10-17 当前提醒振动会话

从`ae68afd`接入闹钟／倒计时当前显示条目的后台节奏／所属ID取消，保留SDK锁、配置、XML／生成物和公共构建入口。完整CTest41/41（11.59秒）、Release保留断言的相关3/3（1.36秒）通过；主代理补停止／到期入口竞态后，最终motor Debug1/1与Release1/1各0.09秒通过。实际LVGL覆盖六源队列、同条合并／再次亮屏不补播、勿扰／关闭、重来复用身份、拒绝及生命周期取消；复用既有组件／字体，真实渲染检查通过。独立只读UI路由及后端因果审查的阻塞已修正并定点复核。

首轮主机39/41来自新UI用例将1秒倒计时只等32ms、误用控件名，修正后3/3及全量通过。进一步停止窗口注入曾1/1失败：step判断到期后、turn_off捕获停止序列前到来的stop被当自然到期；入口将未处理停止优先转取消，补强后Debug／Release通过。首Product虽编译成功，但缺此最后保护，保留旧BIN和清单、明确被替代且未下载。

最终Product `20261017-150950-854`为7,731,440B／`dd2a9c73…`，404源码／29产物重哈希一致；UI Demo `20261017-150605-884`为7,511,904B／`db451793…`，372源码／22产物一致，两目标均退出0。命令仍由隔离包装器调用`Build.ps1 -Example product/ui_demo -Jobs 6`及官方SCons，ArmGCC14.2.1／Python3.13.15／SCons4.10.1，SDK及子模块干净锁定；既有watch_core／SDK LVGL／product_services／FinSH及链接警告保留，未运行云端基线。三镜像DryRun通过；其后两次下载中断无verify完成，恢复写入三镜像verify退出0。启动禁充／默认OFF和显示恢复通过，标准／增强短提醒及关闭停止有用户确认；增强30组自停／保留画面、独立闹钟自然到期／关闭与关闭档仅屏幕也已分项确认；实际供电变化、未形成混合队列及其余边界在证据中单列。完整命令、哈希、失败与边界见[本轮证据](evidence/2026-10-17/reminder-vibration.json)。

## 2026-10-17 振动设置与有界体验

基于`32d3c93`，只改三档设置、v7／24B迁移及既有motor worker的预览两段／GAP和按ID取消；SDK锁、配置、XML／生成物与公共构建入口不变。完整CTest41/41（19.03秒）、补强旧格式默认及configure／提醒／销毁取消后的相关2/2（2.40秒）通过。Release保留断言，相关4/4（1.08秒）及最终补强2/2（1.05秒）通过；实际LVGL指针、字形、档位和底部滚动快照已检查，两个只读源码审查无阻塞。

隔离包装器调用现有`Build.ps1 -Example product/ui_demo -Jobs 6`和官方SCons，两目标均退出0。Product `20261017-141909-762`为7,729,888B／`ce4912c5…`，404源码／29产物重哈希一致；UI Demo `20261017-142351-373`为7,511,608B／`c3e56a4d…`，372源码／22产物一致。Arm GCC14.2.1／Python3.13.15／SCons4.10.1，SDK及子模块固定；既有watch_core枚举、LVGL潜在空指针、product_services符号比较、FinSH和链接／strip警告保留。未运行云端基线。

首次Release调用误用不存在的target，Ninja在编译前退出1，按真实target重跑通过；环境默认Python无Pillow／pyserial，分别复用内置图像环境和已安装SDK串口环境，没有安装依赖。三镜像DryRun通过后，用户确认断电池／仅电脑USB、人在旁；COM5 write／verify退出0，启动禁充、默认OFF及LDO3 EN0已回读。设置体验／关闭停止／普通重启恢复及显示息屏后hold软件亮屏已有用户确认；未验KEY1／物理DEEP。首段错过息屏窗的UART无回执及后续恢复保留，末段allow1、串口正常关闭。命令、完整哈希、失败与分层结果见[本轮证据](evidence/2026-10-17/vibration-settings.json)。

## 2026-10-16 电机LDO3供电前置

基于`585c988`扩展motor prepare及板级LDO3临时借用／精确归还，未改SDK、锁、配置、XML／生成物或公共构建入口。最终相关Debug2/2（0.10秒）／Release2/2（0.09秒）通过，Release保留断言；覆盖准备期间取消／到期、部分写失败、LOW与归还失败重试、原轨借用、SET冲突及其他rail保留。RGB启用编译拒绝与最长107B状态行检查通过，独立源码审查无阻塞。

隔离包装器调用现有`Build.ps1 -Example product -Jobs 6`及官方SCons，`20261016-185159-856`退出0；404源码／29产物重哈希一致，BIN7,728,496B／SHA-256`4df9a03d1dc4efbda10c2c35ad163b16534e21e657d29812c67e219114931acf`。UI Demo编入源码未变，沿用`20261015-195844-577`并重哈希372／22一致，未再编译／下载。SDK／工具版本不变，既有LVGL／watch_core／FinSH／链接警告保留，无本轮失败，不运行手动云端基线。下载与设备观察独立见[供电证据](evidence/2026-10-16/motor-supply.json)，不能由编译推断物理启停或电气余量。

## 2026-10-16 电机供电只读诊断

从`origin/main@af24263`新增Product板级`wf_motor_power`，单次读取PERI_LDO后报告两路LDO请求位；未改motor服务状态机／启停、SDK锁／厂商、XML／生成物、配置或公共构建入口。相关motor／board Debug2/2（0.12秒）、Release2/2（0.11秒）通过，Release保留断言；测试直接用锁定SDK的SF32LB52X pmuc.h，覆盖两路全部EN／PD／code、寄存器／GPIO无写、单次快照与打印期间变化、无效参数零读及128B行长。

初稿误将VOUT2标为LDO2，`20261016-161200-228`虽编译成功但未下载；SDK启动文档及官方DS5203原理图复核后改为两路并列读回，确认设计VOUT2来自LDO3。最终隔离包装器调用官方SCons的`Build.ps1 -Example product -Jobs 6`，`20261016-182526-656`退出0，404源码／29产物重哈希一致，BIN7,727,960B／SHA-256`a419023ad5fd40545d8b11e3b337e195241b863afce9492693ff50d39a3ecbff`；UI Demo编入源码未变，旧372／22重哈希一致，未重复编译。SDK／工具版本与下节相同，既有SDK／FinSH类型转换／链接警告保留，未运行手动云端基线。

用户再确认仅USB后，三镜像DryRun及write／verify退出0；启动禁充／默认OFF与两次LDO请求位回读通过，LDO3 EN=0；PM许可恢复、正常关闭。没有新的pulse、实际电压／电流或UI／KEY1验收。完整命令、清单、修正和硬件边界见[只读诊断证据](evidence/2026-10-16/motor-power-readonly.json)。

## 2026-10-16 手动限时电机软件

基于`origin/main@600e03b`，隔离`codex/motor-pulse`新增PA20默认OFF、显式1–250ms单脉冲、截止／取消、故障OFF与IDLE约束；不改SDK／锁、公共构建入口、XML／生成物或设置格式。完整主机CTest41/41（17.06秒）通过；补充`rthw.h`后相关Debug2/2（0.10秒）、Release2/2（0.09秒）再验通过，Release保留断言。覆盖截止／stop插入、部分ON失败、OFF失败及启动资源失败组合，GPIO测试核对LOW预装与锁存器语义。

本机隔离包装器调用当前`Build.ps1 -Example product -Jobs 6`，底层官方SCons，Arm GCC14.2.1／Python3.13.15／SCons4.10.1。最终Product `20261016-114657-656`退出0，404源码／29产物重哈希一致，BIN7,727,484B／SHA-256`cd88555bbc45aaec41093225c2091b2a8989dc8bb791e4a6e85e8da5e13f9ab5`。UI Demo编入源码未变，沿用`20261015-195844-577`并重哈希372源码／22产物一致。配置PM／ADC1开启、SDK charger／motor／vibrator与standby关闭，tick1000Hz。

首轮Product因缺少中断接口`rthw.h`退出1，补齐后重编译通过；独立审查发现启动补偿OFF失败未持有PM，已修正并补测、复核关闭。既有SDK／watch_core／RTC／FinSH／链接警告保留，新增MSH命令沿用SDK导出宏的类型转换警告。软件交付阶段未运行云端基线或操作设备；同日先沿用此BIN完成仅USB下载verify及[默认OFF启动](evidence/2026-10-16/motor-default-off.json)，该段未启振；随后又按明确授权发[一次20ms请求](evidence/2026-10-16/motor-first-pulse.json)，数字到期／stop后OFF有日志、实际振感未确认，两段均未新构建。具体编译命令／失败／哈希见[精简证据](evidence/2026-10-16/motor-pulse.json)，行为与物理时限边界见[电机契约](MOTOR-PULSE.md)。

## 2026-10-15 设置保存可靠性

基于`origin/main@12ca75a`，隔离`codex/settings-save`新增独立保存worker，偏好设置写后核验、按请求代次发布结果、每请求最多三次尝试，空闲事件无限等待；复用现有UI控件和中文字库反馈／手动重试。GCC15.2.0／CMake3.31.4／Ninja1.13.2全主机CTest39/39（19.03秒），保存服务Release1/1（0.11秒）通过；真实LVGL指针覆盖两条重试入口，三态快照及缺字检查通过。首次组件用例因旧自动重试预期失败，修改为终态停查／手动重试；随后字形检查发现缺“点”，复用现有notification_22后通过。

现有本机隔离包装器顺序调用当前`Build.ps1`的Product／UI Demo，底层仍为官方SCons `--board=sf32lb52-lchspi-ulp -j6`，Arm GCC14.2.1／Python3.13.15／SCons4.10.1。Product `20261015-195505-636`退出0、401源码／29产物重哈希一致，主BIN7,725,544B／SHA-256`310ad9fe4a986bd9f9a6d7ba795b7f349ce4acbdb7bbda4304176d1def971478`；UI Demo `20261015-195844-577`退出0、372源码／22产物一致，主BIN7,510,840B／SHA-256`b1b2e121615d686e9867630a675b7fe8a4ee934cbb44696bffdd53ea908134ff`。

SDK／锁／工具链、XML／生成物、记录格式及公共构建入口未改；既有watch_core／RTC signedness／SDK／FinSH／链接警告保留。未运行云端基线、未打开设备、未下载；不能由事件等待或编译推得Flash断电保证、实际唤醒／触摸或功耗数值。接口和边界见[保存契约](PRODUCT-SETTINGS.md#save-reliability-20261015)，准确命令与本机产物记录见[精简证据](evidence/2026-10-15/settings-save.json)。

## 2026-10-15 手动限时充电软件

从`origin/main@719d3e1`在隔离`codex/charger-manual`实现显式1–60秒启停／后台截止、参数／故障监测、停充未确认和共享ADC同步；不改SDK锁、厂商、公共构建入口、XML或生成物。GCC15.2.0／CMake3.31.4／Ninja1.13.2主机最终38/38（10.85秒），Release相关五目标5/5（0.40秒）通过；覆盖每笔启充传输失败、部分写副作用、stop／截止插入、计时回绕、终止配置／故障位、停止失败保留IDLE，以及拿锁期间到期／取消时不发enable。

复用本机隔离包装器调用当前`Build.ps1`，底层官方`scons --board=sf32lb52-lchspi-ulp -j6`，Arm GCC14.2.1／Python3.13.15／SCons4.10.1；最终Product `20261015-164027-232`退出0，399源码／29产物重哈希一致，BIN7,724,152B／SHA-256`81e594e33646d0fd1a56e2d4ee291ae72ffd420fe21075c3d35ca608840391d0`。UI Demo编入源码未变，旧`135438-944`的372源码／22产物重哈希一致，无重复构建。三镜像DryRun、强钩子SRAM`0x200033BC`及false路径直接到`0x2000345A`返回均通过；ADC1开启、SDK自动charger／standby仍关闭。

首轮一条测试计数断言失败已修正；独立审查指出的拿锁后启充窗口已关闭并补测；初次构建新signedness警告和末尾空行已处理后重新编译／核对。既有SDK／FinSH／链接警告保留。未运行云端基线，没有烧录、串口或启充；当前设备仍为`a9918aa2…`。完整命令与精简证据见[手动充电记录](evidence/2026-10-15/charger-manual.json)，接口与物理截止／温度边界见[充电契约](CHARGER-PREFLIGHT.md#manual-charge-20261015)。

## 2026-10-15 电源观测与显示

**异步修正版**：睡醒失败后将采样移入Product事件线程，首帧前只读缓存，ADC先于PG发布；36/36主机测试（10.52秒）及最后服务头文件补足后1/1通过，涵盖五秒慢读、UI无I/O、息屏不采样、事件合并、epoch迟到拒绝及IDLE配对。初次编译缺`rthw.h`声明已修正；Product `20261015-143554-331`退出0，396源码／29产物重哈希一致，主BIN7,721,336B／SHA-256 `a9918aa2ffe3e48f557d990083e86f53db38b2b4e81913d324cc869f2294d93e`。UI Demo编入源码未再变化，沿用下方`135438-944`并重哈希372源码／22产物一致，不重复编译。三镜像DryRun／retained SRAM检查再次通过，独立审查无阻塞；修正版硬件结果另记，不能把源码通过等同睡醒通过。

从 `0cfd857` 的隔离工作树 `power-status/wristflow` 接只读PG／VBAT ADC，Product显式开启ADC1，SDK charger仍排除；原禁充事务、SDK锁、XML／官方生成输出和公共构建入口不变。沿用本机隔离包装器调用当前 `Build.ps1`，底层仍为官方 `scons --board=sf32lb52-lchspi-ulp -j6`，Arm GCC14.2.1／Python3.13.15／SCons4.10.1。

第一版同步候选 Product `20261015-135417-382`、UI Demo `20261015-135438-944` 均退出0，395／372源码和29／22产物重哈希一致；主BIN为7,720,624／7,510,120B，Product SHA-256 `d4f17491699b5fcdebd98b91e5d041a61064a5413c12ef6672e2d3c834679648`。此前更早候选由本条替代；此版后来睡醒回归失败，正在用异步采样修正，不作为最终交付固件。没有重新安装或升级工具链。

GCC15.2.0／CMake3.31.4／Ninja1.13.2主机CTest35/35（17.64秒）通过，涵盖实际采样源的读失败／打开关闭、独立有效位、百分比演示兼容、已有业务回归；最后关于页滚动取景后Product UI额外1/1通过。实际LVGL渲染检查两种表盘、电量页及USB状态、关于供电行、控制中心单行文案；控制中心换行已修正并增加高度断言。链接检查确认强钩子在retained SRAM `0x200033BC`，DEEP false分支直接到 `0x2000345A` 栈恢复／返回，无Flash调用；SDK自动启充符号未链接。三镜像DryRun通过，不打开设备。

准确命令、精简结果、失败／修复与独立设备记录见[电源证据](evidence/2026-10-15/power-status.json)；成功编译不能替代真实采样、插拔／睡醒或电压表对照。未运行云端基线，不据此声明电压精度、充电电流、续航或功耗通过。

## 2026-10-14 Product 禁充参数准备

从 `54aac18` 的隔离工作树 `charger-limits/wristflow` 构建，Product 冷启动在禁充状态下请求名义 4170mV／80mA；SDK 锁、厂商、UI／生成物和公共构建入口不变。最终官方 SCons 增量构建 `20261014-211019-125` 退出 0，Arm GCC 14.2.1，391 份源码／29 个产物重哈希零差异，BIN 7,716,696 B、SHA-256 `0794fb9dbed939b0ad325db4db76a27d4fc8365ec15c20c9c1fb9cccd364c9dc`。较早 `20261014-210402-423` 后诊断文字变更，最终增量结果取代其交付身份。

GCC 15.2.0／Ninja Debug、Release 的 `aw32001_inhibit` 目标构建及 CTest 各 1/1 通过。链接布局检查通过，强钩子 retained SRAM `0x200033BC`，false 路径只到 SRAM `0x2000345A` 栈恢复／返回；SDK 自动启充入口排除，standby 关闭，PSRAM 私有堆保留。成功编译与 USB 启动／实物电流证据分开，准确命令、产物和后续验收见[本轮证据](evidence/2026-10-14/charger-limits.json)与[充电前置](CHARGER-PREFLIGHT.md)。未运行云端基线，未安排电池＋USB 同接或充电。

## 2026-10-14 Product 默认禁充与诊断

基线 `e31c8c5`，隔离工作树 `charger-preflight/wristflow`。项目强钩子替换板级默认启充，Product 配置排除 SDK charger；没有改厂商、SDK 锁、UI／生成物或公共构建入口。相关契约、硬件条件与精简证据见[充电前置](CHARGER-PREFLIGHT.md)。

忽略的本地环境适配器调用当前 `scripts/Build.ps1`，最终仍为官方 `scons --board=sf32lb52-lchspi-ulp -j6`；初次和 BUSY 检查后构建退出 0。首候选 `20261014-183434-670` 的钩子误放 Flash，后来改为 retained SRAM；RAM 修正版 `20261014-190310-736` 退出 0，Arm GCC 14.2.1，391 源码／29 产物重哈希一致，主 BIN 7,715,624 B、SHA-256 `3c2985b475467965763ac566f69a5cfb8e0e1d3cd627399727be24fa76e9d1c2`。配置／map／nm 确认项目强 BSP_PowerUpCustom 在 SRAM `0x200033BC`，SDK AW 初始化／使能入口排除；布局脚本通过修正版并拒绝首版 ELF，实际 false 路径无 Flash 调用。独立 bootloader 的 MSP 反汇编另存本机证据，不把 Product 覆盖误套到该镜像。

GCC 15.2.0／Ninja 主机目标 `aw32001_inhibit` 的最终 Debug、Release 构建与 CTest 各 1/1 通过，覆盖传输故障、回读失效和无启充写入。收尾已将该目标加入现有 `UNDEBUG`／超时列表，Release 断言保留；命令见精简证据。两候选当时仅 USB 的 KEY1 回归均失败，不能以构建通过代替设备验收；后续同一 RAM 修正版通过仅电池页面／两次 KEY1，有界结论及供电限制见充电前置。SDK 的固定 BUSY 等待不能用传入 20ms 冒充完整硬件时限，未执行充电台架或电机操作。

独立对照工作树 `charger-compare/wristflow` 从同一基线仅增加晚期 `wf_ceb` 诊断，保留 SDK 初始化与旧 SRAM 钩子；`20261014-193546-001` 构建退出 0，388 源码／29 产物重哈希一致，主 BIN `3abe168d…`。与已验 PSRAM 构建源码对比只多一份诊断源文件。仅 USB 取证复现 CEB=1 下 KEY1 不亮、IDLE 下串口响应及软件唤醒正常；该对照固件不作为生产禁充交付。重新写入 RAM 修正版前再次核对其 391 源码／29 产物，无变更／哈希偏差；没有为未变的固件重复构建。

## 2026-10-14 Product LVGL PSRAM 堆修复

基线 `a358830`，隔离工作树 `battery-freeze/wristflow` 的 Product [配置](../apps/product/project/proj.conf)启用锁定 SDK 已有的 LVGL PSRAM 私有堆，大小沿用该选项默认的 2048KiB；UI 缓存、XML／生成 C、SDK 锁和厂商源码不变。已有 SRAM 分配断言与本轮证据见[精简记录](evidence/2026-10-14/freeze-diagnostic.json)。

本机复用已注册环境，以忽略的 `artifacts/build-isolated.ps1 -Example product -Jobs 6` 调用本工作树当前 `scripts/Build.ps1`，最终仍为官方 `scons --board=sf32lb52-lchspi-ulp -j6`。Product `artifacts/product/20261014-162842-722/result.json` 退出 0，387 份源码和 29 个产物逐项重哈希一致；Arm GCC 14.2.1／SDK 与子模块锁保持，既有警告保留。没有重装环境、修改公共构建入口或运行不覆盖该 RT-Thread 分配路径的主机用例。

生成配置实际启用私有堆；新 map 把 2MiB 堆放在 `0x60000000`，framebuffer 紧接 `0x60200000`，区域终点 `0x60255b18` 低于锁定板型的 PSRAM 链接上限，未重叠。主 BIN 为 7,717,628 B，SHA-256 `f7e3417b6b867700e00b4840c5c34153065b3a2075b58b9b8ed8d96b96464b7f`。编译／链接不能代替物理容量、DEEP 保持和实际分配验证；本轮三镜像 write／verify 与启动已单独记录，组合页面及休眠回归仍按实际结果验收。

## 2026-10-13 通知归组与历史构建

实现锚点 `0ddcd89`，沿用元数据 `a94efc5` 与锁定 SDK。主机 CMake／Ninja 使用 UCRT64 GCC，完整 CTest 33/33（19.71 秒）；真实 LVGL 覆盖两人 A/B/C、历史排序、入口和阅读位置、手机单 ID 删除、本地整组删除及长正文无关更新。Pro 2.0.1／LVGL 9.4.0 初次 GUI 导出停在清理阶段，正常重开后一次 Ctrl+B 成功，输出 `Project compiled successfully`；实际固件 LVGL 同为 9.4.0。未手改生成 C 或替换 SDK。

顺序运行隔离包装器 `-Example product -Jobs 6`／`-Example ui_demo -Jobs 6`，底层仍为官方 `scons --board=sf32lb52-lchspi-ulp -j6`。Product `20261013-214151-357`、UI Demo `20261013-215003-613` 均退出 0，387／370 份源码与 29／22 个产物逐项重哈希一致，主 BIN 为 7,715,508／7,509,680 B。Product ROM／RAM 余量为 614,016／292,012 B，较前置元数据主 BIN 增加 1,544 B；链接余量不等于运行时 heap／栈峰值。SDK／子模块干净且锁定，Python 3.13.15／SCons 4.10.1／Arm GCC 14.2.1，既有警告保留。日志与图片在本轮隔离工作树的忽略 `artifacts/`，设备结果单独见[通知专题](NOTIFICATION-UI.md#notification-groups)。

## 2026-10-13 通知元数据保存构建

基线 `7212b23`，隔离分支 `codex/notification-history`，源码锚点 `a94efc5`。相关主机协议／队列／通知 UI／天气服务／天气 UI 5/5（1.89 秒），补原子性断言后协议 1/1（0.12 秒）通过。沿用现有隔离包装器，顺序执行 Product、UI Demo 的 `-Jobs 6`，官方 SCons 与 SDK／子模块／工具锁不变；未更换公共入口、重装环境、编译手机 APK 或触碰 XML／官方生成输出。

Product `20261013-195955-112`、UI Demo `20261013-200407-252` 均退出 0；383／366 份源码及 29／22 个产物逐项重哈希匹配，主 BIN 为 7,713,964／7,508,312 B。Product map 中 phone 与 notifications 分别增加 3,044 B，新诊断副本 480 B；含对齐的 `.bss` 增量 6,584 B，主程序 ROM／RAM 报告余量 615,615／292,012 B。Product 主快照仍在静态 SRAM，UI view 的 snapshot 容量增加 3,044 B，主栈预留仍 8,192 B；链接报告不是运行时 heap／栈峰值证明。完整版本、镜像与独立设备结果在[通知专题](NOTIFICATION-UI.md#notification-metadata-storage)记录。既有 SDK／FinSH／watch_core／链接警告保留。

## 2026-10-13 固件 AQI 接收显示构建

基线 `9b80a46`，隔离分支 `codex/weather-aqi`，固件源码锚点 `c9be245`。协议／队列／天气服务／实际 LVGL 主机 4/4 通过，补整数上界和非有限值断言后协议 1/1 通过；当前页与指数页的 126 实际渲染已检查。最终审阅仅修正 UI 测试的太阳页快照定位，UI 1/1（0.67 秒）重跑通过，固件源码未变。使用既有隔离包装器分别以 `-Example ui_demo -Jobs 6` 和 `-Example product -Jobs 6` 复用主目录锁定 SDK 与官方 `scons --board=sf32lb52-lchspi-ulp -j6`，没有修改公共构建入口或工具锁。

UI Demo `20261013-190844-717` 与 Product `20261013-191154-343` 均退出 0，383／366 份源码和 29／22 个产物逐项重哈希一致；主 BIN 分别为 Product 7,712,988 B、UI Demo 7,508,312 B。SDK／子模块仍锁定且干净，Python 3.13.15／SCons 4.10.1／Arm GCC 14.2.1；已有 SDK／FinSH／watch_core／链接警告保留。隔离工作树直接 Build 因 SDK 未初始化停在预检查；复用既有包装器后，并行环境导出曾触发 Product 的 WinError 5，改为完成 UI Demo 后顺序编译 Product 即通过，没有重装 SDK。完整镜像哈希、失败和硬件独立状态见 [精简证据](evidence/2026-10-13/weather-aqi.json)及[天气专题](WEATHER-UI.md#weather-aqi-display)。

## 2026-10-13 Gadgetbridge 测试包入口

本轮 Android 扩展与 Product 固件构建分开；方法见 [手机扩展](../integrations/gadgetbridge/README.md)，工具参数来源为[扩展锁](../integrations/gadgetbridge/upstream.lock.json)，不调整 `sdk.lock.json`。编译、测试、失败和真实投递以本轮证据记录为准，现有固件构建方法保留。构建脚本不安装 APK 或烧录。

最终 `20261013-163456-533` 构建退出 0（2m33s），辅助类 Robolectric 4/4、协议相关主机 2/2 通过；APK 0.94.0-wf-meta1／SHA `f844c676…` 的签名、独立包名和三项 provider authority 核对通过。锁定上游加补丁的临时干净 index 应用核对通过。AGP 自定义测试 task、网络 DNS／Robolectric 下载失败均已修正并重跑；测试运行使用已知的独立缓存下载配置，未改变上游依赖版本。命令、配置边界、完整哈希、手动安装及真实数据证据见 [精简记录](evidence/2026-10-13/notification-metadata.json)。没有更换固件工具链或公共构建入口，未新增 Product 编译／烧录或云端基线。

## 2026-10-12 呼吸圆环旋转修订构建

基线 `bba01b0`，隔离分支 `codex/breathing-usb`。沿用锁定 SDK／子模块、GCC 15.2.0／CMake 3.31.4／Ninja Debug，最终 CTest **32/32**（11.83 秒），包括旋转方向、三层一致、精确 360° 归一化及暂停／恢复。SDK Python 3.13.15／SCons 4.10.1／Arm GCC 14.2.1 的 Product 与 UI Demo 均退出 0；383／366 份源码及 29／22 个产物重哈希一致。Product 主 BIN 7,711,380 B，UI Demo 7,508,264 B；既有 SDK／其他文件及链接警告保留，旋转 view 没有编译警告。只改手写 runtime，XML／官方生成输出未变；实际 LVGL 动画、命令、完整镜像哈希及测试修复见 [BREATHING 第三轮](BREATHING.md#round3)与[旋转软件证据](evidence/2026-10-12/breathing-rotation.json)。编译与设备结果分开记录。

## 2026-10-12 呼吸训练第二轮构建

基线 `5e01d05`，隔离分支 `codex/breathing-product`。最终 GCC 15.2.0／CMake 3.31.4／Ninja Debug CTest **32/32** 和两个严格 core 用例通过，四张实际 LVGL 覆盖／恢复帧已留证。SDK Python 3.13.15／SCons 4.10.1／Arm GCC 14.2.1，Product 与 UI Demo 均退出 0、镜像清单校验通过，383／366 份源码及 29／22 个产物重哈希一致；SDK／子模块锁不变且干净，XML／官方生成输出未变。命令、v6 配置兼容、镜像哈希、失败修复与硬件未验范围见 [BREATHING 第二轮](BREATHING.md#round2)及[精简证据](evidence/2026-10-12/breathing-round2.json)。没有新增云端运行、串口、烧录或设备结论。

## 2026-10-11 呼吸训练第一轮构建

基线 `f250eb9`，隔离分支 `codex/breathing-ui`。Pro 2.0.1／固定 LVGL 9.4.0 官方 GUI 导出及五页 100% 预览通过；最终 CMake 3.31.4／GCC 15.2.0／Ninja Debug 主机 31/31 与严格 core 通过。Product `artifacts/product/20261011-222319-329/result.json`、UI Demo `artifacts/ui_demo/20261011-222515-154/result.json` 均退出 0、镜像校验通过；383／366 份源码及产物重哈希一致，官方清单 106 份 C 在两端实际编译数据库中全部匹配，字体链接核对通过。Product 主 BIN 7,710,436 B，UI Demo 7,507,392 B。完整哈希、复现方法、失败与最终布局覆盖关系见 [BREATHING](BREATHING.md#第一轮记录)和[精简证据](evidence/2026-10-11/breathing-round1.json)。本机复用已锁定 SDK，Product Base64 从官方 `SIFLI_SDK` 根解析；没有 SDK／子模块升级、手动云端运行、烧录或设备结论。

## 2026-10-11 闹钟列表末尾提示修订构建

沿用 `codex/alarm-usb` / `e5928d25`、既有隔离包装器和官方 SCons。两条提示移入闹钟滚动列表，官方 Pro 2.0.1 / LVGL 9.4.0 GUI 导出成功并保存 100% 预览；实际 LVGL 顶部及滚到底帧按 1:1 核对。受影响 UI 2/2、全主机 CTest 29/29 通过，最终日志 `artifacts/alarm/usb-round3/ctest-scroll-footer-final.log`。Product `artifacts/product/20261011-193101-667/result.json`、UI Demo `artifacts/ui_demo/20261011-193211-417/result.json` 均退出 0、产物校验通过；接续逐项重哈希 357 / 340 份源码及 29 / 22 个产物匹配，SDK / 子模块和工具锁未变。Product 主 BIN 7,663,276 B / `499ac4db1336d88928f9adaaa4e2fea406254b6c74067f2c71e5c1f4b8fb02ea`。本条覆盖下方容量修订版的当前布局证据，旧记录保留其版本；完整镜像与设备观察见 [ALARM 第三轮](ALARM.md#round3)及精简证据的 `scroll_footer_revision`，编译不代替真机验收。

## 2026-10-11 闹钟容量反馈修订构建

基线 `e5928d25`，隔离分支 `codex/alarm-usb`；满五条隐藏加号、独立底部上限文案和“只”字补齐，经官方 Pro 2.0.1 / LVGL 9.4.0 GUI 导出 / 编译成功。受影响 UI 2/2、全主机 CTest 29/29 通过，真实 LVGL 满容量及等待校时帧按 1:1 核对。Product `artifacts/product/20261011-190404-016/result.json` 与 UI Demo `artifacts/ui_demo/20261011-191004-850/result.json` 均退出 0、产物校验通过；357 / 340 份源码及 29 / 22 个产物逐项重哈希匹配，SDK、子模块和工具锁未变。主 BIN 分别为 7,663,068 B 和 7,460,024 B；完整镜像标识及修订版设备进度见 [ALARM 第三轮](ALARM.md#round3)。沿用既有隔离包装器 / 官方 SCons，没有新云端基线；编译结果不代替屏幕与设备验收。

## 2026-10-11 闹钟第二轮构建

基线 `269c37b`，隔离分支 `codex/alarm-product`；官方 Pro 2.0.1 / LVGL 9.4.0 GUI 导出成功。最终 GCC 15.2.0 / CMake 3.31.4 / Ninja Debug CTest **29/29** 和三个严格 core 用例通过，五张真实 LVGL 状态 / 提醒帧按 1:1 核对。Product 与 UI Demo 使用既有官方 Build / SCons 逻辑均退出 0、产物校验通过，357 / 340 个源和 29 / 22 个产物重哈希一致；SDK / 子模块和工具锁不变。命令、镜像哈希、GUI 再检查限制、失败修复和硬件未验范围见 [ALARM 第二轮](ALARM.md#round2)及[精简证据](evidence/2026-10-11/alarm-round2.json)。没有串口、烧录、设备输入或新云端基线。

## 2026-10-11 闹钟第一轮构建

基线 `654507f`，隔离分支 `codex/alarm-ui`；官方 Pro 2.0.1 / LVGL 9.4.0 GUI 导出成功。GCC 15.2.0 / CMake 3.31.4 / Ninja Debug 主机 CTest 24/24 通过，十二份真实 LVGL 渲染核对完成。UI Demo 与 Product 使用官方 `Build.ps1` / SCons 逻辑，SDK Python 3.13.15、SCons 4.10.1、Arm GCC 14.2.1，均退出 0、产物校验通过；SDK / 子模块与工具锁不变且 SDK 工作树干净。

UI Demo `artifacts/ui_demo/20261011-130622-286/result.json`，328 份源码 / 22 个产物重哈希一致，主 BIN 7,456,024 B / `0e75d0193536c4fc1dfcdbbda84c6b58993a9e74ab16d142ad6604fa16591bb4`；Product `artifacts/product/20261011-130956-683/result.json`，343 份源码 / 29 个产物重哈希一致，主 BIN 7,653,756 B / `bf33d61d547fc368765198a213e3eaf25bd60f5ab7c8b0886d81cd1ec4f4780d`。本机隔离环境复用与包装脚本、主机复现命令及已解决失败见 [ALARM 验证记录](ALARM.md#验证记录)，完整源 / 产物哈希见 [精简证据](evidence/2026-10-11/alarm-round1.json)。既有 SDK / `watch_core` / 链接告警保留，未运行手动云端基线；没有串口、烧录或新增硬件结论。

## 2026-10-09 D8 队列契约构建

最终验证源码 `codex/weather-d1@83a6bd2d8954b69e3e8536b0f249efb169381e7e`。`cmake -S tests -B artifacts/queue-d8-host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe`、`cmake --build artifacts/queue-d8-host --parallel 6` 和 CTest 全套退出 0，18/18 通过；随后补代次回绕/失败控制包断流断言并补官方头文件，最终受影响 `phone_queue` 1/1 通过。之后仅 Product 串口诊断拆行，主机所编译模块未改。测试直接编译生产队列函数并连接实际 parser；确定性交错的 IRQ/MQ shim 不等于目标板调度压力实测。

官方 `Build.ps1 -Example product -Jobs 6` 最终退出 0、产物检查通过，记录 `artifacts/product/20261009-123632-974/result.json`，312 份源码/29 个产物重哈希一致。主 BIN 7,620,916 B / `353978765b56c969daf42af853235d235a14dc8f247ab2209a5a35da09a7126f`。首次 `c1b3923` 构建因漏引入 `rthw.h` 而退出 2，保留 `artifacts/product/20261009-122938-783/result.json`；修复后重编，没有沿用失败产物。

首个成功 Product `055eb28` 构建记录 `artifacts/product/20261009-123127-970/result.json`；写入后发现新增 status 行最短 132 B，超过 SDK 128 B console buffer，截断了 stale。`83a6bd2` 将输出拆成 A1/stats/queue 三行，即使各项最大无符号数分别为 94/106/64 B；重新 Product 编译和写入，不改变队列/parser 行为。此前成功版是保留的诊断缺陷记录，不能替代最终版。

UI Demo 同命令退出 0，记录 `artifacts/ui_demo/20261009-122938-333/result.json`；299 份源码/22 个产物与最终工作树哈希一致，主 BIN 7,424,360 B / `b4a9f460b67cb1302e93544ba4d0b1acb4f3a4477b98b9ebc6bda025ac623c06`。后续改动只涉及 Product 队列及主机测试，未重复无影响的 Demo 编译。SDK、子模块、工具链、构建入口、XML/生成物/字体均未改，既有告警保留，未运行手动云端基线。设备结果和未覆盖项见 [D8 记录](BLE-FIRST-LINK.md#queue-d8)及[精简证据](evidence/2026-10-09/queue-d8.json)，编译不替代硬件验收。

## 2026-10-09 D3 配置安全构建

验证源码 `codex/weather-d1@03699cd322e53f14835cf1ef0a7057645a34ba30`。主机沿用下方 CMake/Ninja 命令，目录为 `artifacts/config-d3-host`；CTest 全套 17/17 通过。随后仅加强测试字形断言，排除 LVGL placeholder，受影响的 `component_editor` 1/1 通过，生产源码未再改动。四份主机配置缺项的实际 LVGL 渲染通过，XML/生成物/字体未改，无新 Pro 导出。

官方 `Build.ps1 -Example product -Jobs 6` 与 `-Example ui_demo -Jobs 6` 均退出 0、产物检查通过。记录分别为 `artifacts/product/20261009-000847-308/result.json`、`artifacts/ui_demo/20261009-000847-681/result.json`；310/299 个源码与 29/22 个产物重哈希一致。Product 主 BIN 7,620,676 B / `f2287798776dfde30e8903474b9e2e8e0ac6a571e3c088b169b1f4644b3ef742`，UI Demo 7,424,360 B / `b4a9f460b67cb1302e93544ba4d0b1acb4f3a4477b98b9ebc6bda025ac623c06`。SDK/子模块锁、工具链和公共构建入口未改，既有告警保留，未运行手动云端基线。源码/主机/编译与 USB 结果按 [D3 记录](COMPONENT-EDITOR.md#configuration-d3) 分开验收。

## 2026-10-08 D2 天气时效与请求构建

验证源码 `codex/weather-d1@767a67b51c1cd8d29e6df29a76d3d88f80c30688`。沿用下方 CMake/Ninja/CTest 命令，输出目录为 `artifacts/weather-d2-host`，17/17 通过；新增 `weather_service` 验证独立接收年龄、30 秒请求截止、三小时边界、RTC 跳变、重连代次与旧请求结果隔离。实际 LVGL 超时/过期重试快照、notification_22 逐字字形和按钮几何检查通过。XML/生成物未修改，未重新 Pro 导出。

`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 6` 与 `-Example ui_demo -Jobs 6` 均退出 0、产物校验通过。Product `artifacts/product/20261008-225818-563/result.json`，主 BIN 7,620,380 B / `f7fc54b527d6e4d57814e7065a060548b479d76356ce547b1df3869c13abc28d`；UI Demo `artifacts/ui_demo/20261008-225818-557/result.json`，7,424,064 B / `602372adec0c6cc9d93a8332a539bb7eb878972030c754759ff98817517b793d`。310/299 个源码、29/22 个产物重哈希一致，SDK/子模块锁和工具链未改；既有告警保留，未运行手动云端基线。USB 结果按 [D2 记录](WEATHER-UI.md#weather-d2) 独立验收，不沿用 v2 镜像。

## 2026-10-08 原版天气 v2 构建

验证源码 `codex/weather-d1@10331bf7eac9d51c5ee04d43b849ae3bd817d6cc`。使用下节同一 CMake/Ninja/CTest 命令，主机目录改为 `artifacts/weather-v2-host`，16/16 通过；Product/UI Demo 沿用官方 `Build.ps1 -Example <目标> -Jobs 6`，均编译及产物检查通过。310/299 个源、29/22 个产物重哈希一致；SDK/子模块与工具链锁不变，SDK 工作树干净。Product `artifacts/product/20261008-214449-096/result.json`，主 BIN 7,618,260 B / `033e45f2047e4977794c0aaf1a85007ab50ba5ad274cd8a0455f7d562f11df5d`；UI Demo `artifacts/ui_demo/20261008-214450-426/result.json`，7,422,896 B / `44f52f6b6439ebe92b905bf787bb097fe4db89c2229937f595d7ef48a1bbd041`。

新 Base64 单独链接锁定 SDK 的既有实现，使用项目精简配置，不启用 TLS 包。首次主机缺测试 shim 路径、SCons 列表原位追加导致外部源两种编译动作冲突，修复后以上最终构建通过；原日志保留。实际 LVGL 快照及最长风速单位的几何检查通过，XML/生成物未改，不计为新 Pro 导出。源码/构建/USB 验证分别见 [v2 记录](WEATHER-UI.md#weather-v2) 和 [精简证据](evidence/2026-10-08/weather-v2.json)；既有链接/FinSH 告警保留，不由编译推断真机或功耗通过。

## 2026-10-08 v1 高低温补丁构建

验证源码 `codex/weather-d1@5d286c7df5c5e0630b15413d78a9c9c79e043910`。沿用下节主机构建/CTest 及 Product/UI Demo 官方构建命令，主机 16/16 通过；Product `artifacts/product/20261008-205254-798/result.json` 和 UI Demo `artifacts/ui_demo/20261008-205255-538/result.json` 均退出 0、产物校验通过，307/299 个源及 29/22 个产物重哈希一致。主 BIN 分别 7,615,076 B / `824285cb5131de25643fe065bfc5f42263383b689f59b3fc3f475108334eb8dc`，7,422,360 B / `66c3d5375f1f10561d94f35255b8ad4964e5ecc157832317bf51110cc178184a`。SDK/子模块锁与工具版本不变，既有 FinSH/链接告警保留。

新 LVGL 快照 `artifacts/weather-d1-host/renders/weather_product_current.ppm` 显示高低温；使用 Codex bundled Python 的 Pillow 转换为 `weather_product_current_range.png` 并检查。旧同名 `.png` 不用于此次验收。仅补 v1 高低温及 Kelvin 哨兵检查，紫外线仍占位；编译不证明新增字段真机显示。现场、来源和下一实验见 [天气记录](WEATHER-UI.md#weather-v1-range)及 [精简证据](evidence/2026-10-08/weather-v1-range.json)。以下初版和 CRLF 记录保留对应源码身份。

## 2026-10-08 天气 D1 候选构建

工作目录 `D:/MY_Desk/project/wristflow`，D1 初版与主机/UI Demo 验证锚点为 `codex/weather-d1@3e1a9ce8c1d55bb9f837b2b7f04baf5792293cd3`；最终 Product 的 CRLF 修正版为 `02d4647`，见下方补构建与精简证据。SDK/子模块仍匹配锁且干净，使用 SDK Python 3.13.15、SCons 4.10.1、Arm GCC 14.2.1；主机 GCC 15.2.0、CMake 3.31.4、Ninja。

```powershell
cmake -S tests -B artifacts/weather-d1-host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe
cmake --build artifacts/weather-d1-host --parallel 6
ctest --test-dir artifacts/weather-d1-host --output-on-failure --timeout 60
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 6
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example ui_demo -Jobs 6
```

主机最终共享源码 16/16 通过，D1 初版两目标官方 SCons 和产物校验通过。初版 Product 记录 `artifacts/product/20261008-193403-027/result.json`，`main.bin` 7,614,884 B / SHA-256 `e0677ca60daacf57507633aee3d61f1e4950f907ffb5d657a14d1c4a2f6cbb8b`；UI Demo 记录 `artifacts/ui_demo/20261008-193403-861/result.json`，7,422,344 B / `d2b0f16d807b72ab2c46f302a9496d4b01a763bc7e44199021eebe305a638946`。分别 307/299 个工程源和 29/22 个记录产物在初版验证时逐文件重哈希一致；原 result 无项目 Git HEAD/dirty 字段，因此本节另行绑定验证源码，不推断构建当时的 Git 状态。此前 `20261008-192649-435` / `20261008-192815-611` 也成功；`193403` 记录是在 EOF 空行修正后重编译的初版，不是后续 CRLF 镜像。

失败保留：最初天气测试缺 `product_state.h`，构建日志 `C:/Users/13984/.fastctx/jobs/j-9cifgu/output.log`；随后 `product_ui` 仍断言旧样例 `30°/多云`，日志 `C:/Users/13984/.fastctx/jobs/j-yp7co3/output.log`。补 include 并改为无数据预期后通过；最后一笔 EOF 规范化前的差异空白检查曾非零，随后逐文件修正，最终必须再次检查。SDK/FinSH 与既有链接警告保留。两目标最终后台日志为 `j-xtr7bl` / `j-mf9nz7`；主机完整最后测试日志在 `artifacts/weather-d1-host/Testing/Temporary/LastTest.log`。

Product 编译清单/map/ELF 无 Demo provider、`mock_weather`；Demo 清单纳入 `weather_fixture.c`，当前无固件调用者、相应 factory 被链接剔除，专用主机测试实际使用。运行时覆盖生成 XML 的样例标签，所以仅搜索样例字符串不能判定运行时真假。烧录和真机反馈独立记录于[天气专题](WEATHER-UI.md#2026-10-08-d1-生产数据真实性修复候选)，本节编译不证明手机来源、线程安全、触摸或功耗。天气仍为待后续 D2/D3/D8 处理的候选。

用户手机日志触发 CRLF 窄修复后，沿用同一 Product 构建命令补编译成功：`artifacts/product/20261008-194850-771/result.json`，主 BIN 7,614,892 B / SHA-256 `ae291fa22acca8ff6a2a9e5af3434c8dc4007d9907a780cc9aac6f23039a6bcb`。307/307 源和 29/29 产物重哈希匹配，绑定最终代码 `02d4647`；文件哈希、三镜像清单与验证边界见[精简证据](evidence/2026-10-08/weather-d1.json)，不能仅绑定旧 `3e1a9ce`。UI Demo 与主机目标不编译该平台文件，其既有通过记录继续有效，无需为平台字符串改动重跑无关目标。`pwsh -NoProfile -File artifacts/weather-d1/20261008/Check-Uart-Lines.ps1` 退出 0，负例 121/121 malformed、新版 129/129 valid，输出在同目录 `uart-lines.log`。修正版 USB 三镜像写入/verify 和启动通过，真实天气回包未观察到，具体范围见天气专题。

## 2026-10-08 恢复历史构建记录

本轮没有新增构建。已找回 2026-10-04 Product / `7ec96322` 的 result.json、三镜像及写入/启动记录，现存镜像大小与哈希匹配旧 result.json；详见 [历史验收](PRODUCT-ACCEPTANCE.md#2026-10-04-联合验收尝试历史记录恢复) 和 [精简证据](evidence/2026-10-04/product-joint-attempt.json)。旧工作树及产物路径的归档定位见 [恢复记录](handoffs/WORKSPACE-RECOVERY.md)，不把旧构建算成本轮编译通过。

## 2026-10-05 天气 UI 增量

天气 UI 工作树 `C:/Users/13984/.codex/worktrees/weather-ui/wristflow` 基于 `9ce5b3e`，SDK 仍为 `421126d9f476ed8e2a6f0b0ca28a9f241c182e65`，未修改 SDK 或 `sdk.lock.json`。日升日落页已在 LVGL Pro Editor 2.0.1 Community / LVGL 9.4.0 中预览并以 `Ctrl+B` 导出；输出记录 `Project compiled successfully`。主机验证使用：

```powershell
cmake --build artifacts/weather-host-build --parallel 6
ctest --test-dir artifacts/weather-host-build --output-on-failure --timeout 60
```

两步均退出 0，CTest **16/16** 通过。`artifacts/weather-host-build/renders/` 保存 `weather_current.ppm`、`weather_current_sunny.ppm`、`weather_hourly.ppm`、`weather_daily.ppm`、`weather_indices.ppm` 和 `weather_sun.ppm` 六张 390×450 快照；`weather_sun.png` 为同一 LVGL 快照的预览。测试还检查贴图尺寸、ARGB8888、弧顶、虚线间隔、暗色延伸、地平线交点和太阳点关系。Product 复编译使用：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Example product -Jobs 6
```

该命令调用官方 `scons --board=sf32lb52-lchspi-ulp -j6`，退出 0，产物校验通过。最新机器记录为 `artifacts/product/20261005-141441-099/result.json`；`main.bin` 7,612,188 B，SHA-256 `ee2ec45da3a93f02d47259a067c83c8d65f1ec27e78eae77f6b830e8447198b6`，`hardware_verified=false`。完整页面、fixture、XML/Pro 导出范围、测试覆盖和剩余边界见 [天气 UI 增量](WEATHER-UI.md)。

烧录前用户确认 USB-only、未接电池，主机侧枚举到 `USB-SERIAL CH340 (COM5)`、VID:PID `1A86:7523`。本轮用 sftool 0.2.5 对 Product 三镜像执行 `write_flash --verify`，退出码 0；随后 1Mbps/8N1 启动采样确认 CO5300、FT6146 和 `display on`。证据见 `artifacts/flash/weather-20261005/evidence.json`。编译、烧录和启动不代替用户对天气曲线屏幕视觉/触摸的现场验收，也不代表 BLE、休眠电流、功耗或续航通过。真实 provider 尚未接入，天气同步、过期数据和网络异常策略仍待下一项有界实验。

## 2026-09-27 合入版本 Product 烧录复验

用户授权后，从合入 #28 的 `main 5c0b82c02cf8f80168b076b0f76e2514a1867042` 重新构建 Product。`pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product` 调用锁定环境的官方 `scons --board=sf32lb52-lchspi-ulp -j6`，退出 0、产物校验通过。记录为本工作树 `artifacts/product/20260927-225145-164/result.json`；286 个源文件及全部记录产物的哈希复核一致，SDK/子模块符合锁且无修改。主 BIN 为 7,427,780 B，SHA-256 `e3d78d1737c27bb1ca1110f40a2d438060c308efc26d3d88882493fdf097ebaa`。

确认 COM5 的 CH340、VID:PID `1A86:7523` 和位置 `1-1.1` 后，使用 sftool 0.2.5 按官方镜像清单独立写入三个镜像并 `--verify`，退出 0；不写 settings 分区。受控 RTS 复位采集确认布局 generation=26/pages=3、brightness=47、face=diffusion 恢复，BLE 广播启动、约十秒息屏及一次 KEY1 恢复。用户反馈其他区域似乎正常，但尚无逐项菜单手势矩阵。命令、镜像与日志位置见 [精简证据](evidence/2026-09-27/pr-merge-flash.json)。本次主机验证沿用同内容版本的 [成功完整基线](https://github.com/e-n-ye/wristflow/actions/runs/36323967702)，不重复计算为新的主机运行。

用户随后报告手机通知暂时收不到，Gadgetbridge 显示已连接；在未改源码、未再次烧录或请求复位的串口观察中自行恢复，用户确认收到 QQ、微信真实消息。日志捕获六次通知入库以及通知唤醒/再次息屏；23:18:38 完整状态采样为 connected=1、subscribed=1、generation=1、added=5、rejected=0、unknown=1、dropped=0，之后还有一条通知及两个未支持协议事件，不能将该采样称为最终计数。PM 路径计数继续增长。短暂中断未取得手机/设备同步故障日志，根因未确认，不归因于 PM 或认定代码缺陷已修复。串口于 23:19:42 正常关闭。若再现，先同步采集连接/订阅/计数和 Gadgetbridge 转发日志，再发送一条带时间标记的测试消息；本轮不扩展到长测、电流和续航验收。

## 2026-09-27 PR 收尾与 Windows 模拟器

第三次基线 [36320503029](https://github.com/e-n-ye/wristflow/actions/runs/36320503029) 对应 `04aacd64838f39c72eca54d8983307666062fdb5`。五个固件目标已通过，主机已编译完成，但 CTest 停在第六项 `component_layout`；本轮接手后取消该运行，不能把等待描述为仍在编译。原始日志保留于 `C:/Users/13984/.codex/worktrees/ble-first-link/wristflow/artifacts/ci-hang/original-cloud.log`。

诊断版 `92df443` 的 [36323218734](https://github.com/e-n-ye/wristflow/actions/runs/36323218734) 保留全部原断言，只把 MSVC 的 CRT 错误报告送到 stderr 并关闭交互式错误报告。MSVC 19.44.35229.0／Debug 编译成功，`component_layout` 在 0.01 秒内明确报出第 50 行 `generation == 123 && !memcmp(&layout, &next, sizeof layout)` 失败，其余 14 项通过，总测试时间 12.91 秒。原运行从 13:06:40Z 停留到 13:35:57Z 取消；诊断后不再挂起，证据上传成功。诊断日志为同工作树 `artifacts/ci-hang/diagnostic-cloud.log`。

根因是测试比较了 C 结构体的填充字节：字段相同不保证整个对象内存相同，MSVC Debug 下暴露该问题。最终测试逐一比较全部页面/槽位的声明字段，保留真实记录的逐字节校验、全部 512 个断电边界和代次回绕检查；新增 `0xa5` 填充的编解码回归，要求序列化记录与原记录完全一致。将比较临时退回 `memcmp` 的本机负例稳定在新回归处失败（退出码 3221226505），原源码不被负例改写；证据为 `artifacts/ci-hang/padding-negative.log`。未修改产品存储实现、SDK 或固件配置。

每项 CTest 现限时 60 秒，工作流把主机配置、编译和测试分开并放在五目标固件构建之前，测试步骤限时 5 分钟。修正后的 GCC 15.2.0／Ninja 本机命令 `cmake --build artifacts/host-pr-cleanup-win32 --parallel 6`、`ctest --test-dir artifacts/host-pr-cleanup-win32 --output-on-failure --timeout 60` 均退出 0、15/15 通过，日志为同目录 `local-build.log`／`local-ctest.log`。最终完整云端运行与对应提交由 [PR #28](https://github.com/e-n-ye/wristflow/pull/28) 记录，必须核对成功后再收尾；本轮不烧录，历史真机证据不扩展到新版本。

手动完整基线 [36317904582](https://github.com/e-n-ye/wristflow/actions/runs/36317904582) 对应 `91d85aea237a60a845d55bf9220b0c0c3ed27497`：锁定 SDK 安装与 Hello／BLE／Bringup／UI Demo／Product 五目标成功，MSVC 主机构建在 `ui/runtime/settings_view.c:42` 报 C2371，内部 `accept` 回调与 Windows SDK 的 Winsock `accept()` 声明冲突，尚未执行 CTest，不能记为基线通过。失败日志位于本工作树 `artifacts/pr-cleanup-ci-failure.log`，云端产物 `build-evidence-36317904582-1`。

收尾分支仅将回调及注册点改名为 `accept_settings_confirmation`，行为和测试断言不变；SDK、资源和工具链锁未改。`cmake --build artifacts/host-pm --parallel 6` 与 `ctest --test-dir artifacts/host-pm --output-on-failure` 均退出 0，15/15 主机测试通过。文档整合保留旧 PR 的独有历史资料并注明版本；本次没有烧录，旧 B1 真机结果仍只对应旧固件哈希。最新云端结果另行记录后才合并，编译不代替硬件验收。

第二次基线 [36318779481](https://github.com/e-n-ye/wristflow/actions/runs/36318779481) 对应 `7911b9eef74d8beb00c07cadf8b0c578d2ca1c3a`，五个固件目标和 UI 库编译成功，随后 `tests/component_editor.c:81` 的辅助函数 `select` 再与 Winsock 冲突；重命名为 `select_component`，不改断言或操作。日志为 `artifacts/pr-cleanup-ci-failure-2.log`。本机没有 Visual Studio 2022，MSVC 配置探测失败；改用已有 GCC 15.2.0／Ninja，在 `artifacts/host-pr-cleanup-win32` 启用 `-DWRISTFLOW_BUILD_SIMULATOR=ON` 验证完整 Windows 驱动构建，MSVC 最终结果仍以云端为准。

同一验证还暴露产品三列菜单横向拖动后的误点击：`product_ui` 在 `tests/product_ui.c:210` 进入系统页。产品菜单现在记录拖动并在列表/三列项目上启用按压锁，回归测试覆盖拖动后保持启动器；本机同配置 15/15 通过。该版本待新的云端基线确认。

2026-09-27 通知 UI B1 的主机 15/15、Product/UI Demo 编译和资源校验通过；新增中文字库要求 Product/UI Demo/主机使用 `LV_FONT_FMT_TXT_LARGE`。XML 对比度修正完成官方 GUI 完整导出、3/3 相关测试和两目标增量编译；随后按用户参考图改为列表内左滑删除，官方再导出、新增指针用例和两目标再编译通过。最终 286/276 个工程源哈希及全部产物逐项匹配，Product 主 BIN 7,427,628 B；修正版三镜像已独立写入/verify。命令、失败、最终镜像与硬件证据边界见 [通知 UI](NOTIFICATION-UI.md)。

2026-09-27 Product PM P1：同一 Product 开启 PM/Deep Sleep 并保留 BLE，新增运行时可回退 gate、AON KEY1 与后台事件入口；14/14 主机测试和 Product SCons 编译／产物校验通过。266 个源哈希匹配，SDK 锁与源码不变。版本、命令、镜像及硬件验证边界见 [P1 记录](PM-EVENT-WAKE.md)。

2026-09-27 Product BLE A1：13/13 主机测试通过，开启 BLE/CJSON 并加入官方 LCPU 后，Product SCons 编译与产物校验通过；本次未改 UI 资源、公共构建入口或 SDK。版本、命令、首次缺宏失败、最终镜像和硬件联调进度见 [BLE 首轮记录](BLE-FIRST-LINK.md)。下方早期记录只适用于其注明版本。

本页各节结果仅适用于注明的日期与源码版本。当前 [设置与显示状态](PRODUCT-SETTINGS.md) 已通过 12/12 主机测试、官方 XML 导出和 Product/UI Demo 编译；Product 后续两次烧录与真机发现见 [上板记录](evidence/2026-09-26/product-settings-hardware.md)。前一版 [反馈修复](PRODUCT-UI-FEEDBACK.md) 与 [组件编辑](COMPONENT-EDITOR.md) 的真机范围见 [历史烧录证据](evidence/2026-09-26/product-ui-flash.json)。当前状态见 [STATUS](STATUS.md)，历史记录不代表最新源码验收。

## 设置与显示增量（2026-09-26）

长睡唤醒与秒表退出修订版在同一工作树执行 `cmake --build artifacts/host-settings --parallel 6`、`ctest --test-dir artifacts/host-settings --output-on-failure`，12/12 通过。Editor 2.0.1 Community GUI 的 Export Code and Recompile 成功，预览 LVGL 9.4.0；确认文案新增字形并检查 390×450 实际 LVGL 快照。`pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product` 和 `-Example ui_demo` 均退出 0、产物校验通过，记录分别为 `artifacts/product/20260926-194625-304/result.json` 与 `artifacts/ui_demo/20260926-194945-740/result.json`；260/256 个工程源哈希与构建记录逐项一致。Product 主 BIN 3,390,952 B（SHA-256 `8a763b749fc9d3eaebc497d2051fbe3bf2fa7eba66b69f41e6168c8475ee78c7`），UI Demo 主 BIN 3,382,116 B（SHA-256 `948be5b2d3d06c79df593f378216eefdc43b3fe5d87e023c7fe151477e0bfef1`）。SDK/子模块仍锁定，构建脚本不烧录；Product 已另行写入/verify，真机交互结果见 [上板记录](evidence/2026-09-26/product-settings-hardware.md)。

工作树 `C:/Users/13984/.codex/worktrees/product-settings/wristflow`，分支 `codex/product-settings`，基于 `0edf7cc`。复用锁定 SDK junction 和原已注册环境；本机 `pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example product` / `ui_demo` 调用本工作树实际包装器，最终均执行 `scons --board=sf32lb52-lchspi-ulp -j6`。本机辅助脚本留在忽略的 artifacts；普通检出仍用公共 `scripts/Build.ps1`。

主机命令为 `cmake -S tests -B artifacts/host-settings -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe`、`cmake --build artifacts/host-settings -j6`、`ctest --test-dir artifacts/host-settings --output-on-failure`；12/12 通过。实际 LVGL 指针测试和 26 张 390×450 快照在 `artifacts/host-settings/`；图片与导出日志在本工作树 `artifacts/settings-evidence/`。Editor 2.0.1 Community GUI 导出成功，预览、主机及固件 LVGL 均 9.4.0。

Product 记录为 `artifacts/product/20260926-164447-048/result.json`，主 BIN 3,388,888 B；UI Demo 记录为 `artifacts/ui_demo/20260926-164732-999/result.json`，主 BIN 3,380,564 B；退出 0 且产物验证通过。源码哈希、产物哈希和版本见 [精简证据](evidence/2026-09-26/product-settings.json)。SDK/子模块、分区和公共构建入口未改，无烧录或手动云端运行。修复过 XML flex 属性、字体缺字和无对应字形的图标；既有 SDK 编译/链接警告保留，详见专题记录。

上板发现应用列表左缘返回和持续亮屏后 KEY1 唤醒问题后，Product 修复版通过原项目已安装的 SDK v2.5.1 环境直接执行官方 `scons --board=sf32lb52-lchspi-ulp -j6`，退出 0，主 BIN 3,388,992 B（SHA-256 `5b9b00eea16dd20fb47600b564bee633042fa10382e426aa63a7c586ae9f2d11`）。工作树缺本地 `.tools`，直接运行公共 `Build.ps1` 在环境加载前退出 1；未改公共脚本。修复版已烧录及校验，用户确认左缘返回、KEY1 黑屏唤醒和持续亮屏 5 分钟通过；其余交互未验。命令、原始日志、三镜像哈希和未覆盖项见 [上板记录](evidence/2026-09-26/product-settings-hardware.md)。

## 反馈修复与组件编辑（2026-09-26）

工作树 `C:/Users/13984/.codex/worktrees/product-ui-feedback/wristflow`，分支 `codex/product-ui-feedback`，基于未合入的产品入口提交 `feabc1d`。本机使用相同锁定 SDK 的只读 junction 与已注册环境，`artifacts/build-isolated.ps1 -Example product` / `ui_demo` 调用当前工作树的 `scripts/Build.ps1`，最终仍为官方 `scons --board=sf32lb52-lchspi-ulp -j6`；不烧录。

修复真机编辑页边缘触控后，Product 记录为 `artifacts/product/20260926-151911-153/result.json`，UI Demo 为 `artifacts/ui_demo/20260926-152137-458/result.json`；两项退出 0、产物校验通过、SDK 无改动。对应主 BIN 为 3,330,088 B 和 3,322,908 B，完整哈希、源清单摘要、版本与本机日志位置见 [精简证据](evidence/2026-09-26/product-ui-feedback.json)。最终源文件逐项与两份构建记录哈希一致；Product 修正版已再次烧录、校验和启动。

主机用 `cmake -S tests -B artifacts/host-feedback -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe`、`cmake --build artifacts/host-feedback -j6`、`ctest --test-dir artifacts/host-feedback --output-on-failure`；修正版 10/10 通过，额外覆盖左右 `+` 与垃圾桶的边缘坐标和靠近按钮的空白防误退。Editor 2.0.1 Community 的 GUI `Ctrl+B` 导出成功，预览/主机/固件均为 LVGL 9.4.0；本次触控修复未修改 XML 或生成 C。XML 格式、变更中文 UTF-8、生成清单及实际 SCons 编译来源已核对。SDK、分区与公共构建入口未改，未重跑手动云端基线。

## 手动 CI 基线（2026-09-25）

工作流只响应手动 `workflow_dispatch`，执行完整 SDK 安装、Hello/BLE/Bringup/UI Demo/Product 和 PC UI 测试。PR 与 push 不触发云端构建，也不要求 `Build baselines` 状态检查；日常使用本地命令和真机证据。历史四目标完整运行约 11 分钟，新增 Product 后的总耗时未测量。在 SDK、工具链、公共构建入口变更或阶段验收需要干净环境证据时手动启动，并记录运行对应的 HEAD、命令、产物和失败位置。

PR #18 调整触发策略时，在 `C:/Users/13984/.codex/worktrees/product-runtime/wristflow` 核对唯一事件为 `workflow_dispatch`，当时 Windows 完整构建步骤与 `40e7658` 一致；变更 Markdown 的 UTF-8/相对文件链接和 `git diff --check` 通过。GitHub API 删除 main 的 `required_status_checks` 后读回为 null，PR 要求、管理员约束和禁止强推/删除仍保留。没有为该次触发策略调整再次执行完整构建；下面的产品增量随后增加了手动 Product 构建步骤，尚未执行云端验证。

## USB 产品目标（2026-09-25）

产品目标使用与其他黄山派目标相同的 SDK 和工具链，入口为：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example product -Jobs 4
```

本机最新构建在隔离工作树 `C:/Users/13984/.codex/worktrees/product-runtime/wristflow` 完成，实际 SCons 命令为 `scons --board=sf32lb52-lchspi-ulp -j6`。记录位于 `artifacts/product/20260925-193809-281/`，`exit_code=0`、`artifact_validation_passed=true`，`main.bin` 为 3,081,600 字节，SHA-256 为 `2f0ea7e583f5d8efe265d8c305c7623ba057cc45a32b9f319d58e4e05c5f0a17`。配置打开板载 RTC 和 FlashDB FAL；BLE/PM 仍关闭。产品分区在 `apps/product/project/sf32lb52-lchspi-ulp_hcpu/ptab.json` 增加 `settings`（`0x12DA8000`/`0x4000`），DFU、BLE、文件系统和代码区保持原值。

产品主机测试与行为说明见 [产品固件框架](PRODUCT-RUNTIME.md)。构建阶段完成源码检查、8 项主机测试和交叉编译；随后在用户授权下完成 COM5 写入/校验、启动和 RTC 即时校时，亮度 27/简洁表盘跨 RTS 复位恢复已有日志与用户确认。RTS 复位后时间回到未校时，需单独核查。完整真机范围和剩余项目见该文档的 USB 真机首轮，不把构建成功扩大为全部功能通过。

## 触摸修正增量（2026-09-25）

6/6 主机测试、官方 XML GUI 导出及 UI Demo 本机 SCons 编译通过。14:55 使用独立烧录助手向已确认的 COM5 写入并校验成功，启动串口已有 UI 耗时样本；构建脚本未增加烧录。命令、固件哈希、调度证据、测量限制及剩余验收见 [触摸修正与调度核验](UI-TOUCH.md)。下方菜单/应用与早期记录按各自版本理解。

## 菜单与应用增量（2026-09-25）

白灰资源、蜂窝菜单、秒表、表盘选择和亮度/按键接入已完成官方 GUI 导出；6/6 主机测试及四目标本机固件编译通过。最终模拟器在本次隔离工作区 `artifacts/acceptance-build/`；命令、版本、固件哈希、失败修复和硬件边界见 [菜单与应用](UI-APPS.md)。本次不执行烧录。

## UI 框架增量（2026-09-24）

新增 `core/` 与 `ui/runtime/`，由 PC 测试和官方 SCons UI Demo 共用。`Build.ps1` 的 UI Demo 源哈希记录增加这两个目录，SDK 锁和其他构建目标不变。5 项主机测试已通过；本轮隔离工作区的命令、固件结果、失败记录与硬件边界见 [UI 运行框架](UI-RUNTIME.md)。下方历史结果仍按各自日期理解。

## 当前黄山派基线（2026-09-22）

用户已确认退 Nano 完成、黄山派已下单。`sdk.lock.json` 的默认板型已改为 `sf32lb52-lchspi-ulp`；SDK v2.5.1 及两个子模块提交与下方历史版本表一致，SDK 源码零修改。Hello、BLE、Bringup 在本机实际编译链接成功，包装器产物检查通过；没有硬件运行验证。

项目根目录的 PowerShell 7 命令保持不变：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example hello -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example ble -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example bringup -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example product -Jobs 4
```

底层均为官方 `scons --board=sf32lb52-lchspi-ulp -j4`。工作目录分别是 `vendor/SiFli-SDK/example/get-started/hello_world/rtt/project`、`vendor/SiFli-SDK/example/ble/peripheral/project`、`apps/bringup/project`。VS Code 三个任务及 CI 继续使用同一入口。

黄山派沿用 SDK 自带的旧版 `ptab.json`。各工程产物在 `build_sf32lb52-lchspi-ulp_hcpu/`，主固件为 `main.bin`，分区表为 `ftab/ftab.bin`，bootloader 为 `bootloader/bootloader.bin`，并有 ELF、map、HEX、配置及编译数据库。脚本读取 SDK 生成的 `sftool_param.json` 定位并检查所有镜像，不执行其烧录指令。旧 Nano 的 `output/main.bin` 和根 `ftab.bin` 布局也可由同一检查识别。

| 目标 | 本机记录目录 | main.bin 字节 | SHA-256 |
|---|---|---:|---|
| Hello | `artifacts/hello/20260922-124620-144/` | 300336 | `5d1e396f2e9cd6edd35f0586d2c30dbd7f3dd4cf0f42a5bcf17c9a976e4173c0` |
| BLE | `artifacts/ble/20260922-124628-615/` | 490064 | `09d17264a1822a45731664091785dea1b0a0324f5be2af46f32c154500d396d2` |
| Bringup | `artifacts/bringup/20260922-130142-290/` | 614176 | `deadadab94b802be43bbd3eeab9580de1828c28fafa679c9254b9334984895fb` |

三项 SCons 退出码均为 0，`artifact_validation_passed=true`；记录现在把编译退出码与产物校验结果分开，并给出 `firmware_path`。版本为本机 Python 3.13.15、SCons 4.10.1、Arm GNU 14.2.Rel1/GCC 14.2.1。版本、完整产物哈希和生成配置摘录已归档到 [机器可读证据](evidence/2026-09-22/huangshan-baselines.json)。原始构建日志保留在上表各目录。

包装器另外通过 PowerShell AST 解析并提取实际产物校验块执行：现有 Nano/黄山派两种清单均通过，缺失镜像与重复 `main.bin` 清单均被拒绝。负例保留在 `artifacts/manifest-validation-ef6a6da8179d4e5ebcaca9c1a571c9cc/`，未删除真实固件。JSON 配置、SDK 锁定提交及生成编译数据库路径另行检查。

生成配置确认三项均为黄山派、MPI1 mode 3、CO5300/FT6146、390x450、AW32001 充电、KEY1=PA34/高电平有效及板级分区。Hello/BLE 的 PM 关闭；Bringup 的 PM/Deep Sleep/LVGL v9 开启且 BLE 关闭；三项触摸唤醒关闭。仍未实现同一固件 UI + BLE + PM 联合验证。

本轮失败和处理：

1. `artifacts/hello/20260922-123247-160/`：SCons 已成功，但旧包装器只检查 `output/main.bin` 而失败。该历史 JSON 的 `exit_code=0` 只代表编译器成功，不能当包装器成功。现改为读取官方镜像清单，检查必需镜像唯一且所有列出文件存在、非空。
2. `artifacts/bringup/20260922-124728-694/`：链接报 `CODE_START_ADDR` 未定义。项目未启用官方 Hello 已使用的 `CUSTOM_MEM_MAP`，旧版分区头未进入链接预处理；现在 `Kconfig.proj` 按官方模式启用板级内存映射，不修改 SDK 分区。重编译通过。
3. 保留 SDK 的 RWX LOAD segment 警告、LVGL `lv_obj_tree.c:274` 潜在空指针警告，以及旧版 ftab 子工程的 newlib syscall/`entry` 警告和 `img "dfu" not found`。本轮未启用 DFU，也未抑制警告；上板启动、地址布局和运行稳定性仍待验证。

云端检查由本次分支 PR 的最新 `Firmware / Build baselines` 验证，不能用历史 Nano CI 或本机成功代替。没有烧录、触摸、BLE 连接、休眠电流或续航证据。

## XML UI Demo（2026-09-23）

新增独立 `apps/ui_demo/`，用于编译官方 Editor 2.0.1 导出的 LVGL 9.4.0 页面。运行 `pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example ui_demo -Jobs 4`，或 VS Code 的 `WristFlow: Build UI Demo`。构建生成 C、字体和图片，不需要运行时 XML 或 Pro CLI；XML 修改后须先 GUI 导出。

本机四目标编译与产物检查均通过；最新 UI Demo 主固件 3,025,908 字节，PM/BLE 关闭、`LV_USE_XML=0`、`LV_USE_OBJ_NAME=1`。生成源/资源哈希纳入 `artifacts/ui_demo/20260923-092758-687/result.json`。12 张 342×282 RGB565 小时图的未压缩像素数据约 2.21 MiB；旧的无引用 `hour_seven_data.c` 已从 SCons 图片源收集中排除。原三个目标哈希不变；CI 增加第四项生成 UI 编译，Bringup 验收代码没有改动。截图、失败处理、原始日志位置和哈希见 [UI 演示证据](UI-DEMO.md)。

## PC 交互模拟器（2026-09-23）

运行 `pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Simulate.ps1`，或任务 `WristFlow: Run PC Simulator`。主机使用固定 SDK 内的 LVGL 9.4.0 官方 Win32 驱动，与黄山派 UI Demo 共用 C 适配层；XML 仍只负责外观。`-BuildOnly` 仅构建并执行三项主机测试，不打开窗口，不烧录。依赖、操作与最终四项固件构建记录见 [PC 交互验收](SIMULATOR.md)。本轮 CI 增加 MSVC 主机编译与 CTest，结果以最新 PR 检查为准。

## Nano 历史基线（2026-09-21）

以下版本、直接 SCons 板名、目录与哈希是切换前的 Nano 记录；日常构建以上方黄山派入口为准，不把下方产物用于新板。

验证日期：2026-09-21，Windows x64 / PowerShell 7。两项均实际完成编译和链接，SDK 源码工作树保持干净。没有连接、枚举或刷写设备。

同日新增第三个目标 `bringup`，已实际完成编译和链接。它位于项目自有目录 `apps/bringup/`，开启显示/触摸与 PM，详情及独立证据见 [上板基础固件](BRINGUP.md)。本页的 Hello/BLE 配置和首次固件哈希仍作为原始基线。

## 版本

| 项目 | 本次固定/使用的版本 |
|---|---|
| SDK | v2.5.1 / `421126d9f476ed8e2a6f0b0ca28a9f241c182e65` |
| Bluetooth 子模块 | `4d081f8fc4bfc0199bdccc1bb36b9fdc40e7240c` |
| SiliconSchema 子模块 | `304658c6edd5497211b7d152932327ce6e4450ff` |
| 编译板型 | `sf32lb52-nano_n16r16`，SCons 解析为 `sf32lb52-nano_n16r16_hcpu` |
| 编译器 | Arm GNU Toolchain 14.2.Rel1，GCC 14.2.1 20241119 |
| Python / SCons | CPython 3.13.15 / SCons 4.10.1 |
| CMake / Ninja | 官方安装器锁定 3.30.2 / 1.13.2；本轮入口为 SCons |
| sftool / sdk-exe | 0.2.5 / 0.1.1；安装但未用于烧录 |
| Conan | 2.27.0，配置 `sdk.conan-config.v2.4` |
| 现有引导工具 | Git 2.42.0.windows.1 / uv 0.11.21 |

根 `sdk.lock.json` 和 SDK gitlink 固定源码。官方 `tools/locks/default/lock.json`、`pyproject.toml` 和 `uv.lock` 固定 SDK 工具/依赖。官方 Python 约束是 3.13 系列，本次解析到 3.13.15；后续从零安装应记录实际补丁版本。此处承诺可重建过程，不承诺跨日期、路径和机器逐字节相同的固件。

## 当前机器直接使用

在项目根目录打开 PowerShell 7，Git 需在 PATH 中；首次安装还需要 uv 和网络，日常构建复用已安装环境：

```powershell
Set-Location D:\MY_Desk\project\wristflow
# 仅首次安装或修复环境时运行；当前机器已安装
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Install-Sdk.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example hello -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example ble -Jobs 4
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example bringup -Jobs 4
```

工具已安装时无需每次重复安装。`Install-Sdk.ps1` 已实测复用现有环境；`Build.ps1` 已实测首次构建及 Hello/BLE 增量构建。脚本拒绝版本失配或有改动的 SDK，不会清理/覆盖用户改动。SDK 环境检查失败时先解决安装问题。

`Set-LocalEnvironment.ps1` 设置进程环境，工具、虚拟环境、uv 缓存放在 `.tools/`。构建激活从官方状态文件 `sifli-sdk-env.json` 读取此 checkout 的 default 环境，用其中已安装的 Python 依次执行官方 `tools/sdk_env.py check --profile default --offline` 和 `export --shell powershell --profile default --toolchain gcc --offline`，再加载其生成的 PowerShell 脚本。当前锁文件的兼容性仍由官方管理器检查；检查不通过即停止，不在构建中自动安装或联网修复。没有硬编码环境哈希或自行拼装编译器 PATH。

环境仅作用于当前进程，未永久改系统 PATH 或执行策略。安装仍调用官方 `install.ps1` 并使用已有 uv；首次官方 `uv python install` 也生成了用户目录下的便利入口，当前包装脚本已将后续 Python 入口限定到 `.tools/python-bin`。

包装脚本最终调用的官方命令：

```powershell
# 在项目根目录加载包装器，内部用已安装的 Python 离线检查并导出官方环境
. .\scripts\Set-LocalEnvironment.ps1
Enable-SdkEnvironment

# 基础程序
Push-Location .\vendor\SiFli-SDK\example\get-started\hello_world\rtt\project
scons --board=sf32lb52-nano_n16r16 -j4
Pop-Location

# BLE 外设
Push-Location .\vendor\SiFli-SDK\example\ble\peripheral\project
scons --board=sf32lb52-nano_n16r16 -j4
Pop-Location
```

这些命令只构建。生成的 `download.bat`、`uart_download.bat`、`sftool_param.json` 是产物，不能在未确认板卡、电源和端口时执行。

## 重新取得源码

项目的 `.gitmodules` 使用官方 GitHub 地址。已有项目 checkout 时执行：

```powershell
git submodule update --init --recursive
git -C vendor/SiFli-SDK rev-parse HEAD
git -C vendor/SiFli-SDK submodule status --recursive
```

项目现有公开仓库 `https://github.com/e-n-ye/wristflow` 和首次基线 `27ee4d1`；新电脑克隆方式见 [开发流程](../CONTRIBUTING.md)。初次获取 SDK 时本轮实际使用：

```powershell
git -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=30 clone --branch v2.5.1 --depth 1 https://gitee.com/SiFli/sifli-sdk.git vendor/SiFli-SDK
git -C vendor/SiFli-SDK submodule update --init --recursive --depth 1
```

仅在目标目录不存在时使用 clone 命令。GitHub 与官方 Gitee 镜像的 v2.5.1 标签已独立核对为同一提交。当前本地 SDK origin 是 Gitee，递归子模块也从 Gitee 取得；根项目对外记录的 URL 为 GitHub，提交一致。

## 产物与实际结果

每个示例 `project/build_sf32lb52-nano_n16r16_hcpu/` 下有：

- `main.elf`、`main.map`、`output/main.bin`、HEX。
- `bootloader/bootloader.elf`、`bootloader/output/bootloader.bin`。
- `ftab.bin`、`sftool_param.json` 和下载脚本。
- `.config`、`rtconfig.h`、编译数据库。
- BLE 额外包含官方 LCPU 子工程产物；这不构成任意自定义 LCPU 算法受支持的证明。

完整根路径：

```text
D:/MY_Desk/project/wristflow/vendor/SiFli-SDK/example/get-started/hello_world/rtt/project/build_sf32lb52-nano_n16r16_hcpu/
D:/MY_Desk/project/wristflow/vendor/SiFli-SDK/example/ble/peripheral/project/build_sf32lb52-nano_n16r16_hcpu/
```

| 示例 | 首次完整构建 | main.bin 大小 | SHA-256 |
|---|---|---:|---|
| Hello World | 退出码 0，链接成功 | 296832 B | `5d8e0fc8ce5e81fb2d4ab392786cd9176bd8855d2993d2e2b3d020732798ae7a` |
| BLE peripheral | 退出码 0，链接成功 | 486552 B | `9764d328867fad99989125b7e26bfa43cdfa291581a96c4acbfc9d9dbcef5d0f` |

初次完整构建证据保存在 [evidence/2026-09-21](evidence/2026-09-21)：两个 `*-first-build.json` 记录版本、命令、产物大小与哈希，`*.config` 为配置快照，`*-build.txt` 为实际完整 SCons 日志。最初记录器的 SCons 字段仅捕获了横幅，版本由官方 Python 锁文件与后续增量记录核实为 4.10.1；现已保存完整版本输出。

每次新构建还会生成 `artifacts/<hello|ble|bringup|ui_demo|product>/<时间>/build.log`、`result.json` 和配置副本；这些本机产物被 Git 忽略。`exit_code` 是 SCons 退出码，脚本还检查主 ELF、主 BIN、分区表和配置存在且非空，元数据命令失败会报错。记录中的 `project_sources` 保存工程入口和应用源文件哈希，UI Demo/Product 还记录 XML、字体/图片、共享核心与 UI 源码，产品目标另记录分区覆盖文件。增量构建复用未变文件是正常行为，不要求产物时间一定等于本次执行时间。

## 配置核验

| 配置 | Hello | BLE |
|---|---|---|
| CO5300 / FT6146 | 已启用并编译 | 已启用并编译 |
| 显示分辨率 | 390×450 | 390×450 |
| Bluetooth | 关闭 | 开启 |
| `BSP_USING_PM` / `RT_USING_PM` | **关闭** | **关闭** |
| `TOUCH_WAKEUP_SUPPORT` | 关闭 | 关闭 |

因此当前固件是官方基础功能构建基线，不是低功耗测量固件，也没有完成产品 UI 或 Gadgetbridge 协议。

源码依据（均在固定 SDK 提交）：

- `customer/boards/sf32lb52-nano_n16r16/hcpu/board.conf:31` 选择显示与触摸；`customer/boards/Kconfig_lcd:372` 进一步选择 CO5300、FT6146；同文件 557/611 行给出 390/450。
- `customer/peripherals/touch_panel/ft6146/ft6146.c:252` 创建 100ms 软定时器，初始化时启动，`:337` 的 `deinit` 停止并删除；真实熄屏流程是否走到 deinit、是否需要触摸唤醒需实测。
- `customer/peripherals/display/co5300/co5300.c:328` 的 DisplayOff 仅发送关显示命令；`:497` 把百分比写入亮度寄存器。视觉亮度和实际屏幕电源状态另验。
- `customer/boards/sf32lb52-nano_base/bsp_lcd_tp.c:20` 提供显示 reset/VADD 关断路径；TP PowerDown 拉 reset，不据此假设触控电源轨断电。
- `customer/boards/sf32lb52-nano_n16r16/ptab.yaml:64` 的 `psram_data` 默认只有 8MB；板配置 `BSP_QSPI1_MEM_SIZE=16`。实际容量测试与应用分区预算是两件事，暂不改分区。
- `docs/source/en/app_note/low_power.md:1096` 明确写着 `LCPU code is not open for modification on 52 series.`；BLE 构建生成 LCPU 镜像并不消除此文档边界，仍不预设可部署自定义传感器算法。

支持范围：Hello README 声明 any board，BLE 英文 README 声明 all platforms，其命令示范为另一板型；本次补充了 Nano 的实际编译证据。`example/pm/AMOLED` 官方列 Core N16R16，`gui_pm` 列黄山派，LVGL watch 列表未明确 Nano。本轮没有将这些示例的文档支持或其他板测量当作 Nano 成功运行。

## 问题及处理

1. GitHub clone 停在约 29MB。取消后保留目录 `vendor/SiFli-SDK.github-partial/`，从官方 Gitee 镜像完成同一提交及子模块下载；未执行删除。
2. 官方中国镜像预设的 Python 元数据地址报 `tls handshake eof`。关闭整组预设，保留官方 GitHub assets 镜像，Python 使用 uv 默认路径；安装成功。失败日志 `artifacts/logs/install.log`，成功日志 `install-default.log`。
3. Conan 配置包在 assets 镜像未取得，官方脚本回退 `downloads.sifli.com/dl/sifli-sdk` 后成功。
4. 链接器对 bootloader/main（BLE 另含 LCPU）发出 `LOAD segment with RWX permissions` 警告。构建成功但未隐藏警告；本轮未修改官方链接脚本，也未验证运行时内存保护。
5. 没有硬件，烧录、串口、触摸、显示、BLE、休眠和功耗均未验证。后续已新增 GUI + PM 的 Bringup 构建证据，但仍无其运行证据或 GUI + BLE + PM 联合固件。
6. VS Code 两个任务曾在激活阶段访问 `https://pypi.org/simple/rich/` 时遇到 `tls handshake eof`，尚未进入 SCons。原因是官方 `export.ps1` 每次执行 `uv run --with rich --with tomli_w`，会临时解析依赖。现由项目包装器直接使用已安装的 SDK Python 调用官方环境管理器，SDK 源码不变。修复后按 VS Code 任务相同命令，设置 `UV_OFFLINE=1`、`HTTP_PROXY`/`HTTPS_PROXY`/`ALL_PROXY=http://127.0.0.1:9`、空 `NO_PROXY`，Hello 和 BLE 增量构建均退出 0，BIN 哈希与上表一致。证据分别在 `artifacts/hello/20260921-213529-493/` 和 `artifacts/ble/20260921-213556-601/`；这是离线参数加不可用代理的验证，未物理断网。另用不存在的工具状态目录验证了缺失环境会立即报安装提示，没有改动真实安装。测试环境变量仅设置在测试进程中。

## 工作流范围

旧项目 `.github/workflows/ci.yml` 有 F411 固件、host CTest、模拟器 smoke、manifest 签名测试和汇总 Gate。它们依赖 STM32 链接布局、F411 产品目录和旧 LVGL，不复制到此项目。

当前采用本地同一构建入口、版本校验、实际产物检查、配置/日志/哈希留档。GitHub 工作流调用 Hello、BLE、Bringup 和 UI Demo 四个构建入口；云端结果以实际 Actions 记录为准，不能把本地构建称作 CI 已通过。出现独立业务逻辑时再加入相应 host tests。流程与套餐限制见 [开发流程](../CONTRIBUTING.md)。

2026-09-21 首次全新 Windows runner 已实际完成官方安装和三项编译，全部通过。云端配置与基线一致，BIN 哈希存在跨机器差异；详细版本、产物与运行链接见 [工作流首轮验证](WORKFLOW-EVIDENCE.md)。

## 2026-09-25 页面栈增量

隔离工作区 `C:/Users/13984/.codex/worktrees/ui-shell/wristflow` 的 `codex/page-stack-lifecycle` 使用现有主机构建：`cmake --build artifacts/acceptance-build --parallel 6`、`ctest --test-dir artifacts/acceptance-build --output-on-failure`，最终 6/6 通过。`pwsh -NoProfile -ExecutionPolicy Bypass -File artifacts/build-isolated.ps1 -Example ui_demo` 完成官方 SCons UI Demo 编译，记录 `artifacts/ui_demo/20260925-153021-450/result.json`，主 BIN 为 3,072,252 字节、SHA-256 `e841ab797bb80c8c088134f0eac06a9c88c133a16e4e5617503ddb12d32a4c71`。未改构建入口、SDK 锁或公共配置；Hello/BLE/Bringup 沿用基线并由本次 PR 云端工作流复编译。

构建与烧录仍分开。独立助手先校验源文件、镜像、地址和 COM5 CH340 身份，再按此前上板授权写入并 verify；上板证据与用户反馈边界见 [页面栈验证](UI-NAVIGATION.md)。编译和自动启动采样不代替真机交互验收。
