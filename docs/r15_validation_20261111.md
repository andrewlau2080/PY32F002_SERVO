# R15 故障学习冻结与安全降级核对

日期：2026-11-11  
对象：工作区 V10.6 candidate07 LCDM 与通用控制路径（板上仍为 V10.5）  
结论：**R15 未通过；有部分 LCDM coast 保护和学习样本筛选，但故障覆盖、冻结一致性及双路电气安全没有闭环验证。**

## 已完成源码核对

- **PWM 失联/输入超时**：LCDM 在样本不 fresh 时调用 `Servo_Adaptive_Update(..., false)` 重置自适应样本种子，并将 H 桥 coast；同一处理路径也应对边沿长期不更新。失联实板测试未重做。
- **ADC 量程异常**：raw ADC ≤30 或 ≥4065 时，LCDM 立即 coast 并返回。2026-11-11 的旧代码未显式清除学习样本种子；2026-11-13 已补清除并做离板检查，2026-11-14 又补故障帧，均尚未上板验证。
- **异常速度/样本**：自适应模块拒绝低速、速度方向不符、速度瞬变、duty 跳变及超过 4ms 的样本间隔；LCDM 观察器把估算速度裁剪在 ±512，但没有发现独立速度故障检测/降级状态。
- **堵转**：`servo_control.c` 通用路径实现 stall timer/coast；LCDM 现场路径并未调用这个通用控制器，LCDM 未见等效堵转检测与安全停机。
- **供电异常**：有 VREFINT/VDD 监控结构和遥测相关字段；本 LCDM 驱动路径未发现以电压异常冻结学习并切入已定义输出状态的完整门控。
- **周期超时**：调度器以 1ms 快环服务，积压四次后重置时间基准并放弃 backlog；没有单独把超周期标记为故障、冻结学习并执行输出降级的路径。
- **H 桥互锁/死区**：普通 LCDM 换向分支先 coast 并等待 dead-time；客户 fixed profile 绕过该 guard。当前源代码逻辑及 PWM 单方向输出不能代替目标板两路波形测量；`HBridge_Brake()` 是 coast。

## 未通过原因与所需协助

- 所列故障类别并非都在同一 LCDM 快环内得到检测、学习冻结和明确安全状态；ADC 异常样本种子已在候选源码中重置，堵转保护主要只存在于通用环路。
- 未进行输入掉线、ADC 越界/突跳、堵转、异常速度、VDD 跌落和控制周期迟滞的逐类注入，也未测两路 H 桥互锁和 dead-time。
- **需要用户协助**：DAPLink 接入并交给 Ubuntu；仿真板接舵机/电位器，可安全限制电流/供电；示波器或逻辑分析仪用于观察两路 H 桥。外部 PWM 信号源不是必需条件，内部 PWM 可触发。

R01–R14 未通过项留待后续统一复核，本轮未重跑。

## 2026-11-13 LCDM ADC 故障学习基准修正

- 修改 `src/app_entry.c`：LCDM ADC 越界早退现在 coast 后调用 `Servo_Adaptive_Update(now_ms, 0, 0, false)`，清除 adaptive learner 的前一速度/占空比/时间种子；同时清零 LCDM observer seed，避免恢复后跨越 ADC 故障两侧配对样本。
- 新增 `tools/verify_r15_adc_fault_reset.py` 源码分支检查 **4/4 通过**：ADC fault coasts、通知 learner sample invalid、重置 observer seed、早退闭环。生产 adaptive learner 既有有效/无效样本回归 **15/15 通过**。证据在 `firmware/evidence/r15_adc_fault_20261113/report.json` 与 `firmware/evidence/r15_adaptive_learner_regression_20261113/`。
- 包含该修正的最新 V10.6SIMB2 构建成功，BIN 18,164B，SHA256 `cfbc4e7abca46f95fe1a4d264aa0478182035b44ce863b33445a0725386107a0`，manifest `build/releases/V10.6SIMB2_R03_R15_20261113/manifest.json`；未下载。
- **R15 仍未通过**：仅补齐 LCDM ADC 越界时 learner seed reset；堵转检测、VDD fault gate、快环超时统一降级、异常速度故障分类、customer profile 换向 guard 及实体双路 H 桥互锁均未完成/未测。无需用户协助做本项离板检查；后续实板需要仿真板、限流电源和双路示波器/逻辑分析仪。

## 2026-11-14 故障遥测完善与离板检查

修改 `src/app_entry.c`：ADC 越界、PWM 失联及 PWM 越界释放早退前发布零 duty 状态帧，事件分别为19/20/21，ADC无效时清零速度；`tools/update_customer_logic_vsg_compare.py` 增加事件名称。ADC无效帧中的 feedback 字段为无效原始 ADC，用事件标记区分；输入失联帧中的 PWM 值可能是最后一次采样，不代表输入有效。

`tools/r01_host_sim/test_fault_telemetry.py` 抽取实际早退分支，检查低/高 ADC、6次故障计数、learner无效通知、零 duty、错误字段一致、PWM失联/恢复条件和越界释放，离板通过；曲线发布函数和物理波形未做故障注入。方向保护80/120/160µs各17场景、R01 HOLD窄场景、R08 slew 7项与轨迹17项回归通过。Ubuntu内部诊断构建 `build/releases/V10.6SIMB2P160F_FAULT_20261114/` 通过，BIN 19344B、SHA256 `f78511151c91397ace899553581e3ec5ca2957ca5d6ef6132eaf05833cc5b521`，源码manifest一致，保留1536B最小栈，未下载。R15仍缺各类实板故障和H桥波形，整项未通过。

## 2026-11-15 ADC故障路径复跑

- 当前生产LCDM ADC无效早退块再次通过 **4/4**：coast、learner seed失效、LCDM observer seed重置、闭环立即早退。报告/源码哈希见`firmware/evidence/r15_adc_fault_20261115_rerun/report.json`。
- 这是离板源码检查，不增加实体ADC注入或双路H桥波形证据。堵转、异常速度、VDD异常、快环超时的统一冻结降级仍未完成；需要限流电源和双路示波器/逻辑分析仪做板上验证，当前没有用户需执行的按键操作。

## 2026-11-16 TSSOP20 低 VDD 降级补充

- `ADC_Feedback_IsVddBelow()` 仅在VDD样本有效且不超过100ms时判断欠压；无效或过期样本不会误报为测得欠压。欠压保护现同时进入TSSOP20 LCDM诊断路径和正式`Servo_Control_Update1ms()`生产路径：清除learner样本种子、coast、清除HOLD计时、回报`SERVO_STATE_VDD_LOW`；TJC LCDM显示`VDD LOW`。SOP8路径不包含本改动。
- R02 ADC+VDD主机模拟42/42；扩展后的R15 LCDM/生产控制源码检查13/13。历史统一TSSOP20回归为526/526；新增5项生产路径/界面检查见`firmware/evidence/r15_adc_fault_20261116/report.json`，SHA256 `002cf46d1bb5d736e34ce88202974c9430cd17226f181f9a3e740642e968ad18`。
- Ubuntu TSSOP20正式LCDM全量构建已通过：`build/releases/V10.6_TSSOP20_R13_R15_20261116/`，BIN 16,104B，SHA256 `8271acf54e4e42937a91e45511a567ce0ed542c2e7d2f24fe5c6dadf4bdba0b9`；ELF 213,384B，构建产物未下载。
- 欠压阈值目前沿用参数`vdd_warn_drop_mv`，须在实际供电与负载条件下确认不会误停；R15仍未通过。后续实体验证需要仿真板、限流供电，并观察两路H桥输出。
