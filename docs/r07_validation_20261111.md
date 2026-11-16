# R07 控制参数边界与置信度验证

日期：2026-11-11  
对象：工作区 V10.6 candidate07 的生产自适应模块（当前板上仍为 V10.5）  
结论：**局部软件边界和渐进融合通过 15/15；R07 整机要求未通过。**

## 已完成核对

直接编译并调用生产 `src/servo_adaptive.c`，使用合成数据检查增益、前馈和单步在线更新：

- 速度增益按方向独立；置信度为 0 时返回基准值，500/1000 时按比例逐步融合。
- 速度增益修正限制在基准值的 0.5–2 倍，即使模型比值超界也不越界。
- 前馈随置信度从 fallback 渐进到模型值，并受 `max_duty` 限制；零参考速度或零上限时返回 fallback。
- 速度响应估计使用 1/8 EMA；样本计数带来的置信度变化也逐步融合。
- 普通 LCDM 闭环分支调用 `Servo_Adaptive_GetVelocityGain()` 和 `Servo_Adaptive_GetFeedForwardDuty()`；`customer_profile_direct` 分支绕过此自适应修正。

结果为 **15/15**，运行结果及源码哈希保存在 `firmware/evidence/r07_control_adjustment_20261111/`。这是生产 C 模块的合成输入检查，不是板上控制效果证明。

## 未通过原因与限制

- 未在当前板上固件验证参数改变时的实际输出、响应连续性或是否出现突跳；最新候选尚未下载。2026-11-12曾通过 Ubuntu/DAPLink完成板上恢复，当前运动受方向及同步采集安全门槛限制。
- 增益的最终输出有 0.5–2 倍范围和置信度融合，但代码没有独立的每周期输出变化率限制；EMA 约束模型更新速度，不等价于对控制输出变化率的硬限制。
- 客户固定 profile 直接路径跳过上述增益/前馈调整，因此不能声称所有运行模式都满足自动修整要求。
- 未做真实舵机、电源、负载和方向切换的日志核对，也没有证明参数变化后性能改善。

## 需要用户协助

- 当前无需用户操作。方向沿用用户此前定论；后续实测前核对 DAPLink、板上版本和备份，并准备同步采集，以核对当前固件的实际增益、置信度和输出响应。
- 实测需要仿真板连接舵机和电位器、可观察转臂；内部 `INTPWM` 可提供重复命令，不需要外部 PWM 信号源。
- 板上测试时需允许采集命令、feedback、signed duty、模型 gain/confidence、最终 duty 和时间戳，覆盖置信度建立、正反向和不同响应条件。

此前未通过项继续留待后续统一复核，不在本轮重跑 R01–R06。

## 2026-11-15 最终增益变化率限制（离板）

- `Servo_Adaptive_GetVelocityGain()`增加正反方向独立的最终输出记忆。首次调用从固定基准增益起步，之后每次控制调用最多变化`ceil(base_gain/64)`，目标仍受0.5–2倍基准范围及置信度融合约束；置信度或辨识目标变化时，输出逐周期靠近新目标。新增`g_servo_adaptive_applied_velocity_gain_q8_forward/reverse`供SWD监控；固定 customer profile 仍不调用普通自适应路径。
- R07生产模块回归22/22通过，覆盖变化上/下限、单周期限幅、目标收敛、置信度融合和方向状态隔离。R03时间归一回归19/19、R05学习质量15/15、R06合成对象响应10/10通过；R06用例现按控制周期等待渐进收敛，不再要求单次立即跳到新目标。证据分别位于`firmware/evidence/r07_control_adjustment_20261115_ratelimit_final/`、`r07_r03_regression_20261115/`、`r07_r05_quality_regression_20261115/`及`r07_r06_object_regression_final_20261115/`。
- Ubuntu `--internal-step-test --force-coast` TSSOP20诊断构建通过，BIN 18,812B、SHA256=`5ac6891cb7e3ba55213fedf3515f2107b89a685baa64af4c17709e0472ad21ff`，manifest在`build/releases/V10.6_R07_GAIN_RATE_LIMIT_FORCE_COAST_20261115/`；未下载。
- **2026-11-15 LCDM运行时UI RAM压缩构建**：曾因栈保留断言超144B；普通日志环容量从16条减至8条（160B），`force_servo_curve_log.py`、`dump_servo_curve_log.py`和ring monitor均按ELF符号大小动态计算容量。Ubuntu非internal-step LCDM runtime UI构建通过，BIN 20,092B、SHA256=`7dbb85a6599a9a3841a8d400e1a1e74c883400d86d12ee309d129e3cf54bf868`，manifest `build/releases/V10.6_R07_LCDM_RAM_COMPACT_R2_20261115/`。静态RAM为1528B，超过1536B保留门槛的剩余空间仅8B；应用Flash上限20,096B，剩4B。该配置未强制coast，严禁当作已批准候选或下载；只作为RAM/链接验证产物。新诊断R04构建因12B遥测在早期链接检查中失败，后以16位饱和计数重建通过。
- **2026-11-15 正式TJC LCDM路径构建**：新增 `--production-lcdm` Ubuntu build mode，启用`SERVO_ENABLE_TJC_LCDM=1`与通信模块，绕过runtime UI测试入口；修正`app_model_store_init_context()`被错误限制在测试编译块的问题，并保留可读版本字符串。正式TSSOP20构建通过，BIN 15,716B、SHA256=`32048638018c2ea377a757711cca79750ca7c15d27f417e06cb7dbe569a82f83`，manifest `build/releases/V10.6_R07_PRODUCTION_LCDM_R4_20261115/`，未下载。链接图`_ebss=0x2000032c`，RAM 3KB减静态区812B及保留栈1536B后，额外余量724B；可用Flash 20,096B余4,380B。日志环容量为8，所有dump/monitor工具从ELF符号大小读取，不依赖固定容量。此前20,092B runtime UI测试构建仍留档，仅供诊断。
- **结论**：R07的增益变化率已离板通过，正式TJC LCDM配置现在构建通过且有可用余量；R07整机仍未通过，因为customer profile绕过普通自适应路径，正式候选未下载，正反向真实增益/置信度/负载效果未实测。

## 2026-11-15 当前源码复建

- 在R04 SysTick诊断改动后重新构建正式 TJC LCDM 配置，确认新增诊断仅在诊断宏下编译，正式产品配置仍可构建。构建目录：`build/releases/V10.6_R07_PRODUCTION_LCDM_R5_20261115/`；BIN 15,716B，SHA256=`32048638018c2ea377a757711cca79750ca7c15d27f417e06cb7dbe569a82f83`，与R4产物相同；构建通过、未下载。
- 复核R07生产C模块回归仍为22/22通过（结果见`firmware/evidence/r07_control_adjustment_20261115_ratelimit_final/results.txt`）。这验证边界和每周期增益变化率，不代表板上增益/置信度/前馈实际生效。
- 当前板仍为R6 force-coast诊断固件，整片Flash读回SHA256=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`。R07尚未下载和实机测试；方向映射沿用此前定论。正式驱动固件仍需同版构建、备份及安全下载流程，实测需同步采集当前输出和反馈。
- R07尚缺：正反向实际增益/置信度与最终duty同步记录、不同响应/负载的现场效果、customer profile绕过策略的产品决定。当前无需用户额外操作；离板独立工作继续，实机驱动测试前再确认机构观察和方向安全。
