# 人机协作测试：电量主机试点

活动项和唯一下一步只见[STATUS](STATUS.md#current)。本页保存方法和试点边界；历史命令不构成新设备操作授权。

## 本轮范围与分工

2026-10-21用户要求借电量排查改进人机协作。首轮复用B2实机记录，完成可重复B3回放和OpenHTF本地网页演练。AI制定实验、分析证据；本地程序持有阶段和输出；人只确认需要判断或实际动作的部分。演练由主代理在浏览器操作，没有让用户重复设备输入。

当前工具没有串口、烧录或设备控制接口，不是已经完成的硬件采集台。仍沿用[硬件条件](HARDWARE.md)、[下载](FLASHING.md)与[电量契约](BATTERY-ESTIMATE.md)，SDK环境和固件不变。OpenHTF仅作已运行的候选试点，不是全项目框架选型决定。

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

用独立主机环境安装锁定的直接依赖`openhtf==1.6.1`，不安装到SiFli SDK Python。已实测CPython3.12.9／OpenHTF1.6.1／Tornado6.4.2；不承诺其他组合。官方能力来源是[人工提示](https://github.com/google/openhtf/blob/v1.6.1/openhtf/plugs/user_input.py)及[StationServer](https://github.com/google/openhtf/blob/v1.6.1/openhtf/output/servers/station_server.py)，本机结果单独记录。

```bash
uv run --isolated --python 3.12 --with-requirements scripts/requirements-battery-lab.txt python scripts/battery_lab.py guide --report artifacts/battery-lab/replay-001/replay.json --out artifacts/battery-lab/guide-001
```

打开程序打印的`http://127.0.0.1:<port>`。默认端口4444，也可传`--port 0`分配临时端口。适配层限定回环地址并跳过multicast；保留官方网页，不构建另一套前端。后台运行时禁用控制台输入，所有提示只在网页作答。

三个演练阶段为准备证据、核对供电窗口、判断策略结果；输入`continue`继续、`cancel`取消，其他输入也按取消处理。默认每阶段300秒，`--timeout`可设大于0且不超过900秒。**本轮不要拔线、唤醒或操作设备**：网页确认只说明演练阶段已确认。

每次阶段变化立即追加`events.jsonl`并更新`progress.json`；同时复制绑定的`replay.json`。结束时写`openhtf.json`，工具独立读回最终outcome和哈希后才能记`finished`；输出回调失败即使OpenHTF打印PASS，本工具仍异常退出、记`interrupted`。取消／超时有独立事件、最终`passed=false`和进程退出1，不能当功能失败或设备故障。

换聊天或程序中断后，先读取同一进度，无需再询问用户上一阶段：

```bash
python scripts/battery_lab.py status --run artifacts/battery-lab/guide-001
```

`running_or_interrupted`表示尚无终态，不能仅凭文件判断进程还活着；尾行不完整记`incomplete`，证据哈希改变则拒绝。当前支持证据接续查看，**没有进程重启后自动恢复执行**。确认旧进程已结束后，可从同一离线报告在新目录重新演练；不能照搬为真实设备实验的自动重启／重新arm。

```bash
CC=D:/msys64/ucrt64/bin/gcc.exe uv run --isolated --python 3.12 --with-requirements scripts/requirements-battery-lab.txt python -m unittest discover -s tests -p test_battery_lab.py -v
```

测试覆盖真实C回放、损坏／缺失／混会话、保留期限／充电守卫、缺样仍未知、进度残缺、证据变更、取消／超时及报告写失败。软件测试输出保留在忽略的`artifacts/battery-lab-tests/`，不删除旧轮次。

## 已发现的限制与实测准入

本轮真实网页完成、网页取消及真实1秒提示超时均已验证；没有串口操作、固件编译或新硬件证据。完整命令、失败、哈希和版本见[试点证据](evidence/2026-10-21/battery-workflow-pilot.json)。

OpenHTF1.6.1在网页连接后的收尾仍打印WebSocket未等待协程／pending task警告；最终网页结果和JSON已读回，但不能据此宣布服务可长期稳定采集。取消时官方UI还显示“Ran 5 of 5 phases”，剩余phase实际Waiting，取消阶段标Error；准确完成边界取`events.jsonl`及最终`passed`，不能把进度条视为所有动作完成。上述问题保留，接管真实设备前需处理／验证，不在本轮展开第三方框架重构。

未来设备适配应在同一个本地会话中集中准备、提示动作、收集和清理，并先在模拟串口验收：单拥有者、开口前DTR／RTS关闭、原始字节先存证、基于完整回执等待、会话／页码校验、有限重试、取消后释放资源和恢复状态。设备记录启动时间、网页动作确认时间、PG／页面观测时间分别存储；不能因确认点击就断言物理动作发生，也不能把480秒记录器预算称为480秒BAT窗口。缺少关键窗口应明确未完成。此段是准入契约；获准执行顺序仍只回STATUS。
