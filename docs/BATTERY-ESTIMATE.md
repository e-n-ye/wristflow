# 电压估算电量

本轮授权为[共识](BASELINE.md#battery-estimate-20261018)中的首版估算百分比及现有页面／充电环接入。活动状态和唯一下一步只维护于[STATUS](STATUS.md#current)。本页保存算法、来源与验收契约，不创建第二份队列。

## 数据来源与精度边界

沿用固定SDK `421126d9f476ed8e2a6f0b0ca28a9f241c182e65` 的黄山派 `customer/boards/sf32lb52-lchspi-ulp_base/battery_table.c` 充放电表，每5%取一个参考点，共21点／表；相邻点线性插值。表中41808对应4.1808V，保留0.1mV尺度：同SDK `docs/source/en/middleware/battery_calculator.md` 的示例明确写`38000 // 3.8V`，并将`bat1`通道7读数原样传入计算器。结构／函数注释中的mV与示例数值不一致，不能按普通mV直接使用。

来源可在[锁定SDK算法](https://github.com/OpenSiFli/SiFli-SDK/blob/421126d9f476ed8e2a6f0b0ca28a9f241c182e65/middleware/battery/battery_calculator.c)及[同板参考表](https://github.com/OpenSiFli/SiFli-SDK/blob/421126d9f476ed8e2a6f0b0ca28a9f241c182e65/customer/boards/sf32lb52-lchspi-ulp_base/battery_table.c)复核。只提取参考数据，项目独立实现纯C估算；不启用调用`rt_charge_get_status()`的SDK计算器或其充电服务。旧F411的[复用记录](F411-REUSE.md)主要涉及MAX30102，没有可确认的本电池SOC曲线。

这些点没有电芯型号、容量、温度或负载校准信息，不是原ZCLP500的测量结果；每5%抽样也不同于原101点表。当前无连续电池净电流／电芯温度输入，不能声称库仑计量、容量学习或准确剩余续航。查表、ADC电压对照和SOC曲线校准是不同证据。[TI SLUAAA1](https://www.ti.com/lit/an/sluaaa1/sluaaa1.pdf)说明电压查表需电池特性数据，负载、老化、温度和电压回弹影响结果。

其他对照实现：[Zephyr battery_level_pptt](https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/boards/nordic/battery/src/battery.c)将曲线与ADC采样分开并插值；[Bangle.js](https://github.com/espruino/Espruino/blob/master/libs/banglejs/jswrap_bangle.c#L2971-L3015)使用轻量分段电压映射及部分型号的满电电压校准。它们仅供实现比较，本轮不采用其板级阈值。

## 采样与估算契约

- 原power worker每次有效ADC完成分配一个非零`voltage_sequence`和单调毫秒完成时间。先发布电压、后发布PG时复用同一身份；原IRQ短临界区复制、息屏清缓存和epoch迟到拒绝保留。
- UI线程在`main::read_snapshot()`合并charger缓存后调用`core/battery_estimate`；同一sequence只处理一次。首次／可信模式变化后的新样本直接播种，其后IIR为旧值3份＋新值1份；百分比2点滞回，0／99端点和FULL明确处理。没有随时间增加电量、循环动画或息屏采样。
- 电压读取失败、sequence缺失、年龄超过15000ms、节点电压不在3000–4250mV时估算无效；过期同时撤销显示用电压有效性，历史清零。15000ms边界有效，15001ms失效。范围仅为估算输入合理性门槛，不是电池在位检测、放电截止或充电保护参数。
- ACTIVE使用充电参考表；可信IDLE／STOPPED使用放电参考表。UNKNOWN／FAULT或独立PG尚未返回的普通模式不猜曲线、不消费新sequence；恢复可消费该样本。普通模式切换复用旧sequence时先显示未知，等新ADC；FULL可在仍有效的样本上锚定100。
- 所有电压查表结果最多99；只有现有charger逻辑报告FULL且ADC样本有效时输出100。显示仍是估算，FULL文字／软件状态不证明物理停流。0%仅为参考表低端，不触发任何新关机或停充动作。
- estimator与显示不改充电参数、启停、故障处理、原插接周期、提醒优先级、退出消费或PM策略。没有新增timer、worker、电源lease、Flash保存或设备I/O。

## 统一显示与进度环

`power.estimate_valid/estimated_percent`进入共同快照及`power_equal()`，`wristflow_product_apply_power()`同步legacy电量字段，防止有效性／数值不同步和刷新遗漏。共用formatter显示`~59%`，未知回退有效电压或`--`；UI Demo原确定性百分比不加`~`。

表盘、卡片、设置与关于页复用同一formatter；表盘有效百分比保留显示，以颜色辅助表达充电／故障。现有电量页同时显示估算、实测节点电压、USB和充电状态，并写明“电压估算”；有效性变更后继续刷新，不因percent可用退回占位页。没有实现新的电池设置／长续航／续航优化页。

10-18后续[正式电池主页面](BATTERY-PAGE.md)覆盖上段“没有新电池页”的阶段限制；现有SYSTEM路由改用官方XML新布局，未知大号电量为`--%`、电压独立显示。算法及下文原BIN证据不变，新布局的验证另见页面契约。

充电XML声明百分比和提示控件，官方Pro GUI导出；手写适配层对既有圆角轮廓按路径长度绘制比例，顶部中点顺时针，未知保持暗环，有效FULL估算才全环青绿。样本失效时数字和环一起回到未知，charger缓存的“已充满”文字仍独立保留。仅数值／状态／时间变化时更新；不会建立新插接周期或重新弹屏。XML的`~59%`只是编辑器样例，真正绘制使用同一Product估算。

## 验证与后续数据

从`d9cb294`开发，完整主机45/45（21.21秒）及保留断言Release相关6/6（1.11秒）通过；最后审查补FULL失效暗环后，相关Debug1/1（0.83秒）／Release1/1（0.47秒）通过。官方Pro2.0.1导出／预览编译成功、LVGL9.4.0，100%及真实LVGL各比例／失效帧无重叠缺字，独立只读审查问题均关闭。Product／UI Demo均退出0，415／379源码及29／22产物重哈希一致；完整命令、版本、哈希和失败保留于[软件证据](evidence/2026-10-18/battery-estimate.json)。软件阶段无设备访问，后续下载见下；旧充电画面的真机结果不抵扣本轮。

软件[PR #86](https://github.com/e-n-ye/wristflow/pull/86)已Rebase合入`42a29f5`，MERGED／fetch及完整树确认与已验证源码一致。用户新确认断电池、仅电脑USB后，BIN`fc4d6703…`在COM5首次三镜像write+verify退出0；一次150ms RTS启动捕获确认CO5300／FT6146初始化、early禁充及无电池VBAT约172–178mV下`estimate_valid=0`／低电压拒充。RTC重新校时读回、allow恢复1并自然息屏，13:59:57正常关闭串口。下载／启动记录[PR #87](https://github.com/e-n-ye/wristflow/pull/87)已合入`ca25f42`；后续反馈沿用同BIN、未再开串口或改固件。[硬件精简证据](evidence/2026-10-18/battery-estimate-hardware.json)保留镜像、命令、原始日志哈希、用户原文及边界。

用户随后回报按步骤接回原BAT后电量页为`~91%／4.04V`，无万用表；换接后时间`--:--`，已通过连接同步。插USB显示`~94%`、环比例大致一致，其余指定的插入展示／触摸原页恢复／同周期不重弹步骤无问题。进一步澄清普通页睡醒后应重新出现百分比，用户确认“恢复正常，不需要5s，几乎开屏就有显示”。据此关闭本版有界显示／交互回归，不记独立毫秒计时；未提供USB段同步电压、表测或照片，不能把91→94当作实际充入3%或已证明的曲线切换原因。RTC换接保时、估算精度和实际停流均未因此通过。

后续有表时建议有界记录同一时刻的电池端万用表电压、UI电压／估算值及USB／充电状态，至少保留两个自然出现的不同电压点；此项尚无数据，不构成新设备实验授权，也不能把电压对照写成SOC准确度。真正修正电量曲线需要已知放电条件下的电压和累计净电量数据；不要求手动持续监控，也不安排未授权的充放电或长时测量。
