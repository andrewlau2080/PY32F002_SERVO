# ServoParams v3 可调参数表

日期：2026-06-14

目标：按成熟舵机调机模板重新组织参数方向，同时保持 SOP8 目标板 Flash 参数页紧凑。V3 第一版结构体为 `128 B`，仍可放入最后一页 Flash 参数区。

## 1. V3 本次落地内容

| 项目 | 内容 |
|---|---|
| 参数版本 | `ServoParams.version = 3` |
| 参数大小 | `sizeof(ServoParams) = 128 B` |
| 通讯 payload | `SERVO_COMM_MAX_PAYLOAD = 128` |
| 通讯最大帧 | `SERVO_COMM_MAX_FRAME = 136` |
| 兼容性 | V2 Flash 参数因版本号不同会失效，固件自动回退到 V3 默认参数 |

## 2. V3 新增字段

| 字段 | 默认值 | 单位/格式 | 作用 |
|---|---:|---|---|
| `servo_angle_deg` | 180 | 度 | 舵机输出标称角度，例如 90/180 |
| `pot_angle_deg` | 220 | 度 | 电位器有效机械角度，用于映射和调机说明 |
| `lock_gain_q8` | 256 | Q8, 256=1.0 | 到位 HOLD 或外力反推时的锁力系数预留 |
| `counter_emf_gain_q8` | 256 | Q8, 256=1.0 | 反压/速度阻尼/反刹强度预留 |

说明：本次先把字段和校验落地，避免后续重写控制状态机时继续加临时常数。`lock_gain_q8` 和 `counter_emf_gain_q8` 会在下一步控制状态机重构中接入。

## 3. V3 新增标志

| 标志 | 默认 | 作用 |
|---|---|---|
| `SERVO_PARAM_FLAG_POT_REVERSE` | 关 | 电位器反馈方向/目标映射方向反转 |
| `SERVO_PARAM_FLAG_MOTOR_REVERSE` | 关 | H 桥电机输出方向反转 |

说明：成熟调机模板把“电位器方向”和“电机方向”分开设置。V3 已把两者拆开：`POT_REVERSE` 影响目标 ADC 镜像，`MOTOR_REVERSE` 影响 H 桥 duty 输出符号。旧的 `SERVO_PARAM_FLAG_INVERTER` 暂时保留为兼容目标映射反转。

## 4. V3 与成熟调机模板的对应关系

| 成熟模板项目 | V3 对应 |
|---|---|
| 信号输入最小/最大值 | `pulse_lower_us`、`pulse_center_us`、`pulse_high_us` |
| 电位器有效角度 | `pot_angle_deg`、`adc_min_count`、`adc_center_count`、`adc_max_count` |
| 舵机角度 | `servo_angle_deg` |
| 高中低位微调 | `left_range_count`、`neutral_offset_count`、`right_range_count` |
| 电位器方向 | `SERVO_PARAM_FLAG_POT_REVERSE` |
| 电机方向 | `SERVO_PARAM_FLAG_MOTOR_REVERSE` |
| 最大输出 | `max_duty` |
| 缓启动 | `soft_start_step_count`、`startup_step_count` |
| 锁力系数 | `lock_gain_q8` |
| 反压系数 | `counter_emf_gain_q8` |
| 堵转保护 | `stall_duty_threshold`、`stall_min_move_count`、`stall_time_ms`、`stall_recovery_ms` |
| 失信号 | `SERVO_PARAM_FLAG_LOSE_PPM_ENABLE`、`SERVO_PARAM_FLAG_LOSE_PPM_LOCK` |

## 5. 下一步应接入的控制逻辑

V3 参数只是第一步。下一轮控制重构应按以下顺序接入：

1. 新增 `state_detail/event_detail` 曲线变量，标出加速、减速、纯刹、反刹、HOLD、外力锁止。
2. 用 `servo_angle_deg/pot_angle_deg` 统一角度和 ADC 映射说明，LCDM/上位机显示角度时不再靠临时换算。
3. 用 `lock_gain_q8` 重构 HOLD 外力反推锁力。
4. 用 `counter_emf_gain_q8` 重构速度阻尼和反刹，避免继续用分散常数试过冲。
5. 后续若 128B 不够，再设计 V4 或扩展参数页，不在 V3 里继续硬塞字段。
