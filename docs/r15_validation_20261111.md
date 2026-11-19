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

## 2026-11-19 customer profile 换向保护复核

- LCDM customer profile 以固定阶段输出：驱动7ms、反向制动2ms、反向尾段3ms，之后coast。`customer_profile_direct`绕过通用8ms换向死区和slew guard；该路径是为保持参考波形而设计，直接套用8ms死区会吞掉/改变其2ms与3ms阶段，不能当作无影响的安全修补。
- 当前不修改该时序。离板可做的是抽取profile阶段和异常输入早退条件，核对其状态转移；要决定如何兼顾双路互锁与客户波形，仍需对应客户波形映射及目标板双路输出测量。R15整项仍未通过。

## 2026-11-19 Ubuntu force-coast 构建推进

- 首次链接暴露两个源码问题：非诊断配置误调用只在内部步进宏下定义的`lcdm_r01_stage_record()`，已加宏保护；force-coast版本标签被错误嵌套在内部步进条件中，已修正。`--disable-tjc-rx`诊断选项可裁掉不需要的触摸RX，产品默认配置不变。
- 无触摸RX版本已成功链接，BIN 19,416B，临时SHA256=`f80c4186a1fcf857ac33c13e909c553a673819ba322d70f699f9d53b933ae137`；静态RAM `_ebss=0x200005d8`，预留1,536B栈后余40B。该次构建因版本字符串门槛拒绝归档；修正标签后新构建待完成。板未下载、未动作，R15仍未通过。
- 标签调整后构建器本地校验因版本解析正则过窄再次退出；已改为允许`V10.x`后的字母数字诊断后缀。该次在连接Ubuntu前退出，无新构建产物、无板操作；需重新构建确认。
- 后续Ubuntu编译发现移动版本标签时漏闭合内部步进宏，导致LCDM runtime预处理块未闭合；现已补齐条件层级。该次没有下载或动作，修正待重新构建。

## 2026-11-19 调度丢周期即时冻结

- `src/app_entry.c`的LCDM runtime scheduler现于丢弃tick时coast、调用`Servo_Adaptive_Update(..., false)`使学习基准失效，并复位速度/轨迹观察基准、清除未完成的customer profile阶段。修正边界仅覆盖该runtime scheduler检测到的丢周期；未加入未经定义的超时阈值，也未宣称已覆盖所有快环入口。
- 本机尝试用隔离临时目录编译TSSOP20 force-coast + LCDM runtime配置，但本机缺少项目HAL SDK，未能完成编译；无测试运行、无固件下载、无动作验证。R15仍未通过，后续须先在项目Ubuntu工具链构建，再用force-coast板况检查遥测，最后才安排实物故障与双路互锁验证。
- Ubuntu项目构建已可达链接阶段；首次结果报告RAM超出16B、Flash超出244B，以及非诊断构建误引用`lcdm_r01_stage_record()`。现已修正该条件编译调用；新构建待执行，RAM/Flash余量是否仍超限尚未知。无测试、无下载、无动作，R15保持未通过。

## 2026-11-19 调度丢tick降级候选构建与现板检查

- 上述待构建状态已更新：项目Ubuntu工具链构建通过，产物`build/releases/V10.6_TSSOP20_R04_R15_OVERRUN_FORCE_COAST_20261119/`。BIN 19,420B，SHA256=`ff23b26b2851d64c27f85301f8732f1be4334c44be61306464483b52d997e6be`；force-coast开启、触摸RX关闭、无主动运动诊断；未下载。构建只证明源码可编译链接，不证明硬件降级行为。
- 新增故障处理仅针对LCDM runtime scheduler已丢弃tick：coast、学习样本失效、状态BAD并清理observer/trajectory/reverse/customer-profile阶段。尚未覆盖堵转、异常速度分类、所有调度入口或customer profile H桥波形；R15仍未通过。
- 现板只读识别为R25 force-coast，完整20,480B SHA256=`f7c62dd81d97687ff41a93ef1ca51383fbfd9665e8a6d81f09405296a3ac8fa3`，核心Running。与新候选不同，故未把候选写入板上。匹配R25 ELF只读监控52帧：feedback/raw ADC最高4095，四点样本范围分别为3500–4095、3824–4095、4095固定、4095固定；VDD估值3250–3267mV；duty始终0。此数据新增支持R02取值链路调查，不能单独证明同一时刻PA4电压或焊盘状态；详见R02记录及`firmware/evidence/r04_r15_force_coast_readonly_20261119/`。
- 新候选故障降级路径需在force-coast版本下载后才可板上验证；本轮无下载、无动作。后续要完整验收R15仍需按故障类别注入并核对双路H桥波形，需可控限流供电及示波器/逻辑分析仪；眼下没有需要用户按键或输入信号的事项。
- **后续板上状态**：force-coast候选已按under-reset下载并读回验证（20KB整片SHA256=`f44ef9b3d69ab55286b8f60c27a5d463e53f4ab75fea2cef3c586a82062c5e7d`，应用匹配、尾384B保持、Running）。30秒只读窗口120帧、duty=0，scheduler丢tick计数28→28；没有触发新的overrun，因此新增的丢tick故障分支尚未实机覆盖。ADC_LOST状态持续，raw最高4095，不能把此静态结果算作R15通过。当前板force-coast诊断固件，电机动作仍被编译时禁用。
- 后续受控RAM注入：`tools/trigger_r04_dropped_tick_swd.py`只回拨scheduler RAM时间戳20ms；要求force-coast标签、目标Running和初始duty=0，不复位/不写Flash。板上scheduler丢tick由28增至53，单次最大25，overrun上下文由51增至4147；注入后再采5秒/20帧，计数稳定53、duty恒0、核心Running。该项覆盖R04超时分支的执行和零duty软件状态，不证明其他故障路径，也不等于R15整体通过。证据同R04目录。
