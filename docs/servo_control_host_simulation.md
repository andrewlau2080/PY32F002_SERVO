# 通用闭环主机模拟记录

日期：2026-11-09  
范围：验证通用控制源码的命令映射、反馈误差、方向/占空比、HOLD 状态切换和六段指令路径。不是目标板验收。

## 运行方式

在项目目录执行：

```sh
cc -std=c11 -Wall -Wextra -Werror -Itools/host_sim -Iinc \
  tools/servo_control_host_sim.c src/servo_control.c src/servo_adaptive.c \
  -o /private/tmp/servo_control_host_sim
/private/tmp/servo_control_host_sim build/servo_control_host_sim.csv
```

仿真直接编译生产文件 `src/servo_control.c` 和 `src/servo_adaptive.c`。`tools/servo_control_host_sim.c` 提供确定性的 PWM 输入、ADC 位置反馈、毫秒时钟和 H 桥 duty 接口；`tools/host_sim/py32f0xx_hal.h` 只为主机编译提供 `HAL_GetTick()` 声明。

马达模型是简化的一阶惯性模型：施加 duty 产生比例驱动力，速度以固定比例衰减，位置积分并限制在 ADC 200–3900。参数将输入范围设为 500–2500 μs、中心 1500 μs、ADC 端点 200/3900。每段先回到起点并等待稳定，再改变为终点命令。

## 六段结果

| 指令 | 结束状态 | 最终位置误差（target − feedback） | 该模拟模型中的方向过冲 |
|---|---:|---:|---:|
| 1000→1500 μs | HOLD | +4 ADC | 18 ADC |
| 1500→2000 μs | HOLD | −1 ADC | 16 ADC |
| 2000→1500 μs | HOLD | +1 ADC | 16 ADC |
| 1500→1000 μs | HOLD | −4 ADC | 18 ADC |
| 1000→2000 μs | HOLD | +5 ADC | 17 ADC |
| 2000→1000 μs | HOLD | −5 ADC | 17 ADC |

每段 CSV 约 302 个控制周期；仿真中峰值 duty 为 278。数据文件 `build/servo_control_host_sim.csv` 保存了每毫秒 PWM 命令、目标/反馈 ADC、误差、duty、状态和模型置信度。

## 结论边界

- 可确认：通用 `Servo_Control_Update1ms()` 在这个设定模型中完成六段输入映射、正反向驱动，并进入 HOLD；代码能在主机上编译运行。
- 不可据此确认：V10.5 LCDM 控制路径与通用环路行为一致；真实机构能达到上述时间/过冲/静稳；反向 PWM 制动正确；H 桥两路互锁和载波正确；负载、电压、齿隙或电位器噪声变化后的自适应效果。
- `HBridge_Brake()` 在当前生产源码中调用 `HBridge_Coast()`。因此该模拟没有验证吸能制动；R09/R10 仍未通过。
- 主机模拟结果只作为源码路径回归证据。下载前仍需现场 VSG/逻辑分析仪数据与 CUSTOMER 参考对照。
