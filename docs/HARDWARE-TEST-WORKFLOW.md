# 人机协作测试：电量主机工具

活动项和唯一下一步只见[STATUS](STATUS.md#current)。本页保存方法和试点边界；历史命令不构成新设备操作授权。

## 本轮范围与分工

2026-10-21用户要求借电量排查改进人机协作。PR #120首轮复用B2实机记录，完成可重复B3回放和OpenHTF网页演练；后续用户要求继续，增加模拟采集适配器及共同本地控制页。AI制定实验、分析证据；本地程序持有阶段和输出；人只确认需要判断或实际动作的部分。软件验收由主代理在浏览器操作，没有让用户重复设备输入。

当前命令行仅开放历史回放、引导演练与模拟采集，没有实机端口参数或烧录功能。`SerialTransport`已写出串口边界，但只通过替身验证，未实际打开设备；这不是已验收的硬件采集台。仍沿用[硬件条件](HARDWARE.md)、[下载](FLASHING.md)与[电量契约](BATTERY-ESTIMATE.md)，SDK环境和固件不变。OpenHTF保留phase／measurement／JSON，不再使用其StationServer／SockJS前端；不构成全项目框架选型决定。

## 离线回放

输入`tests/fixtures/battery-b2-20261021.json`来自B2 session3的121条解析记录，保留原始解析文件和8页UART文本的SHA-256及原BIN身份。完整原始文件仍在原工作树忽略的`artifacts/battery-b2-20261021/`，不宣称随仓库分发。fixture不是新采样，也没有补齐OFF间隙。

`scripts/battery_lab.py replay`验证schema、停止状态、数量、会话、连续索引、ADC身份／年龄和记录窗口；编译`core/battery_estimate.c`及主机专用`scripts/battery_replay.c`。记录开始前的历史不可重建，因此第0条只作热状态输入；其余120条比较生产C的滤波、有效性、候选、百分比、模式及decision。携带旧诊断的上下文行允许NEW／SEED与重复调用产生的CACHED对应，单列索引，不把它算作新ADC。

另运行两个纯主机实验及其组合：缺样后30秒内、可信相同放电模式和新ADC恢复时复用历史；1点候选差异跨至少3个独立ADC及60秒后收敛。无效期间仍输出未知，不用旧百分比伪装新测量。30／60秒只是本轮实验参数，未决定固件常量。结果与限制见[B3](BATTERY-ESTIMATE.md#battery-workflow-pilot-20261021)。

从仓库根目录运行；本机已有GCC可用，其他机器用实际编译器路径替换`--cc`：

```bash
python scripts/battery_lab.py replay --cc D:/msys64/ucrt64/bin/gcc.exe --out artifacts/battery-lab/replay-001
```

输出目录必须不存在，防止覆盖旧证据。输出包括`build.log`、精确`engine-input.txt`、引擎输出、四组预测、`replay.json`和便读的`report.md`；JSON记录源码、输入、引擎哈希及编译器／Python版本。失败没有通过报告，保留该次目录与命令输出。

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
