# R03 速度时间基准验证记录

日期：2026-11-10  
对象：V10.5 板上基线及未下载的 V10.6 候选  
结论：**通用路径部分修正；LCDM 现场观察器和加速度/制动实测尚未完成，R03 未通过。**

## 已发现和修正

原通用 `Servo_Control_Update1ms()` 把位置差乘以 16 后直接作为速度 Q4 交给自适应模块，没有除以真实时间差；例如 2 ms 内移动 +4 ADC，原结果为 64 Q4，正确的平均速度是 +32 Q4（即 +2 ADC/ms）。旧逻辑在 4 ms 内 −4 ADC 得到 −64 Q4，而平均速度应为 −16 Q4。相同毫秒重复调用、失联后恢复和长时间间隔也缺少速度基准重建。

未下载的 V10.6 candidate06 增加独立的位置和时间基准，按 `Δposition × 16 / Δt_ms` 计算速度，限制为有符号 16 bit；同一毫秒不计算速度，超过 4 ms 的间隔不用于学习并重建基准，输入失联后在恢复时重新采基准。4 ms 与自适应模块的最大学习间隔使用同一个常量。无效样本传给自适应模块时会清除其前一学习样本基准。

使用真实 `servo_control.c`、注入的位置和时间以及模拟的自适应接口，V10.5 基线在最初 18 项中有 8 项失败；candidate06 扩展至 20 项，全部通过，覆盖 1/2/4 ms 正反速度、同一毫秒、长间隔、输入中断/恢复、计时器回绕和速度数值饱和。这是软件输入输出测试，不证明电机速度或ADC时间戳正确。

通用闭环简化马达模型重新执行六段，均进入 HOLD，最终误差 +4、−1、+1、−4、+5、−5 ADC，CSV 与先前基线逐字节一致。该模型每次使用 1 ms 间隔，且没有真实 H 桥、滤波噪声和机械振动；结果不覆盖 LCDM。

## 当前源码复核（2026-11-10）

- 当前 `src/servo_control.c` 时间差测试重跑 **20/20 通过**：正负方向、1/2/4 ms、同 tick、长间隔、失联恢复、tick 回绕和饱和。结果：`firmware/evidence/r03_current_recheck_20261110/report.json`。
- 当前生产 `src/servo_adaptive.c` 新增实测 **19/19 通过**：正反制动各在 1/2/4 ms 下按实际间隔归一；并覆盖同 tick、>4 ms 间隔、tick 回绕和无效样本重建。结果：`firmware/evidence/r03_adaptive_time_20261110/report.json`。
- **R03 仍未通过**：LCDM 的 `s_lcdm_servo_pos_q4/vel_q4` 观察器只按函数调用次数做预测和残差更新，没有用实际 `dt`；同一 tick 重复调用也会重复修正，HOLD 又把速度清零。虽然 `Servo_Adaptive_Update()` 会按 `dt_ms` 估算制动减速度，但 LCDM 传入的速度未先做时间归一。

## 未通过原因与所需协助

我已完成通用速度与自适应制动估算的离板验证；下一步需修正 LCDM 观察器并回归其时间差输入输出。实机部分需要 DAPLink 接入 Ubuntu，且仿真板需连接舵机和电位器，以内部 `INTPWM` 做正反运动并采集带时间戳的位置、速度、加速度和 duty。**不需要外部 PWM 信号源**。本次 Ubuntu 未检测到 DAPLink；用户需协助接入并在 Parallels 中分配给 Ubuntu。

## 2026-11-10 历史问题记录（已由下方 11-13 observer 修正更新）

- V10.5 LCDM 使用另一套位置/速度观察器，其预测和修正按每次函数调用计算，没有按实测 `dt` 缩放；服务循环延后时可能在同一时间戳内补跑多次，不能将观察器值直接认定为经过时间归一化的 ADC/ms。
- 现场 HOLD 分支会将观察器速度清零，因而不能仅凭 HOLD 时显示零速度确认机械静止。
- 自适应模块对速度变化和时间差有局部制动减速度估计，但其输入速度的真实时间基准、ADC 可信度门控、正反方向实物标定及完整加速度/制动测量尚未验证。

需在真实控制周期记录带时间戳的单次/聚合 ADC、观察器位置、速度和加速度，分别用正反运动及延时/同 tick 样本核对；修正并回归 LCDM 观察器后还需要客户波形和实机验证。构建与测试留档见 `firmware/candidates/v10.6_r01_r03_candidate06/`，均未下载。

## 2026-11-13 LCDM observer 修正进展

- `inc/lcdm_velocity_observer.h` 将 LCDM 位置/速度估算提取为可注入时间戳的生产 helper；按 `HAL_GetTick()` 的实际毫秒差预测位置、用 ADC 差分除以实际 dt 得到 ADC/ms，再以快/慢状态原有滤波系数平滑。相同 tick 不重复修正，超过 4 ms 时重建位置和速度基准，tick 回绕采用无符号差值。
- 复用原有 ADC 历史和 seeded 标志，只新增 observer tick。HOLD 入口不再直接把 observer 速度清零，后续 ADC 样本继续更新估算。该改动只做源码与主机验证，未下载。
- 新增 `tools/r03_host_sim/test_lcdm_observer.c` 与 runner；最终版编译选项 `-std=c11 -Wall -Wextra -Werror`，19/19通过，覆盖初始化、正反方向、1/2ms等速轨迹对照、同tick不更新、>4ms重建和tick回绕。1ms/2ms等速结果分别29/31 Q4/ms（目标32）。曾有Q8版本对照失败（25/29、差4），改为Q12时间系数与有符号舍入后通过；证据在 `firmware/evidence/r03_lcdm_observer_20261113_final/`，失败保留在 `firmware/evidence/r03_lcdm_observer_20261113_timeconstant_recheck/`。
- Ubuntu LCDM 固件构建失败：链接器报告 RAM region overflowed by 16 bytes，并触发“静态 RAM 至少留 1536 bytes stack”的断言。先前未压缩状态时缺 24 bytes；优化为复用既有 ADC/seeded 状态后缩小为缺 16 bytes，但仍未生成可交付 ELF/BIN。未降低栈保护；UART RX ring 为 64B 且采用 `& 0x3F` 环绕，不能只缩数组而不改索引与验证。
- 随后将 TJC 触摸 UART RX ring 缩至 32B（31B有效容量，最多缓存三个完整 9B 触摸帧），同步改为 5-bit 环绕掩码；保留 1536B 最小栈断言。Ubuntu LCDM 固件构建通过，Flash image 18,784B，初始 SP `0x20000c00`、reset PC `0x08004431`。产物与全源哈希见 `build/releases/V10.6_r03_observer_20261113_lcdm/manifest.json`；BIN SHA256 `11f14fe42ae402cfc3c0794d37cdf25bd6bf66368c456158f8a7bad3619ada80`。observer host test 复跑 16/16 通过。
- 新增 `inc/lcdm_rx_ring.h` 后，以生产 ring helper 执行 FIFO、31B满载、溢出计数和回绕测试，**171/171 通过**；最终证据在 `firmware/evidence/r03_lcdm_rx_ring_20261113_final/`。这仅验证软件队列，不覆盖230400bps下的UART中断负载、LCDM帧间隔或触屏连续点击。
- `tools/build_v10_ubuntu.py --internal-step-test` 曾生成受限中位诊断固件。内部扫动前加入30秒1503µs静止预留，保持1503→1543→1503µs、每250ms 1µs、signed duty上限160/1000；监控工具 attach 后立即落首帧、默认采集70秒。该阶段构建V10.6SIMB2位于 `build/releases/V10.6SIMB2_R03_R15_20261113/`，BIN 18,164B，SHA256 `cfbc4e7abca46f95fe1a4d264aa0478182035b44ce863b33445a0725386107a0`，SP=`0x20000c00`，PC=`0x080041cd`；manifest标记flashed=false。仅改善采集同步条件，不保证SWD接入一定在30秒内完成。
- **R03 仍未通过**：observer与ring离板用例通过；LCDM真实控制周期下的加速度/制动、噪声及速度单位仍未实机验收。方向沿用用户此前确认的PWM、马达和电位器结论，不是当前阻塞项。当前实机未通过点是候选版本下的同步采集与微步响应重复性；软件离板测试不能替代该证据。不需要用户按键或外部PWM源。

## 2026-11-15 通用速度恢复门控修正与复跑

- 全局离板回归首次发现失联恢复测试1项失败（19/20）：PWM失联期间速度观测器为可能启用的中位回中控制继续更新，但输入恢复首帧把跨失联位移算作一个速度样本。
- 在`src/servo_control.c`记录上一帧输入有效状态。失联期间仍按真实tick更新速度，供失联回中逻辑使用；由失联恢复的第一帧只重建位置/时间基准，不交给HOLD速度条件或学习样本，下一帧恢复正常测速。这样保持失联回中行为，同时消除恢复瞬间的伪速度。
- 修复后速度测试 **20/20**；LCDM observer **19/19**；RX ring **171/171**；adaptive time **19/19**。重新运行 R07 控制调整 **22/22**、R08 trajectory **17/17**、R09 brake predictor **8/8**。各轮报告位于`firmware/evidence/*_20261115_rerun*`。
- R15 ADC故障路径复核 **4/4**；R13双槽模型store mock复核 **26/26**；R02 ADC development/SOP8 **30/30各自通过**。它们仍分别缺真实故障电气波形、生产Flash保存调用/实板掉电、PA4电压与负载瞬态数据。
- 本轮本地交叉编译未完成：macOS Homebrew 的`arm-none-eabi-gcc 15.2.0`包只提供GCC前端/内部头文件，没有目标C库sysroot（newlib）。GCC内部的`stdint.h`用`#include_next`转发，但系统搜索路径中没有C库的`stdint.h`；`-print-file-name=libc.a`也只返回字面值`libc.a`，对应库不存在。根因是本机工具链未安装目标运行库，不是项目源码漏头文件；该编译器不能替代项目固定Ubuntu工具链。已新增`tools/check_arm_none_eabi_runtime.py`诊断脚本，结果归档在`firmware/evidence/toolchain_macos_runtime_20261116/report.json`。已运行的主机回归全部使用`-Wall -Wextra -Werror`。
- **R03仍未通过**：软件速度/时间边界回归恢复通过，但LCDM实机速度、加速度/制动单位及噪声影响验证仍缺失。方向映射沿用用户此前定论；板上测试待当前源码同版构建及同步采集流程就绪后合并进行。此处“同步采集”用于验证候选响应/时序，不是重新确认基础方向；无用户操作需求。
