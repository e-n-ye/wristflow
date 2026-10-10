# 编译、下载与真机验证

当前构建与版本按 [BUILD](BUILD.md)、[STATUS 当前区](STATUS.md#current) 核对；下载前确认 COM 口、板型、当前镜像清单、供电和授权。本页后面的编号 1–5 保留 PR #11 的 2026-09-24 Hello／Bringup 首次上板流程与实测，不替代当前 Product 指令。

<a id="current-flash-entry"></a>
## 当前下载入口（2026-10-10 源码核对）

主线 `64c7e43` 已包含 [scripts/Flash.ps1](../scripts/Flash.ps1)，需要 PowerShell 7、目标已有构建产物和本机 sftool。脚本以对应构建目录的 `sftool_param.json` 为准，校验锁定板型、NOR、verify 标志、三份非空镜像及地址，拒绝重复项和目录外路径，输出镜像哈希。

- `-Example`：目标为 `hello`、`ble`、`bringup`、`ui_demo`、`watch` 或 `product`。
- `-Port`：实际枚举确认的 `COM` 编号；下例 `COM5` 只是命令示例，不表示本轮观察到了设备。
- `-DryRun`：完成本地校验后返回，不调用 sftool、不打开设备、不复位。它仍要求构建产物和下载工具已存在；通过不证明已下载或能运行。

在项目根目录先做本地校验：

```powershell
pwsh -NoProfile -File .\scripts\Flash.ps1 -Example product -Port COM5 -DryRun
```

实际下载在版本、端口、USB 条件与授权已核对后使用同一命令去掉 `-DryRun`；脚本会执行 `write_flash --verify`、软件复位，并将日志、命令、镜像哈希和退出码写到 `artifacts/flash/product/<run-id>/`。`FLASH SUCCEEDED` 表示下载工具退出 0，记录仍为 `hardware_verified=false`；启动、画面、触摸、BLE 和电源分别验收。`Build.ps1` 仍不执行烧录。

本轮只检查脚本参数与执行路径，没有执行上述命令、构建、下载或串口操作。原天气分支才有此脚本的描述已被现版入口替代，flow 接入时的历史快照继续保留。

## 1. 在 VS Code 编译

1. 用 VS Code 打开项目根目录 `D:\MY_Desk\project\wristflow`。
2. 新建终端，选择用户级终端配置 `SF32`。看到 `SiFli-SDK v2.5.1` 和 `profile=default` 后，运行 `scons --version`。
3. 按 `Ctrl+Shift+B` 编译 Hello，或按 `F1` → `任务: 运行任务`，选择：
   - `WristFlow: Build Hello World`
   - `WristFlow: Build Bringup`
   - `WristFlow: Build UI Demo`
   - `WristFlow: Build BLE Peripheral`
4. 成功标志是终端末尾出现 `BUILD SUCCEEDED: <example> (sf32lb52-lchspi-ulp)`。构建证据保存到 `artifacts/<example>/<时间>/`。

你自己的第一个可修改程序位于 `apps/bringup/src/main.c`。改完后选择 `WristFlow: Build Bringup`，再执行下面的下载命令。每次改动都必须重新编译，确认 `BUILD SUCCEEDED` 后再下载；只保存 C 文件不会改变板上的程序。

也可以在 `SF32` 终端中运行：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Example bringup -Jobs 4
```

把 `bringup` 换成 `hello`、`ble` 或 `ui_demo` 可构建其他目标。脚本会重新检查锁定的 SDK 和子模块，生成 `main.bin`、`bootloader/bootloader.bin`、`ftab/ftab.bin` 及 `sftool_param.json`。

## 2. 下载前核对

- Windows 设备管理器应显示目标板的 `USB-SERIAL CH340 (COMx)`。本次实物为 `COM5`，但 COM 编号可能变化；每次以当前设备管理器为准。
- 下载前关闭正在使用该 COM 口的串口监视器；同一端口通常不能同时由两个程序打开。
- 当前工具为 `.tools/sifli/tools/sftool/0.2.5/sftool.exe`，板型为 `SF32LB52`，存储为 `NOR`。
- 以目标构建目录中的 `sftool_param.json` 为准。黄山派当前三项地址为：`ftab`=`0x12000000`、bootloader=`0x12010000`、main=`0x12020000`。
- 首次下载前确认已保留原程序备份，或确认原程序不再需要。不要使用 Nano 的产物，不要勾选整片擦除。

## 3. 下载一个目标

下面是 Bringup 的示例。命令在项目根目录运行，路径可替换为 Hello、BLE 或 UI Demo 的对应构建目录：

```powershell
.\.tools\sifli\tools\sftool\0.2.5\sftool.exe `
  -p COM5 -c SF32LB52 -m nor -b 500000 `
  --connect-attempts 3 --after soft_reset `
  write_flash --verify `
  "apps\bringup\project\build_sf32lb52-lchspi-ulp_hcpu\bootloader\bootloader.bin@0x12010000" `
  "apps\bringup\project\build_sf32lb52-lchspi-ulp_hcpu\main.bin@0x12020000" `
  "apps\bringup\project\build_sf32lb52-lchspi-ulp_hcpu\ftab\ftab.bin@0x12000000"
```

命令结束后在同一终端运行 `$LASTEXITCODE`，应得到 `0`。三个镜像分别出现进度且退出码为 0，表示传输和校验完成。命令会复位设备并替换当前固件；它不会自动保存旧 Flash，也不会执行整片擦除。

## 4. 看启动日志

下载完成后，用串口工具打开同一个 COM 口，参数为 `1000000 8N1`、无流控。CH340 的 RTS 可能连接复位电路；若打开串口没有启动日志，关闭串口后重新打开，并进行一次短复位。不要向串口发送命令。

Bringup 启动日志应能看到 `CO5300_ReadID 0x331100`、`ft6146`、`KEY1 pin=34` 和 `ACTIVE 390x450`。这些日志证明对应驱动初始化；画面、触摸和亮灭屏仍需在屏幕上实际操作。

## 5. 本次已完成的实测

2026-09-24 在 `COM5` 上完成：

- 用户确认黄山派已组装，5-6、7-8、11-12 三组跳线均短接；此前依据照片怀疑缺跳线的判断已撤回。原彩条程序启动日志与 16 MiB NOR 读取备份留在本机 `artifacts/hardware/`；读取备份不替代完整 Flash／PSRAM 容量验收。
- 用户实际选择 VS Code 的 `SF32` 终端，锁定 v2.5.1 环境与 SCons 4.10.1 已激活；参考教程的 release/v2.4 没有改变项目锁定版本。
- Hello 在 v2.5.1 黄山派配置下编译成功并下载校验成功；`main.bin` SHA-256 为 `5d1e396f2e9cd6edd35f0586d2c30dbd7f3dd4cf0f42a5bcf17c9a976e4173c0`。复位后串口输出 `build on Sep 22 2026, 2.5.1 build 421126d9` 和 `Hello world!`。
- Bringup 编译成功并下载校验成功；`main.bin` SHA-256 为 `deadadab94b802be43bbd3eeab9580de1828c28fafa679c9254b9334984895fb`。复位后日志识别 CO5300、FT6146 和 KEY1，进入亮屏、熄屏、按键唤醒代码路径。用户现场确认显示画面、基本触摸、亮度调节、熄屏和 KEY1 唤醒均正常；照片显示触摸坐标、67% 亮度和 KEY1 计数。
- 本机构建记录在 `artifacts/hello/20260924-143440-299/` 和 `artifacts/bringup/20260924-154533-731/`；下载工具的退出码均为 0，原始日志为 FastCtx jobs `j-yk1z85`（Hello）和 `j-4rn8dn`（Bringup）。串口日志保存在 `artifacts/hardware/20260924-154430-109291-hello-reset-uart/` 和 `artifacts/hardware/20260924-154755-222290-bringup-reset-uart/`。这些本机目录由 Git 忽略，不属于公开仓库。
- 尚未完成触摸四角逐点坐标记录、50 次亮灭屏循环、功耗测量、BLE 或真实 Deep Sleep 验证。

不要把编译成功、串口日志或 `OFF requested` 日志当作触摸通过、Deep Sleep 或功耗通过。硬件观察结果需回填 [到货验收](ACCEPTANCE.md)。
