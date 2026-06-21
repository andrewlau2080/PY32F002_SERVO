# Servo Curve Debug Variables

2026-06-13

## 目的

用调试器/VarViewer 同时观察舵机运动时的目标、反馈、误差、速度、驱动和状态，避免只凭 LCDM 瞬时数字判断。

## 推荐曲线

第一张图看位置：

| 变量 | 含义 |
|---|---|
| `g_servo_curve_target_adc` | PWM 指令换算后的目标 ADC 位置 |
| `g_servo_curve_feedback_adc` | 控制使用的反馈 ADC 位置 |
| `g_servo_curve_traj_adc` | 轨迹规划器当前给位置环的中间目标 |
| `g_servo_curve_overshoot_adc` | 本次动作最大过冲 ADC count |

第二张图看控制：

| 变量 | 含义 |
|---|---|
| `g_servo_curve_error_adc` | 目标 ADC - 反馈 ADC |
| `g_servo_curve_velocity_q4` | ADC 估算速度，q4 格式 |
| `g_servo_curve_ref_velocity` | 速度环目标速度，ADC count/ms |
| `g_servo_curve_duty` | H 桥输出 duty，正负代表方向 |
| `g_servo_curve_state` | 状态：1 HLD，2 DRIVE，9 BRK |
| `g_servo_curve_event` | 控制事件编号 |

## Event 编号

| 编号 | 含义 |
|---|---|
| 1 | 正常 DRIVE |
| 2 | HOLD |
| 3 | 普通 duty=0 Brake |
| 4 | 过点后继续远离目标，触发 overshoot Brake |
| 5 | 末端仍朝目标冲，触发 terminal Brake |
| 6 | 低 duty 抖动抑制 Brake |
| 7 | 严格末端 HLD 屏蔽 |
| 8 | 方向报警/保护 |
| 9 | 过冲后返回 duty 限幅 DRIVE |
| 10 | 过冲后接近目标且速度低，进入安静 HOLD |
| 11 | 固定目标锁止 DRIVE，用于 PWM 未变但偏离目标后的反向锁止 |
| 12 | V3 轨迹/速度环正常 DRIVE |
| 13 | V3 接近目标减速 DRIVE |
| 14 | V3 预测刹车 BRAKE，H 桥只刹车不反向驱动 |
| 15 | V3 过点/停短后的受限 RETURN，限制回拉 duty |
| 16 | V3 HOLD 后外力偏移锁止，按 `lock_gain_q8` 输出 |
| 17 | V3 RETURN 回目标途中近目标速度吸能刹车，减少二次/三次回摆 |
| 18 | V3 刚过目标点且速度仍向外冲时刹车吸能，避免立刻反打造成回摆 |

## 用户参考图判断

用户提供的第一张图像是角度曲线，台阶形目标/角度变化明显，重点是角度台阶切换后几乎没有超过目标再反弹，说明刹车和末端控制做得比较好。

第二张图像像是“角度/目标”叠加“控制输出或误差”的曲线：蓝色是角度或目标位置，灰色在每次阶跃时出现正负尖峰，很可能代表马达驱动 duty、电流近似量、误差或速度。灰色尖峰在切换瞬间很大，稳定段接近 0，这正是我们现在需要观察的内容：运动时要有足够驱动，接近目标时要快速收敛到 0，不能在稳定段持续小幅正反跳。

## 当前重点

对比以下动作：

- `1000 -> 1500`
- `1500 -> 1000`
- `1500 -> 2000`
- `2000 -> 1500`
- `1000 -> 2000`
- `2000 -> 1000`

每次记录：

- `target_adc` 是否到位且稳定
- `feedback_adc` 是否过冲
- `velocity_q4` 是否在目标附近仍很大
- `duty` 是否在稳定段继续 `+/-20..70` 跳动
- `event` 是否频繁在 4/5/6 之间跳

这些数据能判断下一步应该调轨迹速度、速度环增益、刹车窗口，还是 ADC/机械映射。
