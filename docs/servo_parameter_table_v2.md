# ServoParams v2 可调参数表

目标：把历史调试中遇到的问题拆成独立参数，底层程序保持固定，后续通过配置器、LCDM 或生产工装只改 Flash 参数页。

参数结构：`ServoParams` v2。当前结构体约 120 字节，仍放在最后 128 字节 Flash 参数页内。

## 1. 型号与模板

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `model_id` | 0 | 编号 | 舵机型号或齿轮箱类型 | 金属齿、塑胶齿不能共用同一套参数 |
| `profile_id` | 0 | 编号 | 快速、标准、安静、高保持力等模板 | 同一硬件按客户需求切换手感 |

## 2. 输入与角度校准

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `pulse_lower_us` | 900 | us | 输入脉宽下限 | 兼容 1000us 附近输入 |
| `pulse_center_us` | 1500 | us | 输入中位 | 中位校准 |
| `pulse_high_us` | 2100 | us | 输入脉宽上限 | 兼容 2000us 附近输入 |
| `adc_min_count` | 600 | ADC count | 左端反馈位置 | 角度范围不足 |
| `adc_center_count` | 2048 | ADC count | 中位反馈位置 | 中位偏差 |
| `adc_max_count` | 3495 | ADC count | 右端反馈位置 | 角度范围不足 |
| `neutral_offset_count` | 0 | ADC count | 整体中位偏移 | 装配和电位器中点误差 |
| `left_range_count` | 0 | ADC count | 左行程微调 | 左角不足或顶死 |
| `right_range_count` | 0 | ADC count | 右行程微调 | 右角不足或顶死 |

## 3. 死区与 ADC 抗干扰

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `deadband_us` | 2 | us | 按输入灵敏度定义死区 | 1-3us 灵敏度和抖动平衡 |
| `deadband_count_min` | 8 | ADC count | 最小 ADC 死区 | ADC 噪声导致误动作 |
| `adc_sample_count` | 4 | 次 | 每次反馈平均采样次数 | 随机噪声 |
| `adc_filter_shift` | 3 | IIR shift | 滤波强度，越大越稳但越慢 | AD 超过 1V 后抖动 |
| `adc_jump_limit_count` | 0 | ADC count | 单次 ADC 跳变限幅，0 关闭 | 电机换向尖峰串入 ADC |
| `adc_noise_band_count` | 4 | ADC count | 噪声评估预留阈值 | 后续自动报警或自动放宽死区 |

## 3A. 电源监测

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `vdd_nominal_mv` | 3300 | mV | MCU/ADC 标称供电 | 建立 VDD 跌落判断基准 |
| `vdd_warn_drop_mv` | 250 | mV | 跌落阈值 | 判断电机动作时 3.3V 是否被拉低 |
| `vdd_noise_band_mv` | 80 | mV | 抖动阈值 | 判断供电/地线扰动是否会影响 ADC |
| `vdd_sample_interval_ms` | 20 | ms | VREFINT 采样间隔 | 同步记录位置 ADC 和供电变化 |

说明：电位器若由同一个 3.3V 供电，且 ADC 参考也是 3.3V，慢速电源变化理论上会按比例抵消。这里监测 VDD 的目的主要是诊断电机电流造成的瞬态跌落、地弹和参考噪声；这些问题不能只靠数学补偿解决，仍需要 LDO、地线、滤波电容和 ADC 前端硬件配合。

## 4. 上电防乱跑

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `startup_delay_ms` | 200 | ms | 上电后先关闭马达等待 | 电源/ADC/PWM 未稳时乱跑 |
| `startup_input_stable_count` | 5 | 次 | 连续稳定 PWM 数量 | 上电瞬间读到错误脉宽 |
| `startup_input_stable_us` | 3 | us | PWM 稳定判定阈值 | 输入抖动 |
| `startup_adc_stable_count` | 5 | 次 | 连续稳定 ADC 数量 | ADC 初始值漂移 |
| `startup_adc_stable_band_count` | 4 | ADC count | ADC 稳定判定阈值 | 上电反馈未稳定 |
| `startup_step_count` | 4 | ADC count/ms | 首次追目标的目标斜坡速度 | 当前位置和命令位置差很大时冲击 |

## 5. 分段控制输出

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `close_error_count` | 24 | ADC count | 小误差区上限 | 到位附近抖动 |
| `close_stretcher_q8` | 192 | Q8, 256=1.0 | 小误差区增益 | 锁附时正反抖 |
| `close_boost_duty` | 60 | duty/1000 | 小误差区起动补偿 | 到位嗞嗞声和静态电流 |
| `approach_error_count` | 120 | ADC count | 接近目标区上限 | 末端慢跨 |
| `approach_stretcher_q8` | 320 | Q8, 256=1.0 | 接近目标区增益 | 快到点时拖尾或晃过去 |
| `approach_boost_duty` | 120 | duty/1000 | 接近目标区起动补偿 | 克服末端摩擦但避免过冲 |
| `stretcher_q8` | 384 | Q8, 256=1.0 | 大误差区增益 | 大范围移动速度和力度 |
| `boost_duty` | 160 | duty/1000 | 大误差区起动补偿 | 克服静摩擦 |
| `max_duty` | 850 | duty/1000 | 最大驱动占空比 | 限流、保护齿轮和驱动 IC |
| `forward_speed_q8` | 256 | Q8 | 正向速度比例 | 左右速度不一致 |
| `reverse_speed_q8` | 256 | Q8 | 反向速度比例 | 左右速度不一致 |

## 6. 到位保持与嗞嗞声

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `hold_mode` | 2 | 0 滑行 / 1 刹车 / 2 短刹后滑行 | 到位后的 H 桥策略 | 到位静态电流、发热、马达声 |
| `hold_exit_band_count` | 16 | ADC count | HOLD 退出阈值，形成滞回 | 死区边缘反复驱动 |
| `hold_brake_time_ms` | 8 | ms | 短刹车持续时间 | 到位快速停住但不长期耗电 |
| `hold_settle_ms` | 20 | ms | 到位稳定时间预留 | 后续更严格到位判定 |
| `brake_band_count` | 3 | ADC count | 旧版小误差刹车范围 | 保留兼容，v2 主要使用 hold_mode |

## 7. 反向切换保护

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `reverse_pause_ms` | 8 | ms | 正反切换前暂停 | 未到位反向导致瞬间大电流 |
| `reverse_brake_ms` | 3 | ms | 暂停前段短刹车 | 降低反向机械冲击 |

## 8. 保护与失信号

| 字段 | 默认值 | 单位/格式 | 作用 | 主要解决问题 |
|---|---:|---|---|---|
| `stall_duty_threshold` | 500 | duty/1000 | 堵转判定占空比门槛 | 大电流堵转 |
| `stall_min_move_count` | 4 | ADC count | 堵转期间最小反馈变化 | 区分真堵转和慢速移动 |
| `stall_time_ms` | 300 | ms | 堵转持续时间 | 防止误触发 |
| `stall_recovery_ms` | 500 | ms | 堵转恢复等待 | 防止反复冲击 |
| `input_timeout_ms` | 100 | ms | PWM 失信号超时 | 接收机断信号 |

## 9. 功能标志

| 标志 | 默认 | 作用 |
|---|---|---|
| `SERVO_PARAM_FLAG_INVERTER` | 关 | 目标方向反转 |
| `SERVO_PARAM_FLAG_OL_PROTECT` | 开 | 堵转/过载保护 |
| `SERVO_PARAM_FLAG_PWM_100_PERCENT` | 关 | 满占空比策略预留 |
| `SERVO_PARAM_FLAG_NARROW_BAND` | 关 | 窄频/窄行程模式预留 |
| `SERVO_PARAM_FLAG_SOFT_START` | 开 | 目标位置软启动 |
| `SERVO_PARAM_FLAG_LOSE_PPM_ENABLE` | 关 | 失信号回中位 |
| `SERVO_PARAM_FLAG_LOSE_PPM_LOCK` | 开 | 失信号锁当前位置 |

## 10. 调试优先顺序

| 顺序 | 先调什么 | 原因 |
|---:|---|---|
| 1 | 3.3V 稳压、ADC 原始值、`adc_sample_count`、`adc_filter_shift` | ADC 不稳时，控制参数怎么调都会抖 |
| 2 | `vdd_*` 遥测 | 判断 ADC 抖动是否和供电跌落同步 |
| 3 | `startup_*` | 先保证上电不乱跑 |
| 4 | `adc_min/center/max`、`left/right_range_count` | 先确定角度和中位 |
| 5 | `deadband_*`、`hold_*` | 先解决到位电流和嗞嗞声 |
| 6 | `close_*`、`approach_*`、`stretcher_q8` | 再调速度、锁附和末端手感 |
| 7 | `reverse_*`、`stall_*` | 最后做保护和极限动作 |
