# R19 版本、构建身份与唯一下载对象核对

历史基线：2026-11-16；最新状态见文末2026-11-19增量。

当时结论：历史构建可追溯，但尚无与完整工作树一致的新BIN/ELF；R19部分通过。

## 当前硬件与构建状态

- 最近归档的板况为 R6 `P50 V10.6SIMB2C` force-coast，整片 20KB SHA256=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`；本轮未重新读板。
- `build/releases/V10.6_R21_FINAL_20261113/` 是历史构建：实际 BIN **18,660 B**，SHA256=`ac76262a374ab095632991f83fa131c39837d8b56f0cbc363cad6aec97c2dfe8`。manifest 含35项源文件哈希、SDK、flags及4种产物哈希，标记`flashed=false`。旧文档曾误记18,704 B，已按实际文件与manifest更正。
- 最新完整LCDM force-coast候选`build/releases/V10.6_R11_STABILITY_TIME_COAST_R4_20261115/`，BIN 19,180 B，SHA256=`e2d6476a48a00d6e781e5b273c740ae759be8e0fa29c1d363b21dcc832e23e09`，manifest覆盖36项源文件且标记未下载。逐项与当前工作树比对，`src/servo_control.c`哈希不一致，原因是其后加入R03失联恢复速度基准修复。
- `V10.6_R11_STABILITY_TIME_PRODUCTION_R3_20261115`同样与当前`src/servo_control.c`不一致，且为非force-coast生产路径，不选作当前诊断下载对象。
- `V10.6_R14_PRIMASK_FORCE_COAST_20261115`比当前工作树有3个输入文件不同：`Makefile`、`src/app_entry.c`、`src/servo_control.c`，也不选作当前候选。
- 旧 `candidate07` 快照只含`src/app_entry.c`副本/哈希，其他构建输入缺失，不能重建验证；不再作为下载候选。

## 下载准备结论

- 正在准备受控诊断下载，但**当前没有与完整工作树一致的BIN/ELF**。本机 Homebrew `arm-none-eabi-gcc 15.2.0`缺少newlib/sysroot；`stdint.h`无法编译且`libc.a`不存在。诊断见`firmware/evidence/toolchain_macos_runtime_20261116/report.json`。
- 应通过固定Ubuntu工具链重建当前源码，保存全量源文件哈希、SDK/flags与产物哈希；第一版下载对象只选force-coast诊断镜像，用于非运动的MCU调度、显示和遥测检查。不能选生产BIN，也不能因force-coast镜像启动而宣称产品通过。
- PWM目标、电位器手动方向和马达方向沿用用户确认的既有定论；最近一次微步采集不重复不推翻该结论。当前动作固件的待验点是微步重复性、同步输出证据及报警行为。下载仍需同版构建、只读识别/备份、under-reset写入和整片读回。
- 当前无需用户按键或输入PWM。R19剩余验证是生成同版构建、唯一候选确认和板上读回；本轮没有下载。

## 2026-11-19 最新构建与板上状态更正

- 当前最新构建为`build/releases/V10.6_TSSOP20_R01_CENTER_RETURN_DIAG_20261119_R1/`，BIN 16,332B，SHA256=`2a204ad96fcbf37183948482a8566f3d7ce5e9989f19cb68f49d95b29ac438e6`，ELF SHA256=`695dd49cb92fc42d838327808e8832dae63374ac7b4d6ca0e568c11c846d2fd0`。manifest记录目标`py32f002ax5`、SDK、flags、全量参与源码和产物哈希，`flashed=false`。
- 当前板按R01停止记录为R16慢速诊断固件；SWD确认active=0、arm=2、HBridge duty=0/state=coast、核心Running。中心回归候选没有下载。以后任何下载仍须先实时识别并备份整片Flash，不能依靠旧板况摘要。
- R19仍部分完成：构建身份可追溯；候选板上写入/回读尚未进行，R01/R20产品验收均未通过。
