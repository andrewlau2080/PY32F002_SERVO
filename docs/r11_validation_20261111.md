# R11 到位、HOLD 与外力偏移核对

初次核对：2026-11-11；最新源码增量：2026-11-15  
对象：工作区 V10.6 LCDM 与通用控制路径；板上当前为 R6 force-coast `P50 V10.6SIMB2C`  
结论：**连续稳定时间门控已完成离板修正与回归；R11仍部分通过，真实静稳/抗外力释放和实体 HOLD 输出尚未验。**

## 已核对内容

- LCDM 有多条 HOLD 进入条件，会参考误差和速度；进入时清零驱动、速度和输出方向状态，并把 HOLD anchor 设为命令目标。
- 从 HOLD 偏移后，代码需连续累计 8 次偏移确认才进入 static lock 输出；确认期间每次调用被条件打断会清零计数。另有 30 次低速偏差确认用于 load push。计数按调用次数，不按真实经过时间计算。
- 通用 `servo_control` 简化主机仿真曾有六段停入 HOLD 的结果，位置误差在模拟终点 ±5 ADC；其对象模型没有机械振动或用户施力释放，也不能代表 LCDM 快速环。
- `HBridge_Brake()` 是 coast；HOLD 期间不是主动保持电气制动。

## 未通过原因与限制

- 一条 LCDM 快速 HOLD 入口只要求无新命令、误差在 release band 内且速度低于门限，即直接调用 `lcdm_servo_enter_hold()`；没有先要求误差/速度连续稳定一段由真实时间定义的 settle duration。
- HOLD anchor 固定为 command target；外力偏移的确认按 fast-loop 调用次数，而非实际时间；误差门槛、重新纠偏时的冲击和释放后往返次数没有本版实测。
- 通用简化仿真不是 LCDM 测试，且无机械振动/外力对象。板上仍为 V10.5，candidate07 未下载，无长时 HOLD 或外力偏移曲线。

## 需要用户协助

- 将 DAPLink 接至 Mac 并分配给 Ubuntu；仿真板接舵机/电位器且可观察转臂。内部 PWM 可触发动作，不需要外部 PWM 源。
- 板测需记录长时间目标不变曲线，并轻推转臂后松开；同步记录 error、速度、HOLD anchor、状态/事件、signed duty、方向变化和时间戳，量稳定时间、偏移量及二次换向次数。

以上是2026-11-11初始核对结论；后续变更和验证以本页2026-11-15增量记录为准。R01本轮另有复跑，见`docs/r01_validation_20261110.md`。

## 2026-11-15 连续稳定时间修正（离板）

- 新增`ServoStabilityTimer`：首次合格样本只启动计时；只有条件连续成立并按真实tick累计到门槛才确认；观测间隔超过4ms会清除已累计时间，避免循环停顿被当作稳定时间。
- LCDM专用控制环中，普通快速HOLD入口和偏移确认已改为按真实时间计时：连续8ms后确认静态HOLD/偏移纠正；外力/负载确认从30次调用改为连续30ms。ADC无效、输入超界、目标变化等路径重置相关计时器。
- 正式通用`Servo_Control_Update1ms()`也接入稳定时间：到位需位置误差进入HOLD滞回带、有效速度不超过36 Q4/ms，并连续达到`hold_settle_ms`（默认20ms）；目标变化或>4ms采样间隔重新计时。ADC速度估算在输入失联时继续用于安全回中，但无效输入仍不会进入自适应学习。
- 边界回归`tools/r11_host_sim/test_stability_timer.c` **48/48通过**。生产`servo_control.c`六段对象模拟全部进入HOLD；由CSV重建的稳定候选至HOLD延时为20/26/26/20/26/20ms，**6/6通过**。数据`firmware/evidence/r11_hold_settle_20261115/simulation.csv`。这是软件模型结果，不代表实机稳定或外力恢复通过。
- 正式TJC LCDM Ubuntu构建通过，BIN 15,928B，SHA256=`4f55dc44337a45ea1d60de85fded74c3bd57658007cfd2a7ddf424eb4c6df843`；`_ebss=0x20000334`，保留1536B栈后另有716B RAM余量。构建目录`build/releases/V10.6_R11_STABILITY_TIME_PRODUCTION_R3_20261115/`，未下载。force-coast LCDM诊断构建也通过，BIN 19,180B，未下载。
- **R11仍部分通过**：两条软件路径的计数器已改为真实时间并完成离板对照；目标板长时间静稳、误差/速度门槛实效、轻推转臂后释放、二次换向和HOLD输出仍未验。当前板R6 force-coast，没有触发动作；实测须待R01方向安全证据具备后批量进行。

## 2026-11-19 当前源码复跑

- 重编译生产通用闭环六段host模型，并重跑`tools/r11_host_sim/verify_hold_settle.py`；稳定候选至HOLD为26/26/26/20/26/20ms，6/6达到至少20ms门槛。仿真CSV在`/tmp`完成检查后清理，未重复保存。
- 这是通用生产控制路径和合成对象结果；TSSOP20 LCDM实板的长时HOLD与轻推释放仍未验证，R11继续部分通过。
