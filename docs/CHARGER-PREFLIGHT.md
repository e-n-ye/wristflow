# 黄山派充电软件前置核验

本页保存契约、版本证据和未覆盖项；活动任务与唯一下一步只见 [STATUS 当前区](STATUS.md#current)。本轮基线为 PR #59 合入后的 `origin/main@e31c8c5`，工作树 `charger-preflight/wristflow`、分支 `codex/charger-preflight`。

## 2026-10-14 电芯参数与资料边界

用户补充中顺芯通用参数图片，结合此前实物标签 500mAh：最大充电 0.5C 即 250mA，20℃以下最大 0.2C 即 100mA；允许充电温度 10–45℃，单串充电 4.2V。图片还要求带保护板型号按较小的参数执行，特殊型号按单体资料；保护板参数和 ZCLP500 专用规格仍未核实。不能将 250mA 作为全部温度下的限值。

源图存于本机忽略目录 `artifacts/charger-preflight/battery-parameters.jpg`；完整来源哈希见[精简证据](evidence/2026-10-14/charger-preflight.json)。本轮未执行图片中关于新电池先充电的说明；图片提供参数，不是设备操作授权。

芯片资料采用厂商编制的 [AW32001E V1.4，2023-09](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1157/AW32001E.pdf)，已提取并查看寄存器页 35–38 的渲染。该资料对应 E 型，实装 IC 的完整型号和版本仍待现场核对：

| 项目 | 资料事实 | 当前用途 |
|---|---|---|
| 禁充 | REG01[3] CEB=1，复位值 0xAC | 默认禁充请求和回读；不从芯片复位默认值推断所有软复位状态 |
| 恒流 | REG02[5:0] 编码 `(code+1)×8mA`，默认 code=15 即 128mA | 修正 SDK `set_chg/8` 推算依据；特殊低电流模式须结合 REG0B，诊断保留原始字节 |
| 限压 | REG04[7:2] 为 `3600+code×15mV`，默认 4.2V | SDK 4215mV 请求与电芯 4.2V 区分；名义 4.2V 的电气表范围达 4.22V，后续须留误差余量 |
| 看门狗 | REG05[6:5] 控制充电芯片看门狗，默认 160 秒；超时可重置供电和多数寄存器 | 在确认禁充后关闭该看门狗；保留终止和安全计时设置 |

后续台架方案的保守候选为名义 4.17V、80mA；这是待验证请求值，不是本轮已经写入的参数或充电允许条件。真实充电还需确认温度保护接法、预充／终止、芯片误差、保护板上限及实测电流／电压。本轮没有充电使能接口。

## 当前实现范围

- `apps/product/src/aw32001_inhibit.c` 是可注入字节传输的禁充事务：设置 CEB=1并回读，再关闭芯片看门狗并回读，最后再次确认 CEB。最多七次传输，没有轮询或重试；不写充电电压／恒流，不存在 CEB=0 路径。失败停止，`inhibit_verified=false`；通信失效时物理禁充状态未知，不伪称硬件已关断。
- `product_charger.c` 以强符号覆盖锁定板的弱 `BSP_PowerUpCustom(bool)`，保留 PA38／PA26／PA30 和 500μs 上电顺序；入口以官方 `HAL_RAM_RET_CODE_SECT` 放 retained SRAM，普通 DEEP 的 false 路径只在 SRAM 判断并返回。冷启动 true 分支使用 HAL，早期不分配、不调用 RTOS 等待、不打印或断言。当前 standby 配置关闭；启用 `PM_STANDBY_ENABLE` 会编译报错，因为 true 路径的 I²C HAL／helper 仍在 Flash，尚不支持 standby 在 Flash 恢复前调用。
- Product 配置排除 SDK charger。原 `INIT_PREV` 启充、battery fault 的禁充后再启充、强制 resume 和 finsh 使能写入口随 AW32001 驱动排除；SDK 与子模块保持锁定版本，厂商源码不改。
- `wf_charger status` 只打印最近一次早期结果并通过 RT I²C2 锁手动读取原始寄存器，无后台活动、使能命令、虚构电量或温度。早期 verified 是历史回读，不能当作当前状态；屏幕关闭时共享电源／引脚可能让读取失败，失败明确返回。

HAL 传入 20ms 超时，且在已观察到 BUSY 时提前失败；锁定 SDK 另有固定 1000ms 的 BUSY 等待，检查后硬件状态变化仍可能进入它。不能声称整个 HAL 调用严格不超过 20ms；真实故障时限仍待验证。HAL_GetTick 使用 AON GTIMER，屏蔽中断不代表该时钟停止。

## 启动与恢复覆盖

| 路径 | 源码证据 | 验证边界 |
|---|---|---|
| Product 冷启动 | HAL_Init → BSP_IO_Init → BSP_Power_Up(true) →项目强钩子；随后 RT components 才进入 main | 最终链接和仅 USB 启动回读分别验收，应用 main 末尾设置不能替代此入口 |
| 普通 DEEP | `bf0_pm.c:1137` 使用 BSP_Power_Up(false) | 沿用现有 PSRAM／显示恢复；不在共享总线断电期间新增 I²C 写入 |
| standby | `bf0_pm.c:1385` 使用 true；早期 HAL_Init／MSP 还可在 Flash 初始化前调用 true | 当前明确不支持，配置关闭并加编译保护；需独立解决整个 RAM／ROM 调用链，不由入口属性代替 |
| 独立 bootloader | SF32LB52X 子项目 `butterflmicro/ram_v2` 的启动使用自身强 HAL_MspInit；未发现调用 BSP_IO_Init／BSP_Power_Up／AW32001 | Product 强钩子不进入该镜像；单独检查 map／反汇编。ROM、软复位遗留寄存器及前级供电窗口仍需实物证据 |
| USB 插入／错误 | SDK GPIO 事件本身只通知；原状态查询／resume 等启充入口被排除 | 芯片自主状态及复位行为须回读／台架核验，不能由应用代码推断 |

## 验证和未覆盖项

主机 `aw32001_inhibit` 覆盖原先启充／默认禁充／其他电源位、七个传输故障点、ACK 但写无效、末次 CEB 丢失、空参数。首次 Debug CTest 1/1 通过。它验证事务性质，不覆盖 HAL 时限、GPIO、电源或真实芯片。

Product 初次、BUSY 前置检查及 RAM 修正后的构建均退出 0；RAM 修正版 `20261014-190310-736` 的 391 份源码／29 个产物重哈希一致，主 BIN 7,715,624 B、SHA-256 `3c2985b4…`。配置排除 SDK charger，main.elf 的 `BSP_PowerUpCustom` 为 retained SRAM 项目强符号，原弱函数段被丢弃，原 AW 初始化／使能符号不存在。`tests/check_charger_layout.py` 通过修正版并拒绝首版 ELF；实际 false 路径反汇编另行核验。独立 bootloader 的强 HAL_MspInit 反汇编仅关闭 WDT 后返回，没有 BSP_IO_Init／BSP_Power_Up／BSP_PowerUpCustom 符号。准确命令、结果与镜像哈希归入精简证据。未运行云端基线，SDK／工具链／公共构建入口未改。

首候选 `3de2ea38…` 三镜像 write／verify 通过，18:47:38 的启动回读 early verified=1、REG01=0xAC、REG05=0x1A，PSRAM／手机校时和天气恢复。用户随后报告短按 KEY1 无法亮屏，拔 USB 后启动亮屏，再次按 KEY1 仍失败；因此页面／息屏回归不通过，本候选不能合入。首段 UART 在用户拔 USB 时于 18:49:54 以 SerialException 退出 1；息屏后没有 KEY 或断言日志，不能把串口安静单独当作故障根因。

主代理查到首版布局错误：原弱钩子 SRAM `0x20008748`，新覆盖却在 Flash `0x12099c10`；SDK 先调用钩子，随后才解除 NOR Flash DPD。即使 false 路径立即返回，入口取指也依赖休眠 Flash。失败 BIN／ELF／map 已保存。修正版入口 SRAM `0x200033BC`，false 路径只执行 SRAM 内比较、跳转和返回，没有函数调用或 Flash 跳板。

RAM 修正版三镜像 write／verify 与启动禁充位均正常，19:11:54 睡前只读快照为 REG01=0xAC、REG02=0x0F、REG04=0xA3、REG05=0x1A、REG06=0xC0、REG08=0x42、REG09=0x20、REG0B=0x43。REG09=0x20 是未诊断的原始输入故障值，不自动清错或重新启充。19:11:58 自然息屏后，用户报告“现在按key无法唤醒”；没有 KEY／恢复／断言日志，采集正常停止。因此 SRAM 修正没有解决全部故障，不能把首版布局错误当作完整根因；两候选均不得合入。

随后仅 USB 下恢复 PR #59 已验 PSRAM BIN `f7e3417b…`，旧工作树 flash 记录 `20261014-192014-655` write／verify 退出 0。19:30:01.458 记录 KEY1，约 308ms 后记录 screen on；用户确认“基线 KEY1 正常”。基线仍有 SDK 自动启充路径，电池必须保持断开。该恢复只证明一次 KEY1 对照正常，不能关闭旧无日志长待机故障。

源码独立核验没有发现 PA44 覆盖 KEY1：PA34 对应 HPAON pin10、CR2，PA44 对应 pin20、CR3，注册按 mask 修改并 OR WER；但裁剪 AW 驱动同时省掉 PA44／PMUC slot1／GPIO IRQ 初始化。普通 DEEP 在 WFI 后续行调用 `BSP_Power_Up(false)`，没有转入 true／HAL_Init。CEB 电源影响、早期时序和省掉的初始化仍是待区分假说，串口安静本身不能区分未唤醒、恢复卡死或日志链失效。

控制变量诊断使用独立 `charger-compare/wristflow`、`codex/charger-compare`，从同一 `e31c8c5` 保留完整基线，只加 `charger_compare.c` 的 `wf_ceb status|inhibit`：经已注册 RT I²C2 读取 REG01、OR CEB 后写入、精确回读，失败停止；没有 HAL 控制器重初始化、看门狗／电压／电流修改或启充命令。SDK 没有适用的现成 REG01 回读 shell 入口，原 `aw32001 status` 还可能触发故障重启充电，诊断不使用它。这个晚期手动入口仅用于对照，不能替代生产默认禁充或 H2 验收。

对照版 `3abe168d…` 的 388 源码／29 产物重哈希一致，与已验 PSRAM 基线只多上述诊断源文件；三镜像 write／verify 后，同次启动禁充前获“对照版 KEY1 正常”。19:44:41 REG01 从 0xA4 仅改为 0xAC 并精确回读；19:45:21 息屏后 KEY1 不亮。随后同版复位，用 `wf_pm hold` 禁止 DEEP 后再禁充：allow=0、enter/exit/deep_ticks 均零，KEY1 仍不亮，但息屏后串口 status／线程／堆查询实时响应，main 正常 suspend。因此原 SDK 初始化裁剪和 DEEP 都不是复现这一现象的必要条件。

无复位地用 `wf_pm wake` 取得亮屏日志，首个约十秒窗口用户没有看到，不能记作物理亮屏失败；之后在一分钟内有界发送八次、间隔八秒，用户确认“软件唤醒亮了”。亮屏期间重新回读 REG01=0xAC。屏幕关闭时一次手动 I²C 查询等待约五秒后失败，SDK 自行复位总线并发九个时钟；当时物理位未知，不能以睡前历史回读替代实时结果。候选事务仍无后台轮询，诊断不再次启充。

## KEY1 设计连接与仅电池验证决定

主代理独立获取[官方 V1.2 EPRO](https://downloads.sifli.com/hardware/files/documentation/ProPrj_%E7%AB%8B%E5%88%9B%C2%B7%E9%BB%84%E5%B1%B1%E6%B4%BESF32LB52%E5%BC%80%E5%8F%91%E6%9D%BFV1.2%28202504250924%29.epro)并抽查 GPIO 页 14 的 SW1 实例、符号端点及网络：按钮两个端点分别接 `PA_34_KEY1` 和 `VBAT`。相关 record ID／来源哈希见精简证据；这里只读取原设计，没有自制导出器或改硬件。POWER 页仍以 SY6103 符号和 AW32001E 兼容备注绘制，不能据此确认实装 IC。

AW32001E V1.4 第 16 页说明 CEB=1 的 OPSM 只启用 IN→SYS，CEB=0 才启用 Q2 充电路径。这与无电池时禁充后 VBAT 按键源可能缺少有效高电平的假说吻合；现场已证明串口响应和软件可亮屏，但没有测 VBAT／PA34 电平，不将设计推断写成实物根因。USB-only 无电池时 KEY1 当前确实不可用，不能继续沿用“禁充后 USB-only KEY1 必须正常”的未证假设。

第二次恢复 PSRAM 基线 write／verify 后，用户确认“恢复后 KEY1 正常”；在得知设计连接及证据边界后，明确选择“继续仅电池验证”。先在电池断开、仅 USB 下重新写入已有 RAM 修正版 `3c2985b4…`，三镜像 write／verify 与启动禁充回读通过；手动 REG01=0xAC、REG05=0x1A。末次 REG04=0xA7 与前序 0xA3 不同，符合 SDK 旧目标配置遗留，候选不改限压／恒流；REG09=0x08 只保留原始值，未诊断或重新启充。正常关闭采集后才指导拔 USB、按原 BAT 接法接回电池。

用户随后确认“仅电池：页面正常，KEY1 两次正常”，对应明确给出的启动／触摸／菜单→主页→天气五页→返回及两次自然息屏／KEY1，最后保持临时常亮。该同版有界用户观察通过；BAT-only 没有串口或电气测量，不能扩展为当时寄存器、实际 DEEP 驻留、BLE 重连、充电、电压／电流／温度或续航通过。初版布局缺陷和无电池 USB-only KEY1 现象分别保存，当前软件不改变按键供电连接；实物 VBAT／PA34 测量仍未进行。

本轮可交付范围为冷启动／普通 DEEP 的禁充请求与回读、只读诊断及上述供电条件下的页面／KEY1 结果。standby 不支持且编译阻断，通信失效不保证物理禁充。USB-only 无电池时 KEY1 不可用是当前明确限制，软件事件可亮屏；不能为恢复该按键而重新启充。接回 USB 前先断开电池，真实充电及限压／恒流／温度保护台架另开小闭环。

电池独立供电只取得本次页面／两次 KEY1 的有界观察；USB 与电池同接、真实充电、电机脉冲、功耗和续航均未验证。H2 全部条件仍未满足；PR 收尾状态见当前区和最终交接，不据此允许真实电池充电。
