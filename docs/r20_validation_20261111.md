# R20 下载前需求与证据门槛核对

更新：2026-11-16  
结论：**R20未通过；当前正在准备force-coast诊断下载，但还没有哈希匹配的当日候选。**

## 已核对事实

- 早期V10.5下载曾完成2MHz under-reset写入、20KB整片回读、应用区逐字节匹配和Running确认；verification同时明确功能稳定性未验证。该次成功下载是调试证据，不是产品验收。
- 最近归档板况为R6 `P50 V10.6SIMB2C` force-coast，整片SHA256=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`；本轮没有重新读板。
- R11 force-coast最新BIN有完整manifest，但`src/servo_control.c`哈希与当前源码不一致；R14 force-coast构建有3项源码输入与当前树不一致。历史BIN不可冒充当前源码构建。
- 本机交叉编译器缺少newlib/sysroot，当前未产出新BIN/ELF。诊断见`tools/check_arm_none_eabi_runtime.py`及`firmware/evidence/toolchain_macos_runtime_20261116/report.json`。
- R01基础方向结论沿用用户此前确认；当前候选微步重复性/同步遥测、PA4电压、双路H桥波形、真实HOLD/行程/制动和R12数值门槛等仍未闭环。

## 下载门槛与执行顺序

1. 先用固定Ubuntu工具链对当前完整源码重建并生成全量manifest；选择force-coast诊断版，只检查MCU运行、遥测、界面和非运动时序。
2. 下载前只读识别板况并备份整片Flash；用under-reset写入，写后整片回读并逐字节校验应用区，确认Running。
3. 诊断镜像通过只表示版本/运行身份可信。方向结论沿用用户此前确认的结果；动作固件用于验证当前候选的微步重复性和遥测质量，按中位限幅小步测试，不做端点或自动扫动。
4. 全矩阵要求、波形和数值门槛满足后，才可将产品固件验收标记为通过。

当前不需要用户按键、输入PWM或操作仿真板。**答案：下载正在准备，但还不能执行；当前缺的是同版构建产物，不是用户操作。**
