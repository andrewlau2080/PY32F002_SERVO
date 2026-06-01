# PY32F002 舵机参数通讯协议草案

版本：v0.2

目标：同一套 `ServoParams`、`ServoTelemetry` 和 `ServoComm` 帧格式同时服务三种场景：PA4 单线配置器、开发板 LCDM 调试界面、生产写参数工装。最终 SOP8 目标板不需要直接驱动 LCDM；LCDM 属于开发板/工装前端。

## 1. 设计原则

| 原则 | 要求 | 原因 |
|---|---|---|
| PWM 优先 | 上电默认按普通 PWM 舵机工作 | 避免正常接收机信号被误判为配置通讯 |
| 明确握手 | 配置器和舵机必须完成 Break + Sync + Hello/Reply | 防止干扰、失信号或错误脉冲触发写参数 |
| 半双工 | 配置器发送时舵机只听，舵机回复时配置器释放信号线 | PA4 只有一根信号线 |
| 一包全参数 | 写参数命令 payload 直接包含完整 `ServoParams` | 避免多个参数分散写入造成半更新状态 |
| 读回确认 | 写入后必须 `READ_PARAMS` 再比对 CRC | 确认 Flash 中实际保存的参数 |
| 遥测可读 | 配置器可随时读取 `ServoTelemetry` | 上位机或 LCDM 模块可以直观看到舵机状态 |
| 参数带 CRC | `ServoParams` 自带 CRC32，通讯帧再带 CRC16 | 区分参数存储错误和通讯错误 |

## 1A. 开发板 LCDM 模式

开发板和最终目标板不共用 PCB，因此 LCDM 不受 SOP8 IO 限制。开发板可用 USART1 连接串口 HMI 智能屏，目标板仍只保留舵机控制和参数页。

| 项目 | 定义 |
|---|---|
| LCDM 物理接口 | USART1，建议 115200 bps 或 57600 bps，8N1 |
| LCDM 类型 | UART HMI 智能屏，屏幕端负责页面、条形图、触摸按钮 |
| PY32 任务 | 发送遥测变量，接收参数修改命令，不直接绘制像素 |
| 协议 | 复用本文第 4 节帧格式和第 5 节命令表 |
| 最终目标板 | 不接 LCDM；生产时写入已调好的 `ServoParams` |

开发板 LCDM 和 PA4 单线配置可以共用上层包处理函数 `Servo_Comm_HandleFrame()`，只是在底层收发不同：

| 前端 | 底层收发 | 上层处理 |
|---|---|---|
| PA4 单线配置器 | 单线半双工，Break + Sync 后进入配置 | `ServoComm` |
| 开发板 LCDM | USART1 全双工或半双工串口 | `ServoComm` |
| PC 生产工装 | USB-UART 或 SWD 辅助下载 | `ServoComm` 或直接写参数页 |

LCDM 页面规划和选型见 `docs/lcdm_development_interface.md`。

## 2. 物理层

本节主要描述最终舵机 3PIN 信号线上的 PA4 单线配置方式。开发板 LCDM 模式可以直接使用 USART1，不必占用 PA4，也不受 SOP8 目标板 IO 限制。

| 项目 | 定义 |
|---|---|
| 信号线 | 舵机 3PIN 的 S 线，接 PY32 PA4 |
| 电气方式 | 单线半双工，建议开漏/开集电极或串阻隔离，空闲为高 |
| 电平 | 与舵机 MCU VCC 同电平，配置器必须检测或匹配 VTref |
| 普通 PWM | 50 Hz，默认识别 900-2100 us 高脉冲 |
| 配置通讯 | 建议 19200 bps，8N1，LSB first，空闲高，低电平为起始位 |
| 方向切换 | 发送方发送完成后释放线至少 2 ms，再由另一方回复 |
| 马达状态 | 进入配置模式后舵机必须停止 H 桥输出 |

硬件建议：

| 位置 | 建议 |
|---|---|
| 配置器 S 线输出 | 串 1k-4.7k 电阻，避免与接收机或 MCU 输出冲突 |
| 舵机 PA4 | 保持输入为默认状态；仅回复时短时切为开漏输出 |
| 配置模式调试 | 示波器同时看 S 线和 VCC，先确认无总线冲突 |

## 3. 配置模式握手

| 步骤 | 配置器动作 | 舵机动作 | 判定 |
|---:|---|---|---|
| 1 | 上电或接入舵机后等待 200 ms | 正常按 PWM 模式运行或失信号保护 | 不进入配置 |
| 2 | 将 S 线拉低 60-100 ms | 检测到超出普通 PWM 的长低电平 | 进入候选配置状态，停止 H 桥 |
| 3 | 释放 S 线为高 8-20 ms | 等待同步字 | 未收到同步则返回普通 PWM |
| 4 | 发送同步字节 `0x55, 0xAA` | 检查位宽和同步序列 | 同步失败则退出配置 |
| 5 | 发送 `HELLO` 帧 | 回复 `HELLO` 帧 | 双方确认协议版本、结构体长度和能力 |
| 6 | 配置器发送 `ACK` 或继续命令 | 舵机保持配置模式 | 允许读写参数和读遥测 |

超时规则：

| 场景 | 超时 | 动作 |
|---|---:|---|
| Break 后无 Sync | 100 ms | 舵机返回普通 PWM 模式 |
| 收到 Sync 后无合法 `HELLO` | 100 ms | 舵机返回普通 PWM 模式 |
| 配置模式无任何合法帧 | 3000 ms | 舵机返回普通 PWM 模式 |
| 写参数后无读回确认 | 由配置器决定 | 建议提示失败，不继续下一台 |

## 4. 帧格式

所有多字节字段使用 little-endian。

| 字节偏移 | 字段 | 长度 | 说明 |
|---:|---|---:|---|
| 0 | SOF | 1 | 固定 `0x7E` |
| 1 | Version | 1 | 当前 `0x01` |
| 2 | Command | 1 | 命令码 |
| 3 | Length | 1 | Payload 字节数，最大 120 |
| 4 | Payload | N | 命令数据 |
| 4+N | CRC16_L | 1 | CRC16 low byte |
| 5+N | CRC16_H | 1 | CRC16 high byte |
| 6+N | EOF | 1 | 固定 `0x7E` |

CRC16：

| 项目 | 定义 |
|---|---|
| 算法 | CRC16/Modbus 多项式 `0xA001` |
| 初值 | `0xFFFF` |
| 计算范围 | `Version + Command + Length + Payload` |
| 字节顺序 | 低字节在前 |

说明：帧采用长度字段确定 payload 结束位置，因此 payload 中出现 `0x7E` 不需要转义。若后续实测抗干扰不足，再升级为 SLIP/HDLC 转义。

## 5. 命令表

| 命令 | 值 | 方向 | Payload | 回复 | 说明 |
|---|---:|---|---|---|---|
| `HELLO` | `0x01` | 配置器 -> 舵机 | 空 | `ServoCommHello` | 读取协议版本、结构体长度和能力 |
| `READ_PARAMS` | `0x02` | 配置器 -> 舵机 | 空 | `ServoParams` | 读当前运行参数 |
| `WRITE_PARAMS` | `0x03` | 配置器 -> 舵机 | 完整 `ServoParams` | `ACK/NACK` | 校验并写入最后一页 Flash |
| `READ_TELEMETRY` | `0x04` | 配置器 -> 舵机 | 空 | `ServoTelemetry` | 读实时硬件和控制状态 |
| `EXIT_CONFIG` | `0x05` | 配置器 -> 舵机 | 空 | `ACK` | 舵机退出配置模式，恢复 PWM |
| `ACK` | `0x80` | 双向 | `ServoCommAck` | 无 | 命令成功 |
| `NACK` | `0x81` | 双向 | `ServoCommAck` | 无 | 命令失败和错误码 |

## 6. HELLO 数据

结构体：`ServoCommHello`

| 字段 | 类型 | 说明 |
|---|---|---|
| `device_magic` | `uint32_t` | 固定 `0x50325356`，ASCII 含义近似 `V S 2 P` |
| `protocol_version` | `uint16_t` | 当前 `1` |
| `fixed_program_version` | `uint16_t` | 固件固定程序版本 |
| `params_size` | `uint16_t` | `ServoParams` 字节数 |
| `telemetry_size` | `uint16_t` | `ServoTelemetry` 字节数 |
| `capabilities` | `uint32_t` | bit0 读参数，bit1 写参数，bit2 读遥测，bit3 共用 PWM 脚 |

## 7. 参数包

结构体：`ServoParams`

| 字段 | 类型 | 对应调试项 | 说明 |
|---|---|---|---|
| `magic` | `uint32_t` | 参数识别 | 固定 `0x53525650` |
| `version` | `uint16_t` | 参数版本 | 当前 `2` |
| `size` | `uint16_t` | 长度 | 必须等于结构体长度 |
| `pulse_lower_us` | `uint16_t` | Pulse Lower | 输入识别下限 |
| `pulse_center_us` | `uint16_t` | Neutral pulse | 中位脉宽，默认 1500 us |
| `pulse_high_us` | `uint16_t` | Pulse High | 输入识别上限 |
| `adc_min_count` | `uint16_t` | 左端校准 | 左端电位器 ADC |
| `adc_center_count` | `uint16_t` | 中点校准 | 中位电位器 ADC |
| `adc_max_count` | `uint16_t` | 右端校准 | 右端电位器 ADC |
| `neutral_offset_count` | `int16_t` | Neutral | 中位偏移 |
| `left_range_count` | `int16_t` | Left Range | 左行程调整 |
| `right_range_count` | `int16_t` | Right Range | 右行程调整 |
| `deadband_us` | `uint16_t` | Dead Band | 脉宽死区 |
| `deadband_count_min` | `uint16_t` | Dead Band | 最小 ADC 死区 |
| `stretcher_q8` | `uint16_t` | Stretcher | 误差增益，256 = 1.0 |
| `max_duty` | `uint16_t` | Max.Duty | 最大占空比，满量程 1000 |
| `boost_duty` | `uint16_t` | Boost | 起动占空比 |
| `brake_band_count` | `uint16_t` | 到位保持 | 小误差刹车范围 |
| `drive_frequency_hz` | `uint16_t` | Drive Frequency | 电机驱动频率 |
| `forward_speed_q8` | `uint16_t` | Speed Adj | 正转速度比例，256 = 1.0 |
| `reverse_speed_q8` | `uint16_t` | Speed Adj | 反转速度比例，256 = 1.0 |
| `soft_start_step_count` | `uint16_t` | Soft Start | 软启动目标步进 |
| `stall_duty_threshold` | `uint16_t` | OL Protect | 堵转判定占空比 |
| `stall_min_move_count` | `uint16_t` | OL Protect | 堵转判定最小 ADC 变化 |
| `stall_time_ms` | `uint16_t` | OL Protect | 堵转持续时间 |
| `stall_recovery_ms` | `uint16_t` | OL Protect | 堵转恢复等待 |
| `input_timeout_ms` | `uint16_t` | Lose PPM | 失信号时间 |
| `model_id` | `uint16_t` | 型号选择 | 区分塑胶齿、金属齿或客户型号 |
| `profile_id` | `uint16_t` | 参数模板 | 区分快速、标准、安静、高保持力等模板 |
| `startup_delay_ms` | `uint16_t` | 上电防乱跑 | 上电后先等待电源、PWM、ADC 稳定 |
| `startup_input_stable_count` | `uint16_t` | 上电防乱跑 | 连续多少个稳定 PWM 样本后允许闭环 |
| `startup_input_stable_us` | `uint16_t` | 上电防乱跑 | PWM 稳定判定允许的 us 变化 |
| `startup_adc_stable_count` | `uint16_t` | 上电防乱跑 | 连续多少个稳定 ADC 样本后允许闭环 |
| `startup_adc_stable_band_count` | `uint16_t` | 上电防乱跑 | ADC 稳定判定允许的 count 变化 |
| `startup_step_count` | `uint16_t` | 上电软启动 | 第一次追目标时的目标 ADC 步进 |
| `adc_sample_count` | `uint16_t` | ADC 抗干扰 | 每次反馈读取的平均采样次数 |
| `adc_filter_shift` | `uint16_t` | ADC 抗干扰 | IIR 滤波强度，越大越稳但越慢 |
| `adc_jump_limit_count` | `uint16_t` | ADC 抗干扰 | 单次滤波前允许的最大 ADC 跳变，0 为关闭 |
| `adc_noise_band_count` | `uint16_t` | ADC 抗干扰 | 噪声评估保留项，用于调试显示和后续算法 |
| `hold_mode` | `uint16_t` | 到位保持 | 0 滑行，1 持续刹车，2 短刹车后滑行 |
| `hold_exit_band_count` | `uint16_t` | 到位保持 | HOLD 状态退出阈值，形成滞回 |
| `hold_brake_time_ms` | `uint16_t` | 到位保持 | 短刹车保持时间 |
| `hold_settle_ms` | `uint16_t` | 到位保持 | 到位稳定时间保留项，用于后续判定 |
| `close_error_count` | `uint16_t` | 近目标控制 | 小误差区上限 |
| `close_stretcher_q8` | `uint16_t` | 近目标控制 | 小误差区增益 |
| `close_boost_duty` | `uint16_t` | 近目标控制 | 小误差区起动补偿 |
| `approach_error_count` | `uint16_t` | 末端控制 | 接近目标区上限 |
| `approach_stretcher_q8` | `uint16_t` | 末端控制 | 接近目标区增益 |
| `approach_boost_duty` | `uint16_t` | 末端控制 | 接近目标区起动补偿 |
| `reverse_pause_ms` | `uint16_t` | 反向保护 | 正反转切换前暂停时间 |
| `reverse_brake_ms` | `uint16_t` | 反向保护 | 暂停前段短刹车时间 |
| `vdd_nominal_mv` | `uint16_t` | 电源监测 | 标称 MCU/ADC 供电电压 |
| `vdd_warn_drop_mv` | `uint16_t` | 电源监测 | 低于标称多少 mV 视为明显跌落 |
| `vdd_noise_band_mv` | `uint16_t` | 电源监测 | VDD 抖动评估阈值 |
| `vdd_sample_interval_ms` | `uint16_t` | 电源监测 | 读取内部 VREFINT 估算 VDD 的间隔 |
| `flags` | `uint32_t` | 功能选择 | 反向、保护、软启动、失信号等 |
| `crc32` | `uint32_t` | 参数 CRC | `crc32` 字段按 0 参与计算 |

写参数流程：

| 步骤 | 动作 | 失败处理 |
|---:|---|---|
| 1 | 配置器 `READ_PARAMS` 保存原参数 | 失败则停止 |
| 2 | 配置器修改完整 `ServoParams` 并计算 CRC32 | CRC 错误禁止发送 |
| 3 | 配置器 `WRITE_PARAMS` 发送完整结构体 | 收到 NACK 则提示错误 |
| 4 | 舵机校验版本、长度、范围和 CRC32 | 不合法不擦写 Flash |
| 5 | 舵机擦写最后 128B Flash 参数页 | Flash 失败返回 NACK |
| 6 | 配置器再次 `READ_PARAMS` | 逐字节比对确认写入 |

## 8. 功能标志

| 标志 | bit | 对应功能 | 说明 |
|---|---:|---|---|
| `INVERTER` | 0 | Inverter | 舵机方向反转 |
| `OL_PROTECT` | 1 | OL Protect | 堵转/过载保护 |
| `PWM_100_PERCENT` | 2 | 100% PWM | 满占空比策略预留 |
| `NARROW_BAND` | 3 | Narrow Band | 窄频/窄行程模式预留 |
| `SOFT_START` | 4 | Soft Start | 软启动 |
| `LOSE_PPM_ENABLE` | 5 | Lose PPM Enable | 失信号回中位 |
| `LOSE_PPM_LOCK` | 6 | Lose PPM Lock | 失信号锁定 |

## 9. 遥测包

结构体：`ServoTelemetry`

| 字段 | 类型 | 来源 | 说明 |
|---|---|---|---|
| `uptime_ms` | `uint32_t` | SysTick | 上电运行时间 |
| `input_us` | `uint16_t` | PA4 PWM | 当前识别命令脉宽 |
| `input_period_us` | `uint32_t` | PA4 PWM | 输入周期 |
| `target_adc` | `uint16_t` | 控制算法 | 当前目标 ADC |
| `feedback_adc_raw` | `uint16_t` | PA3 ADC | 原始/平均 ADC |
| `feedback_adc` | `uint16_t` | PA3 ADC | 滤波后 ADC |
| `error_adc` | `int16_t` | 控制算法 | 目标与反馈差值 |
| `motor_duty` | `int16_t` | H 桥输出 | 正负代表方向 |
| `servo_state` | `uint8_t` | 控制状态 | 0 无信号，1 保持，2 驱动，3 堵转保护 |
| `hbridge_state` | `uint8_t` | H 桥 | 0 滑行，1 刹车，2 正转，3 反转 |
| `input_valid` | `uint8_t` | PWM 输入 | 1 有效，0 失信号 |
| `params_valid` | `uint8_t` | 参数层 | 1 当前参数 CRC 有效 |
| `params_crc32` | `uint32_t` | 参数层 | 当前参数 CRC |
| `adc_min_count` | `uint16_t` | 参数层 | 左端校准值 |
| `adc_center_count` | `uint16_t` | 参数层 | 中位校准值 |
| `adc_max_count` | `uint16_t` | 参数层 | 右端校准值 |
| `input_age_ms` | `uint16_t` | PWM 输入 | 距离最后有效输入边沿时间 |
| `vrefint_raw` | `uint16_t` | ADC 内部通道 | 内部 VREFINT 原始 ADC，用于判断 VDD 变化 |
| `vdd_mv` | `uint16_t` | ADC 内部通道 | 由 VREFINT 估算的 MCU/ADC 供电电压 |
| `reserved` | `uint16_t` | 预留 | 后续可扩展电压、电流、温度 |

LCDM 或上位机建议显示：

| 页面 | 显示项目 |
|---|---|
| 输入页 | `input_us`、`input_period_us`、`input_valid` |
| 位置页 | `target_adc`、`feedback_adc_raw`、`feedback_adc`、`error_adc` |
| 电源页 | `vrefint_raw`、`vdd_mv`、VDD 跌落/抖动提示 |
| 输出页 | `motor_duty`、`hbridge_state`、`servo_state` |
| 参数页 | `params_crc32`、左右中 ADC 校准、Dead Band、Max Duty、Boost |

## 10. 错误码

| 错误码 | 值 | 说明 |
|---|---:|---|
| `OK` | `0x00` | 成功 |
| `BAD_FRAME` | `0x01` | 帧头、版本、长度或帧尾错误 |
| `BAD_CRC` | `0x02` | 通讯 CRC 错误，预留 |
| `BAD_COMMAND` | `0x03` | 不支持命令 |
| `BAD_LENGTH` | `0x04` | Payload 长度不正确 |
| `BAD_PARAMS` | `0x05` | 参数结构体校验失败 |
| `FLASH_ERROR` | `0x06` | Flash 擦写失败 |

## 11. 后续实现任务

| 任务 | 状态 | 说明 |
|---|---|---|
| 参数结构体和 CRC32 | 已实现 | `servo_params.c/.h` |
| 帧构造、解析、命令处理和 CRC16 | 已实现 | `servo_comm.c/.h` |
| PA4 单线 bit-bang 收发 | 待实现 | 需结合配置器硬件确认开漏/串阻和时序 |
| 配置模式握手检测 | 待实现 | 在 `pwm_input.c` 或独立物理层中检测 Break + Sync |
| 上位机/LCDM 菜单 | 待实现 | 按本文件结构体和命令表开发 |
| 生产校准流程 | 待实现 | 写入左右中 ADC、参数模板和读回校验 |
