# 历史回放基线

此目录固定`17a9de551cd2165af71b0e6f8c28678bd3b6a42c`的`core/battery_estimate.c/.h`，只供旧设备记录复算及既有主机策略实验。源码内容保持该提交，不跟随生产修复；`power_state.h`仍从core引用。

Product、UI Demo及当前生产回放均不链接此目录。当前生产行为见`core/battery_estimate`，两条旧实机记录与新源码比较由`battery_lab.production_compare`完成。旧设备记录不是新版本已经通过实机验收的证明。
